# retrain_model.py (Исправленная и дополненная версия)

from prophet import Prophet
from sklearn.linear_model import ElasticNet # Или LinearRegression
from sklearn.preprocessing import StandardScaler
from sklearn.pipeline import Pipeline
# from sklearn.model_selection import train_test_split # Оставил закомментированным
import pandas as pd
import joblib
import numpy as np
from prometheus_api_client import PrometheusConnect # Убрал неиспользуемый MetricsList
from datetime import datetime, timedelta
import os
import sys # Убедимся, что sys импортирован для stderr

# --- Конфигурация ---
MODEL_DIR = os.getenv("MODEL_DIR", "/models")
MODEL_PROPHET_PATH = os.path.join(MODEL_DIR, "prophet_cpu.pkl")
MODEL_LR_PATH = os.path.join(MODEL_DIR, "lr_scaler.pkl")
PROMETHEUS_URL = os.getenv("PROMETHEUS_URL", "http://prometheus-kube-prometheus-prometheus.monitoring.svc:9090")
APP_NAME = os.getenv("APP_NAME", "messenger-app")
APP_NAMESPACE = os.getenv("APP_NAMESPACE", "messenger-app")

# --- Обработка TRAINING_DAYS с использованием float() ---
try:
    TRAINING_DAYS_STR = os.getenv("TRAINING_DAYS", "7") # Получаем строку (дефолт 7 дней)
    TRAINING_DAYS = float(TRAINING_DAYS_STR) # Пытаемся преобразовать в float
    if TRAINING_DAYS <= 0:
        print(f"Warning: TRAINING_DAYS value '{TRAINING_DAYS_STR}' is not positive. Using default 7.0 days.", file=sys.stderr)
        TRAINING_DAYS = 7.0 # Возврат к дефолту, если <= 0
except ValueError:
    # Если преобразование в float не удалось
    print(f"Warning: Invalid non-numeric value for TRAINING_DAYS ('{TRAINING_DAYS_STR}'). Using default 7.0 days.", file=sys.stderr)
    TRAINING_DAYS = 7.0 # Используем float дефолт

PROMETHEUS_STEP = os.getenv("PROMETHEUS_STEP", "1m") # Шаг данных в Prometheus
# Параметры для создания 'replicas_calculated' (можно вынести в env)
# Формула: replicas = CPU_USAGE / CPU_PER_REPLICA + CONNECTIONS / CONN_PER_REPLICA
# Подберите эти значения экспериментально!
CPU_PER_REPLICA = float(os.getenv("CPU_PER_REPLICA", "0.8")) # Пример: 0.8 ядра CPU на реплику
CONN_PER_REPLICA = float(os.getenv("CONN_PER_REPLICA", "50"))  # Пример: 50 соединений на реплику
MIN_REPLICAS_CALC = int(os.getenv("MIN_REPLICAS_CALC", 1))   # Мин. расчетное число реплик
MAX_REPLICAS_CALC = int(os.getenv("MAX_REPLICAS_CALC", 25))  # Макс. расчетное число реплик

# --- Инициализация ---
print(f"--- Starting Model Retraining ---")
print(f"Model directory: {MODEL_DIR}")
print(f"Prometheus URL: {PROMETHEUS_URL}")
print(f"Target application: {APP_NAMESPACE}/{APP_NAME}")
print(f"Training data window: {TRAINING_DAYS} days")
print(f"Prometheus step: {PROMETHEUS_STEP}")
print(f"Formula params: CPU_PER_REPLICA={CPU_PER_REPLICA}, CONN_PER_REPLICA={CONN_PER_REPLICA}")
print(f"Formula limits: MIN_REPLICAS={MIN_REPLICAS_CALC}, MAX_REPLICAS={MAX_REPLICAS_CALC}")

# Создаем директорию для моделей, если ее нет
try:
    os.makedirs(MODEL_DIR, exist_ok=True)
    print(f"Model directory '{MODEL_DIR}' ensured.")
except OSError as e:
     print(f"Error creating model directory '{MODEL_DIR}': {e}. Exiting.", file=sys.stderr)
     sys.exit(1)


