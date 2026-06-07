FROM python:3.11-slim

ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1

WORKDIR /app

COPY requirements.txt ./requirements.txt
RUN pip install --no-cache-dir -r requirements.txt \
 && useradd -r -u 1001 api

COPY app ./app

ENV MOTOR_URL=http://motor:8080
ENV MOTOR_TIMEOUT_SECONDS=10
ENV USE_MOCK=false
ENV CORS_ORIGINS=http://localhost:8080,http://127.0.0.1:8080,http://frontend

EXPOSE 8000
USER api
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]
