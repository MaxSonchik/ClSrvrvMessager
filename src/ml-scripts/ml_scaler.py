from kubernetes import client, config
from prometheus_api_client import PrometheusConnect
import joblib
import numpy as np
import time

# Загрузка моделей
model_prophet = joblib.load("prophet.pkl")
model_lr = joblib.load("lr.pkl")

# Подключение к Prometheus и Kubernetes
config.load_incluster_config()
api = client.AppsV1Api()
prom = PrometheusConnect(url="http://prometheus-server.default.svc:9090")

def predict_replicas():
    # Получение метрик [[8]]
    cpu = prom.get_current_metric_value(
        query="avg_over_time(container_cpu_usage_seconds_total{container='messenger'}[5m])"
    )[0]["value"][1]
    
    latency = prom.get_current_metric_value(
        query="histogram_quantile(0.99, sum(rate(http_request_duration_seconds_bucket[5m])) by (le))"
    )[0]["value"][1]
    
    # Прогноз нагрузки
    future = model_prophet.make_future_dataframe(periods=1, freq="H")
    forecast = model_prophet.predict(future)
    predicted_cpu = forecast["yhat"].iloc[-1]
    
    # Определение реплик [[9]]
    X = np.array([[predicted_cpu, latency]])
    replicas = int(np.round(model_lr.predict(X))[0])
    return max(1, min(replicas, 10))

# Основной цикл
while True:
    try:
        replicas = predict_replicas()
        api.patch_namespaced_deployment_scale(
            name="messenger-app",
            namespace="default",
            body={"spec": {"replicas": replicas}}
        )
    except Exception as e:
        print(f"Ошибка: {e}")
    time.sleep(300)