# Инициализация клиента Prometheus
try:
    prom = PrometheusConnect(url=PROMETHEUS_URL, disable_ssl=True)
    print("Prometheus client initialized.")
except Exception as e:
    print(f"Error initializing Prometheus client at {PROMETHEUS_URL}: {e}. Exiting.", file=sys.stderr)
    sys.exit(1)

# --- Функции ---

def fetch_prometheus_data(metric_query, days, step):
    """Запрашивает исторические данные из Prometheus."""
    end_time = datetime.now()
    start_time = end_time - timedelta(days=days) # timedelta корректно работает с float
    print(f"\nFetching data for query: {metric_query}")
    print(f"Time range: {start_time.isoformat()} to {end_time.isoformat()} ({days:.3f} days), Step: {step}")
    try:
        metric_data = prom.custom_query_range(
            query=metric_query,
            start_time=start_time,
            end_time=end_time,
            step=step,
        )
        if not metric_data:
            print(f"Warning: No data returned from Prometheus for query.", file=sys.stderr)
            return pd.DataFrame() # Возвращаем пустой DataFrame

        # Преобразуем в DataFrame
        # Prometheus может вернуть несколько временных рядов, если селекторы не уникальны.
        # Для простоты берем первый (предполагаем, что sum() дает один ряд).
        if len(metric_data) > 1:
            print(f"Warning: Prometheus returned {len(metric_data)} metrics for the query. Using the first one.", file=sys.stderr)

        values = metric_data[0]['values']
        if not values:
             print(f"Warning: Prometheus returned metric data but with empty values list.", file=sys.stderr)
             return pd.DataFrame()

        df = pd.DataFrame(values, columns=['timestamp', 'value'])
        df['timestamp'] = pd.to_datetime(df['timestamp'], unit='s') # Преобразуем UNIX timestamp
        df['value'] = pd.to_numeric(df['value']) # Преобразуем значение в число
        df = df.set_index('timestamp') # Делаем timestamp индексом
        print(f"Fetched {len(df)} data points successfully.")
        return df

    except Exception as e:
        print(f"ERROR fetching data from Prometheus for query '{metric_query}': {e}", file=sys.stderr)
        # Возвращаем пустой DataFrame при любой ошибке запроса
        return pd.DataFrame()

# --- Основной скрипт обучения ---

# 1. Загрузка данных
print("\n--- 1. Fetching Historical Data ---")
# Запросы к Prometheus (убедитесь, что селекторы верны!)
cpu_query = f"sum(rate(container_cpu_usage_seconds_total{{namespace='{APP_NAMESPACE}', container='{APP_NAME}'}}[5m])) by (namespace, pod)" # Добавил by, чтобы можно было проверить ряды
connections_query = f"sum(messenger_active_connections{{namespace='{APP_NAMESPACE}', app='{APP_NAME}'}}) by (namespace, pod)" # Добавил by

df_cpu = fetch_prometheus_data(cpu_query, TRAINING_DAYS, PROMETHEUS_STEP)
df_connections = fetch_prometheus_data(connections_query, TRAINING_DAYS, PROMETHEUS_STEP)

# Проверяем, получены ли данные
if df_cpu.empty or df_connections.empty:
    print("\nError: Could not fetch sufficient data for one or both metrics from Prometheus. Cannot proceed with training. Exiting.", file=sys.stderr)
    sys.exit(1) # Завершаем скрипт, если данных нет

# 2. Объединение и подготовка данных
print("\n--- 2. Processing Data ---")
df_cpu = df_cpu.rename(columns={'value': 'cpu_usage'})
df_connections = df_connections.rename(columns={'value': 'active_connections'})

# Объединяем по времени, используя внешний join, чтобы не потерять точки
# resample('T') - округляем до ближайшей минуты для лучшего совпадения индексов перед merge
# tolerance - задаем допустимое расхождение во времени при объединении
df_merged = pd.merge_asof(df_cpu.sort_index(), df_connections.sort_index(),
                          left_index=True, right_index=True,
                          direction='nearest', tolerance=pd.Timedelta(PROMETHEUS_STEP)*2) # Ищем ближайшее значение в пределах 2х шагов

