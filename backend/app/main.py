from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware

from .schemas import MoveRequest, MoveResponse
from .motor_client import MotorClient, MotorUnavailable


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
    return {"ok": True}


@app.get("/readyz")
def readyz():
    if not motor.is_ready():
        raise HTTPException(status_code=503, detail="motor not ready")
    return {"ok": True}


@app.post("/move", response_model=MoveResponse)
def move(req: MoveRequest):
    try:
        return motor.move(req)
    except MotorUnavailable as exc:
        raise HTTPException(status_code=503, detail=str(exc))


@app.get("/metrics")
def metrics():
    # Placeholder. In later phases, expose Prometheus/plain-text metrics.
    return {"metrics": []}

