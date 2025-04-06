from prophet import Prophet
from sklearn.linear_model import ElasticNet # Или LinearRegression, что вы использовали
from sklearn.preprocessing import StandardScaler
from sklearn.pipeline import Pipeline
from sklearn.model_selection import train_test_split # Полезно для оценки модели
import pandas as pd
import joblib
import numpy as np
from prometheus_api_client import PrometheusConnect, MetricsList
from datetime import datetime, timedelta
import os
import sys

# --- Конфигурация ---
MODEL_DIR = os.getenv("MODEL_DIR", "/models")
MODEL_PROPHET_PATH = os.path.join(MODEL_DIR, "prophet_cpu.pkl")
MODEL_LR_PATH = os.path.join(MODEL_DIR, "lr_scaler.pkl")
PROMETHEUS_URL = os.getenv("PROMETHEUS_URL", "http://prometheus-kube-prometheus-prometheus.monitoring.svc:9090")
APP_NAME = os.getenv("APP_NAME", "messenger-app")
APP_NAMESPACE = os.getenv("APP_NAMESPACE", "messenger-app")
TRAINING_DAYS = int(os.getenv("TRAINING_DAYS", 7)) # Сколько дней данных использовать для обучения
PROMETHEUS_STEP = os.getenv("PROMETHEUS_STEP", "1m") # Шаг данных в Prometheus

# --- Инициализация ---
print(f"Starting model retraining process...")
print(f"Model directory: {MODEL_DIR}")
print(f"Prometheus URL: {PROMETHEUS_URL}")
print(f"Fetching data for app: {APP_NAMESPACE}/{APP_NAME}")
print(f"Training data window: {TRAINING_DAYS} days")
print(f"Prometheus step: {PROMETHEUS_STEP}")

os.makedirs(MODEL_DIR, exist_ok=True) # Создаем директорию, если ее нет

prom = PrometheusConnect(url=PROMETHEUS_URL, disable_ssl=True)

# --- Функции ---

def fetch_prometheus_data(metric_query, days):
    """Запрашивает исторические данные из Prometheus."""
    end_time = datetime.now()
    start_time = end_time - timedelta(days=days)
    print(f"Fetching data for query: {metric_query}")
    print(f"Time range: {start_time} to {end_time}")
    try:
        metric_data = prom.custom_query_range(
            query=metric_query,
            start_time=start_time,
            end_time=end_time,
            step=PROMETHEUS_STEP,
        )
        if not metric_data:
            print(f"Warning: No data returned for query: {metric_query}", file=sys.stderr)
            return pd.DataFrame() # Возвращаем пустой DataFrame

        # Prometheus возвращает список метрик, даже если запрос один. Берем первый.
        # Преобразуем в DataFrame: timestamp, value
        values = metric_data[0]['values']
        df = pd.DataFrame(values, columns=['timestamp', 'value'])
        df['timestamp'] = pd.to_datetime(df['timestamp'], unit='s')
        df['value'] = pd.to_numeric(df['value'])
        df = df.set_index('timestamp')
        print(f"Fetched {len(df)} data points.")
        return df

    except Exception as e:
        print(f"Error fetching data for query '{metric_query}': {e}", file=sys.stderr)
        return pd.DataFrame()


# --- Основной скрипт обучения ---

# 1. Загрузка данных из Prometheus
cpu_query = f"sum(rate(container_cpu_usage_seconds_total{{namespace='{APP_NAMESPACE}', container='{APP_NAME}'}}[5m]))" # Должно совпадать с scaler'ом
connections_query = f"sum(messenger_active_connections{{namespace='{APP_NAMESPACE}', app='{APP_NAME}'}})" # Уточните селекторы

df_cpu = fetch_prometheus_data(cpu_query, TRAINING_DAYS)
df_connections = fetch_prometheus_data(connections_query, TRAINING_DAYS)

if df_cpu.empty or df_connections.empty:
    print("Error: Could not fetch sufficient data from Prometheus. Exiting.", file=sys.stderr)
    sys.exit(1)

# 2. Объединение и подготовка данных
df_cpu = df_cpu.rename(columns={'value': 'cpu_usage'})
df_connections = df_connections.rename(columns={'value': 'active_connections'})

