#!/bin/bash
GREEN='\033[0;32m'
NC='\033[0m' # No Color
eval $(minikube docker-env)

echo "Building messenger-app..."
cd app
docker build -t messenger-app:latest . && cd ..

echo "Building ML-scaler..."
cd ml-scripts
docker build -t ml-scaler-image:latest -f Dockerfile . && cd ..

# 3. Удаление старых ресурсов
echo "Cleaning up old resources..."
helm uninstall prometheus grafana --namespace default --no-hooks
kubectl delete all --all

# 4. Установка Prometheus без PVC и проб
echo "Installing Prometheus..."
helm install prometheus prometheus-community/prometheus \
  --set server.persistence.enabled=false \
  --set alertmanager.persistence.enabled=false \
  --set server.livenessProbe.enabled=false \
  --set server.readinessProbe.enabled=false \
  --set alertmanager.livenessProbe.enabled=false \
  --set alertmanager.readinessProbe.enabled=false \
  --set prometheus-node-exporter.pspEnabled=false

# 5. Установка Grafana без PVC
echo "Installing Grafana..."
helm install grafana grafana/grafana \
  --set persistence.enabled=false

# 6. Применение манифестов приложения
echo "Deploying application..."
kubectl apply -f k8s/

# 7. Проверка статуса
echo "Checking pods status..."
kubectl get pods -n default --watch