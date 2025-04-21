from kubernetes import client, config
from prometheus_api_client import PrometheusConnect
import joblib
import numpy as np
import pandas as pd
import time
import os
import sys
import logging # Используем стандартный модуль логирования

# --- Настройка Логирования ---
logging.basicConfig(level=logging.INFO,
                    format='%(asctime)s - %(levelname)s - %(message)s',
                    handlers=[logging.StreamHandler(sys.stdout)]) # Вывод в stdout для Docker/K8s логов

# --- Конфигурация ---
MODEL_DIR = os.getenv("MODEL_DIR", "/models")
MODEL_PROPHET_PATH = os.path.join(MODEL_DIR, "prophet_cpu.pkl")
MODEL_LR_PATH = os.path.join(MODEL_DIR, "lr_scaler.pkl")
PROMETHEUS_URL = os.getenv("PROMETHEUS_URL", "http://prometheus-kube-prometheus-prometheus.monitoring.svc:9090")
APP_NAME = os.getenv("APP_NAME", "messenger-app")
APP_NAMESPACE = os.getenv("APP_NAMESPACE", "messenger-app")
# Используем try-except для более надежного парсинга int из env
try:
    MIN_REPLICAS = int(os.getenv("MIN_REPLICAS", "1"))
except ValueError:
    logging.warning(f"Invalid MIN_REPLICAS value. Using default: 1")
    MIN_REPLICAS = 1
try:
    MAX_REPLICAS = int(os.getenv("MAX_REPLICAS", "25"))
except ValueError:
    logging.warning(f"Invalid MAX_REPLICAS value. Using default: 25")
    MAX_REPLICAS = 25
try:
    SLEEP_INTERVAL = int(os.getenv("SLEEP_INTERVAL", "300")) # 5 минут
except ValueError:
    logging.warning(f"Invalid SLEEP_INTERVAL value. Using default: 300")
    SLEEP_INTERVAL = 300
PROPHET_FREQ = os.getenv("PROPHET_FREQ", "5T")

# --- Инициализация ---
logging.info("Initializing ML Scaler...")
logging.info(f"Model directory: {MODEL_DIR}")
logging.info(f"Prometheus URL: {PROMETHEUS_URL}")
logging.info(f"Target Deployment: {APP_NAMESPACE}/{APP_NAME}")
logging.info(f"Scaling range: {MIN_REPLICAS}-{MAX_REPLICAS} replicas")
logging.info(f"Check interval: {SLEEP_INTERVAL} seconds")

# Загрузка конфигурации Kubernetes
try:
    config.load_incluster_config()
    logging.info("Loaded in-cluster K8s config.")
except config.ConfigException:
    try:
        config.load_kube_config()
        logging.info("Loaded local K8s config (for testing outside cluster).")
    except config.ConfigException as e:
        logging.error(f"Could not load any Kubernetes config: {e}", exc_info=True)
        sys.exit(1) # Критическая ошибка - выходим

k8s_api = client.AppsV1Api()
try:
    prom = PrometheusConnect(url=PROMETHEUS_URL, disable_ssl=True)
    logging.info("Prometheus connection initialized.")
except Exception as e:
    logging.error(f"Failed to initialize Prometheus connection: {e}", exc_info=True)
    # Можно решить выйти или продолжить попытки позже
    sys.exit(1)

# --- Функции ---

