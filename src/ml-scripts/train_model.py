from prophet import Prophet
from sklearn.linear_model import LinearRegression
import pandas as pd
import joblib
from sklearn.linear_model import ElasticNet
from sklearn.preprocessing import StandardScaler
from sklearn.pipeline import Pipeline

# Загрузка данных
df = pd.read_csv("training_data.csv")

# Обучение Prophet для прогнозирования CPU [[8]]
df_prophet = df.rename(columns={"timestamp": "ds", "cpu_usage": "y"})
model_prophet = Prophet(interval_width=0.95)
model_prophet.fit(df_prophet)

X = df[["cpu_usage", "latency"]]
y = df["replicas"]
model_lr = Pipeline([
    ('scaler', StandardScaler()),
    ('elasticnet', ElasticNet(alpha=0.1, l1_ratio=0.5))
])
model_lr.fit(X, y)

# Сохранение моделей
joblib.dump(model_prophet, "prophet.pkl")
joblib.dump(model_lr, "lr.pkl")