# test_data.py
import pandas as pd
import numpy as np

# Задаем желаемое количество точек данных
num_points = 50000


dates = pd.date_range(start="2020-01-01", periods=num_points, freq="h") # Изменен старт для большего реализма


cpu = np.random.normal(loc=60, scale=15, size=num_points)

cpu = np.clip(cpu, 0, None)

latency = np.random.exponential(scale=0.5, size=num_points) + 0.1 # Добавим небольшой базовый уровень

max_replicas = 20
replicas = np.clip(np.round(cpu / 20 + latency * 5), 1, max_replicas).astype(int)

# Создаем DataFrame
df = pd.DataFrame({
    "timestamp": dates,
    "cpu_usage": cpu,
    "latency": latency,
    "replicas": replicas
})

# Сохраняем в CSV
output_filename = "training_data.csv"
df.to_csv(output_filename, index=False)

print(f"Successfully generated {num_points} records into {output_filename}")