def load_models_with_retry(retry_interval=30):
    """Пытается загрузить модели, повторяя попытки."""
    while True:
        try:
            logging.info(f"Attempting to load models from {MODEL_DIR}...")
            # Проверяем существование файлов перед загрузкой
            if not os.path.exists(MODEL_PROPHET_PATH):
                raise FileNotFoundError(f"Prophet model not found: {MODEL_PROPHET_PATH}")
            if not os.path.exists(MODEL_LR_PATH):
                 raise FileNotFoundError(f"Scaler model not found: {MODEL_LR_PATH}")

            model_prophet = joblib.load(MODEL_PROPHET_PATH)
            model_lr = joblib.load(MODEL_LR_PATH)
            logging.info("Models loaded successfully.")
            return model_prophet, model_lr
        except FileNotFoundError as e:
            logging.warning(f"{e}. Models not yet available. Waiting for initial training. Retrying in {retry_interval}s...")
        except Exception as e:
            logging.error(f"Error loading models: {e}. Retrying in {retry_interval}s...", exc_info=True)
        time.sleep(retry_interval)

def get_current_metrics():
    """Получает текущие метрики из Prometheus."""
    logging.debug("Fetching current metrics from Prometheus...")
    try:
        # Определяем запросы
        cpu_query = f"sum(rate(container_cpu_usage_seconds_total{{namespace='{APP_NAMESPACE}', container='{APP_NAME}'}}[5m])) by (namespace)"
        connections_query = f"sum(messenger_active_connections{{namespace='{APP_NAMESPACE}', app='{APP_NAME}'}}) by (namespace)"

        cpu_data = prom.custom_query(query=cpu_query)
        connections_data = prom.custom_query(query=connections_query)

        # Извлекаем значения, обрабатывая пустой результат
        current_cpu = float(cpu_data[0]['value'][1]) if cpu_data else 0.0
        # Ограничиваем CPU снизу нулем на всякий случай
        current_cpu = max(0.0, current_cpu)

        current_connections = float(connections_data[0]['value'][1]) if connections_data else 0.0
        current_connections = max(0.0, current_connections) # Соединения не могут быть отрицательными

        logging.info(f"Current metrics: CPU rate: {current_cpu:.4f}, Connections: {current_connections:.0f}")
        return current_cpu, current_connections

    except Exception as e:
        logging.error(f"Error fetching metrics from Prometheus: {e}", exc_info=True)
        return None, None # Возвращаем None при ошибке

def predict_cpu(model_prophet):
    """Прогнозирует CPU на следующий период."""
    logging.debug("Predicting next CPU usage...")
    try:
        # Создаем DataFrame для прогноза на 1 шаг вперед (частота из PROPHET_FREQ)
        # include_history=False важно, чтобы не пересчитывать историю
        future = model_prophet.make_future_dataframe(periods=1, freq=PROPHET_FREQ, include_history=False)
        forecast = model_prophet.predict(future)
        # Извлекаем предсказанное значение ('yhat') для последнего шага
        predicted_cpu = forecast["yhat"].iloc[-1]
        # Ограничиваем прогноз снизу нулем
        predicted_cpu = max(0, predicted_cpu)
        logging.info(f"Predicted CPU usage (yhat): {predicted_cpu:.4f}")
        return predicted_cpu
    except Exception as e:
        logging.error(f"Error predicting CPU with Prophet: {e}", exc_info=True)
        return None # Возвращаем None при ошибке

def predict_replicas(model_lr, predicted_cpu, current_connections):
    """Определяет необходимое количество реплик на основе прогноза CPU и текущих соединений."""
    logging.debug(f"Predicting replicas based on predicted_cpu={predicted_cpu:.4f}, current_connections={current_connections:.0f}")
    # ВАЖНОЕ ЗАМЕЧАНИЕ: Использование предсказанного CPU и ТЕКУЩИХ соединений - это гибридный подход.
    # Для чисто предиктивного масштабирования нужно было бы предсказывать и соединения.
    try:
        # Создаем DataFrame с фичами для модели scaler'а
        # Убедитесь, что названия колонок ('cpu_usage', 'active_connections')
        # и их порядок ТОЧНО совпадают с теми, что использовались при ОБУЧЕНИИ модели model_lr!
        features = pd.DataFrame([[predicted_cpu, current_connections]], columns=['cpu_usage', 'active_connections'])

        # Используем обученный пайплайн (предполагается, что model_lr - это Pipeline)
        replicas_float = model_lr.predict(features)[0]

        # Округляем и ограничиваем результат заданными MIN/MAX
        replicas_rounded = int(np.round(replicas_float))
        target_replicas = max(MIN_REPLICAS, min(replicas_rounded, MAX_REPLICAS))

        logging.info(f"Predicted replicas (raw): {replicas_float:.2f}, Rounded: {replicas_rounded}, Clamped Target: {target_replicas}")
        return target_replicas
    except Exception as e:
        logging.error(f"Error predicting replicas: {e}", exc_info=True)
        logging.warning(f"Falling back to minimum replicas: {MIN_REPLICAS}")
        return MIN_REPLICAS # Возвращаем минимум в случае ошибки

