from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from .schemas import MoveRequest, MoveResponse
from .motor_client import MotorClient


app = FastAPI(title="Mancala API", version="0.1.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "http://localhost:8080",
    ],
    allow_methods=["GET", "POST", "OPTIONS"],
    allow_headers=["Content-Type"],
)

motor = MotorClient()


@app.get("/healthz")
def healthz():
    return {"status": "ok"}


@app.get("/readyz")
def readyz():
    return {"status": "ready"}


@app.post("/move", response_model=MoveResponse)
def move(req: MoveRequest):
    return motor.move(req)


@app.get("/metrics")
def metrics():
    return {}