# Объединяем по временной метке. Используем внешний join и интерполяцию,
# т.к. метрики могут приходить не одновременно.
df_merged = pd.merge(df_cpu, df_connections, left_index=True, right_index=True, how='outer')
# Заполняем пропуски, например, методом ffill (forward fill)
df_merged = df_merged.ffill().bfill() # Заполняем вперед, потом назад (если в начале есть NaN)
df_merged = df_merged.dropna() # Удаляем строки, если NaN остались

if df_merged.empty:
    print("Error: Dataframe is empty after merging and cleaning. Exiting.", file=sys.stderr)
    sys.exit(1)

# Сбрасываем индекс, чтобы timestamp стал колонкой
df_merged = df_merged.reset_index()
print(f"Combined dataframe shape after cleaning: {df_merged.shape}")
print("Sample data:\n", df_merged.head())

# 3. Обучение Prophet для прогнозирования CPU
print("\n--- Training Prophet model for CPU ---")
df_prophet = df_merged[['timestamp', 'cpu_usage']].rename(columns={"timestamp": "ds", "cpu_usage": "y"})

# Опционально: Удаление выбросов перед обучением Prophet
# q_low = df_prophet["y"].quantile(0.01)
# q_hi  = df_prophet["y"].quantile(0.99)
# df_prophet_filtered = df_prophet[(df_prophet["y"] > q_low) & (df_prophet["y"] < q_hi)]
# print(f"Using {len(df_prophet_filtered)} points for Prophet after outlier removal.")

model_prophet = Prophet(interval_width=0.95) # Можно добавить параметры сезонности, если нужно
model_prophet.fit(df_prophet) # Используем полный или отфильтрованный датасет
print("Prophet model training complete.")
joblib.dump(model_prophet, MODEL_PROPHET_PATH)
print(f"Prophet model saved to {MODEL_PROPHET_PATH}")


# 4. Подготовка данных и обучение модели масштабирования (ElasticNet/LR)
print("\n--- Training Scaler model (ElasticNet/LR) ---")

# Создаем целевую переменную 'replicas'.
# !! ЭТО САМЫЙ СЛОЖНЫЙ МОМЕНТ: КАК ОПРЕДЕЛИТЬ ИДЕАЛЬНОЕ ЧИСЛО РЕПЛИК ДЛЯ ПРОШЛЫХ ДАННЫХ?
# Вариант 1 (Упрощенный, как в вашем исходном коде): Используем простую формулу.
# Это НЕ ОБУЧЕНИЕ на реальной эффективности, а подгонка под формулу.
df_merged['replicas_calculated'] = np.clip(np.round(df_merged['cpu_usage'] / 0.8 + df_merged['active_connections'] / 50), MIN_REPLICAS, MAX_REPLICAS) # Пример формулы, подберите коэффициенты!

# Вариант 2 (Сложный, требует метрик производительности):
# Если бы у вас были исторические данные по latency или error rate, можно было бы
# построить модель, которая предсказывает эти метрики по CPU и connections,
# а затем найти число реплик, которое держит latency/errors ниже порога. Это выходит за рамки простого примера.

# Используем Вариант 1 для демонстрации:
X = df_merged[["cpu_usage", "active_connections"]]
y = df_merged["replicas_calculated"]

# Разделение на трейн/тест для оценки (хорошая практика)
# X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

# Создание и обучение пайплайна (Scaler + Model)
# Используем весь датасет для финальной модели, которая пойдет в прод
model_lr_pipeline = Pipeline([
    ('scaler', StandardScaler()),
    ('regressor', ElasticNet(alpha=0.1, l1_ratio=0.5, random_state=42)) # Или LinearRegression()
])

model_lr_pipeline.fit(X, y)
print("Scaler model training complete.")

# Опционально: Оценка модели на тестовых данных (если делали split)
# from sklearn.metrics import mean_squared_error
# y_pred = model_lr_pipeline.predict(X_test)
# mse = mean_squared_error(y_test, y_pred)
# print(f"Scaler model test MSE: {mse}")

joblib.dump(model_lr_pipeline, MODEL_LR_PATH)
print(f"Scaler model saved to {MODEL_LR_PATH}")

print("\n--- Model retraining process finished successfully! ---")