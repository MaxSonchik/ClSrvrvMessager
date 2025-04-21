import pandas as pd
import joblib
import numpy as np
from prophet import Prophet
from sklearn.linear_model import ElasticNet # Или LinearRegression, если использовали его
from sklearn.preprocessing import StandardScaler
from sklearn.pipeline import Pipeline
import os
import sys


MODEL_DIR = "."  # Сохраняем модели в текущую директорию (.)

MODEL_PROPHET_PATH = os.path.join(MODEL_DIR, "prophet_cpu.pkl")
MODEL_LR_PATH = os.path.join(MODEL_DIR, "lr_scaler.pkl")
INPUT_CSV_PATH = "local_training_data.csv" # Имя CSV файла, сгенерированного generate_local_data.py

MIN_REPLICAS = 1
MAX_REPLICAS = 25 # Убедитесь, что это значение не меньше максимального в ваших данных

# --- Инициализация ---
print(f"--- Starting Local Model Training ---")
print(f"Input data file: {INPUT_CSV_PATH}")
print(f"Output model directory: {MODEL_DIR}")
print(f"Prophet CPU model path: {MODEL_PROPHET_PATH}")
print(f"Scaler LR/ElasticNet model path: {MODEL_LR_PATH}")

# --- Основной скрипт обучения ---

# 1. Загрузка данных из ЛОКАЛЬНОГО CSV
print(f"\n--- 1. Loading data from {INPUT_CSV_PATH} ---")
try:
    # Используем parse_dates для автоматического преобразования колонки timestamp
    df_merged = pd.read_csv(INPUT_CSV_PATH, parse_dates=['timestamp'])
    print(f"Loaded {len(df_merged)} records.")
    if df_merged.empty:
        print("Error: CSV file is empty. Exiting.", file=sys.stderr)
        sys.exit(1)
    # Проверяем наличие необходимых колонок
    required_cols = ['timestamp', 'cpu_usage_rate', 'active_connections', 'replicas']
    if not all(col in df_merged.columns for col in required_cols):
         missing_cols = [col for col in required_cols if col not in df_merged.columns]
         print(f"Error: Missing required columns in CSV: {missing_cols}. Exiting.", file=sys.stderr)
         # Подсказка, какие колонки есть
         print(f"Available columns: {df_merged.columns.tolist()}", file=sys.stderr)
         sys.exit(1)

except FileNotFoundError:
    print(f"Error: Input file not found at {INPUT_CSV_PATH}. Exiting.", file=sys.stderr)
    sys.exit(1)
except Exception as e:
     print(f"Error loading or parsing CSV: {e}. Exiting.", file=sys.stderr)
     sys.exit(1)

print("Sample data:\n", df_merged.head())
print("\nData description:\n", df_merged.describe())

# 2. Обучение Prophet для прогнозирования CPU
print("\n--- 2. Training Prophet model for CPU prediction ---")
df_prophet = df_merged[['timestamp', 'cpu_usage_rate']].rename(columns={"timestamp": "ds", "cpu_usage_rate": "y"})

# Проверка на NaN перед обучением Prophet
if df_prophet['y'].isnull().any():
    nan_count = df_prophet['y'].isnull().sum()
    print(f"Warning: {nan_count} NaN values found in 'cpu_usage_rate' for Prophet. Filling with forward-fill then back-fill.", file=sys.stderr)
    df_prophet['y'] = df_prophet['y'].ffill().bfill() # Заполняем пропуски
    # Если после заполнения все еще есть NaN (например, если все значения NaN), удаляем строки
    df_prophet.dropna(subset=['y'], inplace=True)

if len(df_prophet) < 2: # Prophet требует минимум 2 точки данных
     print("Error: Not enough valid data points (< 2) for Prophet training after handling NaNs. Exiting.", file=sys.stderr)
     sys.exit(1)

print(f"Training Prophet model using {len(df_prophet)} data points...")
model_prophet = Prophet(interval_width=0.95) # Можно добавить параметры сезонности при необходимости

try:
    model_prophet.fit(df_prophet)
    print("Prophet model training complete.")
    joblib.dump(model_prophet, MODEL_PROPHET_PATH)
    print(f"Prophet model saved to {MODEL_PROPHET_PATH}")
except Exception as e:
    print(f"Error training or saving Prophet model: {e}. Exiting.", file=sys.stderr)
    sys.exit(1)


# 3. Подготовка данных и обучение модели масштабирования (ElasticNet/LR)
print("\n--- 3. Training Scaler model (ElasticNet/LR) ---")


X = df_merged[["cpu_usage_rate", "active_connections"]]
y = df_merged["replicas"]

# Проверка на NaN в фичах и таргете
initial_rows = len(df_merged)
df_merged.dropna(subset=["cpu_usage_rate", "active_connections", "replicas"], inplace=True)
if len(df_merged) < initial_rows:
    print(f"Warning: Dropped {initial_rows - len(df_merged)} rows with NaN values before training Scaler model.", file=sys.stderr)

X = df_merged[["cpu_usage_rate", "active_connections"]]
y = df_merged["replicas"]

if X.empty or y.empty or len(X) < 2:
    print("Error: Not enough valid data points (< 2) for Scaler model training after handling NaNs. Exiting.", file=sys.stderr)
    sys.exit(1)

print(f"Training Scaler model using {len(X)} data points...")

# Создание и обучение пайплайна (Scaler + Model)
model_lr_pipeline = Pipeline([
    ('scaler', StandardScaler()), # Стандартизация фичей
    ('regressor', ElasticNet(alpha=0.1, l1_ratio=0.5, random_state=42)) # Модель регрессии

])

try:
    model_lr_pipeline.fit(X, y)
    print("Scaler model training complete.")
    joblib.dump(model_lr_pipeline, MODEL_LR_PATH)
    print(f"Scaler model saved to {MODEL_LR_PATH}")
except Exception as e:
     print(f"Error training or saving Scaler model: {e}. Exiting.", file=sys.stderr)
     sys.exit(1)

print("\n--- Local model training finished successfully! ---")