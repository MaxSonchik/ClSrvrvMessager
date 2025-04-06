from kubernetes import client, config
from prometheus_api_client import PrometheusConnect
import joblib
import numpy as np
import pandas as pd
import time
import os
import sys 

# --- Конфигурация ---
MODEL_DIR = os.getenv("MODEL_DIR", "/models") # Путь к моделям из переменной окружения или по умолчанию
MODEL_PROPHET_PATH = os.path.join(MODEL_DIR, "prophet_cpu.pkl") # Модель для CPU
MODEL_LR_PATH = os.path.join(MODEL_DIR, "lr_scaler.pkl")       # Модель для определения реплик
PROMETHEUS_URL = os.getenv("PROMETHEUS_URL", "http://prometheus-kube-prometheus-prometheus.monitoring.svc:9090") # URL Prometheus в кластере (проверьте имя сервиса!)
APP_NAME = os.getenv("APP_NAME", "messenger-app") # Имя масштабируемого Deployment
APP_NAMESPACE = os.getenv("APP_NAMESPACE", "messenger-app") # Неймспейс приложения (если отличается от default)
MIN_REPLICAS = int(os.getenv("MIN_REPLICAS", 1))
MAX_REPLICAS = int(os.getenv("MAX_REPLICAS", 25)) # Ваш максимум
SLEEP_INTERVAL = int(os.getenv("SLEEP_INTERVAL", 300)) # Интервал проверки (5 минут)
PROPHET_FREQ = os.getenv("PROPHET_FREQ", "5T") # Частота для Prophet (5 минут) - должна соответствовать шагу сбора метрик Prometheus

# --- Инициализация ---
print(f"Initializing ML Scaler...")
print(f"Model directory: {MODEL_DIR}")
print(f"Prometheus URL: {PROMETHEUS_URL}")
print(f"Target Deployment: {APP_NAMESPACE}/{APP_NAME}")
print(f"Scaling range: {MIN_REPLICAS}-{MAX_REPLICAS} replicas")
print(f"Check interval: {SLEEP_INTERVAL} seconds")

try:
    # Пытаемся прочитать переменную окружения и преобразовать в float
    TRAINING_DAYS_STR = os.getenv("TRAINING_DAYS", "7") # Получаем строку
    TRAINING_DAYS = float(TRAINING_DAYS_STR) # Преобразуем в float
    if TRAINING_DAYS <= 0:
        print(f"Warning: TRAINING_DAYS value ({TRAINING_DAYS}) is not positive. Using default 7 days.", file=sys.stderr)
        TRAINING_DAYS = 7.0 # Возвращаемся к дефолту, если значение некорректно
except ValueError:
    print(f"Warning: Invalid value for TRAINING_DAYS ('{TRAINING_DAYS_STR}'). Using default 7 days.", file=sys.stderr)
    TRAINING_DAYS = 7.0 # Используем float и здесь
    
# Загрузка конфигурации Kubernetes (внутри кластера)
try:
    config.load_incluster_config()
    print("Loaded in-cluster K8s config.")
except config.ConfigException:
    try:
        config.load_kube_config() # Для локального запуска вне кластера
        print("Loaded local K8s config.")
    except config.ConfigException:
        print("Could not load any K8s config.", file=sys.stderr)
        exit(1)

k8s_api = client.AppsV1Api()
prom = PrometheusConnect(url=PROMETHEUS_URL, disable_ssl=True) # disable_ssl т.к. внутри кластера обычно http

# --- Функции ---

def load_models():
    """Загружает модели с диска."""
    try:
        model_prophet = joblib.load(MODEL_PROPHET_PATH)
        model_lr = joblib.load(MODEL_LR_PATH)
        print("Models loaded successfully.")
        return model_prophet, model_lr
    except FileNotFoundError:
        print(f"Warning: Models not found at {MODEL_DIR}. Waiting for initial training.", file=sys.stderr)
        return None, None
    except Exception as e:
        print(f"Error loading models: {e}", file=sys.stderr)
        return None, None

def get_current_metrics():
    """Получает текущие метрики из Prometheus."""
    try:
        # Важно: Имя контейнера должно совпадать с именем в Deployment! Обычно совпадает с app name.
        # Запрос может потребовать настройки под вашу среду. '{}' может включать pod label selector.
        # Используем rate для CPU
        cpu_query = f"sum(rate(container_cpu_usage_seconds_total{{namespace='{APP_NAMESPACE}', container='{APP_NAME}'}}[5m]))"
        # Используем актуальное имя метрики соединений
        connections_query = f"sum(messenger_active_connections{{namespace='{APP_NAMESPACE}', app='{APP_NAME}'}})" # Уточните селекторы app label

        cpu_data = prom.custom_query(query=cpu_query)
        connections_data = prom.custom_query(query=connections_query)

        current_cpu = float(cpu_data[0]['value'][1]) if cpu_data else 0.0
        current_connections = float(connections_data[0]['value'][1]) if connections_data else 0.0

        print(f"Current metrics: CPU usage rate: {current_cpu:.4f}, Active connections: {current_connections}")
        return current_cpu, current_connections

    except Exception as e:
        print(f"Error fetching metrics from Prometheus: {e}", file=sys.stderr)
        return None, None

