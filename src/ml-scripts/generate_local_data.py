# generate_local_data.py
import pandas as pd
import numpy as np

print("Generating local training data...")

# --- Параметры ---
num_points = 5000  # ~7 дней с шагом 2 мин
freq = "2min"
start_date = "2024-04-01 00:00:00"
output_filename = "local_training_data.csv"
max_replicas = 15

# --- Генерация ---
dates = pd.date_range(start=start_date, periods=num_points, freq=freq)
time_in_day = (dates.hour * 60 + dates.minute) / (24 * 60)
daily_seasonality_cpu = 15 * np.sin(2 * np.pi * time_in_day - np.pi/2)
daily_seasonality_conn = 30 * np.sin(2 * np.pi * time_in_day - np.pi/2)
weekly_seasonality_cpu = 5 * (dates.dayofweek < 5)
weekly_seasonality_conn = 20 * (dates.dayofweek < 5)
trend_cpu = np.linspace(0, 5, num_points)
trend_conn = np.linspace(0, 10, num_points)
noise_cpu = np.random.normal(loc=0, scale=5, size=num_points)
noise_conn = np.random.normal(loc=0, scale=10, size=num_points)

base_cpu = 20
# Используем имя 'cpu_usage_rate', чтобы соответствовать запросу в retrain_model.py
cpu_usage_rate = base_cpu + daily_seasonality_cpu + weekly_seasonality_cpu + trend_cpu + noise_cpu
cpu_usage_rate = np.clip(cpu_usage_rate, 0.1, None)

base_conn = 50
active_connections = base_conn + daily_seasonality_conn + weekly_seasonality_conn + trend_conn + noise_conn
active_connections = np.clip(active_connections, 1, None)
active_connections = np.round(active_connections).astype(int)

# Генерируем 'replicas' на основе 'cpu_usage_rate' и 'active_connections'
replicas = np.clip(np.round(cpu_usage_rate / 0.8 + active_connections / 50.0), 1, max_replicas).astype(int)

# --- DataFrame и Сохранение ---
df = pd.DataFrame({
    "timestamp": dates,
    "cpu_usage_rate": cpu_usage_rate, # Имя колонки как в запросе Prometheus
    "active_connections": active_connections,
    "replicas": replicas
})
df.to_csv(output_filename, index=False)

print(f"Successfully generated {num_points} records into {output_filename}")
print("Sample data:\n", df.head())