# df_merged = pd.merge(df_cpu, df_connections, left_index=True, right_index=True, how='outer')
# Заполняем пропуски (если есть) после объединения
df_merged = df_merged.ffill().bfill()
df_merged = df_merged.dropna() # Удаляем строки, где все еще есть NaN

if df_merged.empty:
    print("Error: Dataframe is empty after merging and cleaning Prometheus data. Exiting.", file=sys.stderr)
    sys.exit(1)

df_merged = df_merged.reset_index() # timestamp снова становится колонкой
print(f"Combined dataframe shape after processing: {df_merged.shape}")
if not df_merged.empty:
    print("Sample data after processing:\n", df_merged.head())

# 3. Обучение Prophet для прогнозирования CPU
print("\n--- 3. Training Prophet Model (CPU Prediction) ---")
# Готовим данные для Prophet: колонки 'ds' (timestamp) и 'y' (значение)
df_prophet = df_merged[['timestamp', 'cpu_usage']].rename(columns={"timestamp": "ds", "cpu_usage": "y"})

if df_prophet.empty or len(df_prophet) < 2 : # Prophet требует минимум 2 точки данных
     print(f"Error: Not enough data points ({len(df_prophet)}) for Prophet training after processing. Exiting.", file=sys.stderr)
     sys.exit(1)

print(f"Training Prophet on {len(df_prophet)} data points...")
# Инициализируем и обучаем модель
try:
    model_prophet = Prophet(interval_width=0.95) # Можно добавить weekly_seasonality=True/False, daily_seasonality=True/False по необходимости
    model_prophet.fit(df_prophet)
    print("Prophet model training complete.")
    # Сохраняем модель
    joblib.dump(model_prophet, MODEL_PROPHET_PATH)
    print(f"Prophet model saved successfully to {MODEL_PROPHET_PATH}")
except Exception as e:
    print(f"ERROR during Prophet model training or saving: {e}", file=sys.stderr)
    sys.exit(1)


# 4. Подготовка данных и обучение модели масштабирования (ElasticNet/LR)
print("\n--- 4. Training Scaler Model (Replicas Calculation) ---")

# --- Расчет целевой переменной 'replicas' ---
# Используем простую формулу как baseline. В реальном мире это требует анализа производительности.
# Добавляем небольшое значение к знаменателю, чтобы избежать деления на ноль
df_merged['replicas_calculated'] = (df_merged['cpu_usage'] / (CPU_PER_REPLICA + 1e-6) +
                                     df_merged['active_connections'] / (CONN_PER_REPLICA + 1e-6))

# Округляем до ближайшего целого и ограничиваем
df_merged['replicas_calculated'] = np.round(df_merged['replicas_calculated']).astype(int)
df_merged['replicas_calculated'] = np.clip(df_merged['replicas_calculated'], MIN_REPLICAS_CALC, MAX_REPLICAS_CALC)
print(f"Calculated target 'replicas' based on formula. Sample:")
print(df_merged[['timestamp', 'cpu_usage', 'active_connections', 'replicas_calculated']].head())

# Определяем признаки (X) и цель (y)
X = df_merged[["cpu_usage", "active_connections"]]
y = df_merged["replicas_calculated"]

if X.empty or y.empty or len(X) != len(y):
     print(f"Error: Feature matrix X or target vector y is empty or sizes mismatch after processing. Exiting.", file=sys.stderr)
     sys.exit(1)

# Создаем и обучаем пайплайн: StandardScaler -> ElasticNet
print(f"Training Scaler model on {len(X)} data points...")
try:
    model_lr_pipeline = Pipeline([
        ('scaler', StandardScaler()), # Стандартизация признаков
        ('regressor', ElasticNet(alpha=0.1, l1_ratio=0.5, random_state=42)) # Модель регрессии
    ])
    model_lr_pipeline.fit(X, y)
    print("Scaler model training complete.")
    # Сохраняем пайплайн
    joblib.dump(model_lr_pipeline, MODEL_LR_PATH)
    print(f"Scaler model (pipeline) saved successfully to {MODEL_LR_PATH}")
except Exception as e:
    print(f"ERROR during Scaler model training or saving: {e}", file=sys.stderr)
    sys.exit(1)

print("\n--- Model Retraining Process Finished Successfully! ---")