def predict_cpu(model_prophet):
    """Прогнозирует CPU на следующий период."""
    try:
        # Создаем DataFrame для прогноза на 1 шаг вперед
        future = model_prophet.make_future_dataframe(periods=1, freq=PROPHET_FREQ, include_history=False)
        forecast = model_prophet.predict(future)
        predicted_cpu = forecast["yhat"].iloc[-1]
        # Ограничим прогноз снизу нулем
        predicted_cpu = max(0, predicted_cpu)
        print(f"Predicted CPU usage (yhat): {predicted_cpu:.4f}")
        return predicted_cpu
    except Exception as e:
        print(f"Error predicting CPU: {e}", file=sys.stderr)
        return None

def predict_replicas(model_lr, predicted_cpu, current_connections):
    """Определяет необходимое количество реплик."""
    try:
        # Создаем DataFrame с фичами для модели scaler'а
        # Внимание: порядок фичей должен совпадать с тем, что было при обучении!
        # Предполагаем порядок: ['cpu_usage', 'active_connections']
        features = pd.DataFrame([[predicted_cpu, current_connections]], columns=['cpu_usage', 'active_connections'])

        # Применяем scaler (если он был частью пайплайна при обучении)
        # Если scaler обучался отдельно, его тоже нужно загрузить и применить:
        # features_scaled = scaler.transform(features)
        # replicas_float = model_lr.predict(features_scaled)[0]

        # Если модель - это Pipeline (Scaler + Regressor)
        replicas_float = model_lr.predict(features)[0]

        # Округляем и ограничиваем результат
        replicas = int(np.round(replicas_float))
        replicas = max(MIN_REPLICAS, min(replicas, MAX_REPLICAS))
        print(f"Predicted replicas (float): {replicas_float:.2f}, Rounded & Clamped: {replicas}")
        return replicas
    except Exception as e:
        print(f"Error predicting replicas: {e}", file=sys.stderr)
        return MIN_REPLICAS # Возвращаем минимум в случае ошибки

def scale_deployment(target_replicas):
    """Масштабирует Deployment до нужного количества реплик."""
    try:
        # Получаем текущее состояние scale
        current_scale = k8s_api.read_namespaced_deployment_scale(name=APP_NAME, namespace=APP_NAMESPACE)
        current_replicas = current_scale.spec.replicas

        if current_replicas == target_replicas:
            print(f"No scaling needed. Current replicas: {current_replicas}")
            return

        print(f"Scaling deployment '{APP_NAMESPACE}/{APP_NAME}' from {current_replicas} to {target_replicas} replicas...")
        body = {"spec": {"replicas": target_replicas}}
        k8s_api.patch_namespaced_deployment_scale(
            name=APP_NAME,
            namespace=APP_NAMESPACE,
            body=body
        )
        print(f"Scaling request sent successfully.")

    except client.ApiException as e:
        print(f"Kubernetes API error scaling deployment: {e}", file=sys.stderr)
    except Exception as e:
        print(f"Error scaling deployment: {e}", file=sys.stderr)

# --- Основной Цикл ---
model_prophet, model_lr = None, None

while True:
    if model_prophet is None or model_lr is None:
        print("Attempting to load models...")
        model_prophet, model_lr = load_models()

    if model_prophet and model_lr:
        print("\n--- Running Scaling Check ---")
        current_cpu, current_connections = get_current_metrics()

        if current_cpu is not None and current_connections is not None:
            predicted_cpu = predict_cpu(model_prophet) # Используем только Prophet для прогноза CPU

            if predicted_cpu is not None:
                # Используем ПРОГНОЗ CPU и ТЕКУЩЕЕ число соединений для расчета реплик
                # Альтернатива: обучить Prophet и для соединений, если они имеют тренд/сезонность.
                target_replicas = predict_replicas(model_lr, predicted_cpu, current_connections)
                scale_deployment(target_replicas)
            else:
                print("Skipping replica prediction due to CPU prediction error.")
        else:
            print("Skipping scaling check due to metric fetching error.")
    else:
        print("Models not available, skipping scaling check.")

    print(f"--- Sleeping for {SLEEP_INTERVAL} seconds ---")
    time.sleep(SLEEP_INTERVAL)