def scale_deployment(target_replicas):
    """Масштабирует Deployment до target_replicas."""
    try:
        logging.debug(f"Reading current scale for deployment '{APP_NAMESPACE}/{APP_NAME}'...")
        current_scale = k8s_api.read_namespaced_deployment_scale(name=APP_NAME, namespace=APP_NAMESPACE)
        current_replicas = current_scale.spec.replicas
        logging.info(f"Current replica count: {current_replicas}")

        if current_replicas == target_replicas:
            logging.info(f"Target replicas ({target_replicas}) match current count. No scaling needed.")
            return True # Масштабирование не требовалось

        logging.info(f"Scaling deployment '{APP_NAMESPACE}/{APP_NAME}' from {current_replicas} to {target_replicas} replicas...")
        # Создаем тело запроса для patch
        body = {"spec": {"replicas": target_replicas}}
        # Выполняем patch запрос
        k8s_api.patch_namespaced_deployment_scale(
            name=APP_NAME,
            namespace=APP_NAMESPACE,
            body=body
        )
        logging.info(f"Scaling request sent successfully to {target_replicas} replicas.")
        return True # Масштабирование выполнено

    except client.ApiException as e:
        # Обрабатываем ошибки Kubernetes API
        logging.error(f"Kubernetes API error scaling deployment: {e.status} {e.reason} - {e.body}", exc_info=True)
        return False # Ошибка масштабирования
    except Exception as e:
        # Обрабатываем другие возможные ошибки
        logging.error(f"Unexpected error scaling deployment: {e}", exc_info=True)
        return False # Ошибка масштабирования

# --- Основной Цикл ---
logging.info("Waiting for initial model load...")
# Пытаемся загрузить модели при старте, ждем, пока они не появятся
model_prophet, model_lr = load_models_with_retry(retry_interval=60) # Повтор каждые 60 сек

if not model_prophet or not model_lr:
     logging.error("Failed to load models after multiple retries. Exiting.")
     sys.exit(1)

logging.info("Models loaded. Starting main scaling loop.")

# ================= КЛЮЧЕВОЙ БЛОК =================
while True:
    logging.info("--- Running Scaling Check ---")

    # 1. Получаем текущие метрики
    current_cpu, current_connections = get_current_metrics()

    if current_cpu is not None and current_connections is not None:
        # 2. Предсказываем будущую нагрузку CPU
        predicted_cpu = predict_cpu(model_prophet)

        if predicted_cpu is not None:
            # 3. Предсказываем необходимое количество реплик
            target_replicas = predict_replicas(model_lr, predicted_cpu, current_connections)

            # 4. Выполняем масштабирование
            scale_successful = scale_deployment(target_replicas)
            if not scale_successful:
                 logging.warning("Scaling operation failed. Check previous errors.")
        else:
            logging.warning("Skipping replica prediction due to CPU prediction error.")
    else:
        logging.warning("Skipping scaling check due to metric fetching error.")

    # --- Пауза перед следующей проверкой ---
    logging.info(f"--- Sleeping for {SLEEP_INTERVAL} seconds ---")
    time.sleep(SLEEP_INTERVAL)
