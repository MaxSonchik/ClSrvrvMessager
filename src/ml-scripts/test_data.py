import pandas as pd
import numpy as np

# Генерация синтетических данных [[2]]
dates = pd.date_range(start="2025-03-25", periods=168, freq="h")
cpu = np.random.normal(loc=60, scale=15, size=168)
latency = np.random.exponential(scale=0.5, size=168) + 0.5
replicas = np.clip(cpu / 30 + latency * 2, 1, 10).astype(int)

df = pd.DataFrame({
    "timestamp": dates,
    "cpu_usage": cpu,
    "latency": latency,
    "replicas": replicas
})
df.to_csv("training_data.csv", index=False)