FROM python:3.9-slim

# Обновление pip и установка зависимостей
RUN pip install --upgrade pip && \
    pip install --no-cache-dir \
    prophet==1.1.6 \
    scikit-learn==1.3.2 \
    joblib==1.4.2 \
    prometheus-api-client==0.5.7 \
    kubernetes==28.1.0

# Копирование файлов
COPY . /app
WORKDIR /app

CMD ["python", "ml_scaler.py"]