from __future__ import annotations

import os
from typing import Iterable

from fastapi import FastAPI, HTTPException, Response
from fastapi.middleware.cors import CORSMiddleware
from pydantic import ValidationError

from .motor_client import MotorClient, MotorUnavailable
from .schemas import MoveRequest, MoveResponse


def _cors_origins() -> list[str]:
    configured = os.getenv("CORS_ORIGINS")
    if configured:
        return [origin.strip() for origin in configured.split(",") if origin.strip()]
    return [
        "http://localhost:8080",
        "http://127.0.0.1:8080",
    ]


def _metrics_lines() -> Iterable[str]:
    yield "# HELP mancala_backend_requests_total HTTP requests handled by the backend"
    yield "# TYPE mancala_backend_requests_total counter"
    yield f"mancala_backend_requests_total {app.state.requests_total}"
    yield "# HELP mancala_backend_move_requests_total Move requests delegated to the motor"
    yield "# TYPE mancala_backend_move_requests_total counter"
    yield f"mancala_backend_move_requests_total {app.state.move_requests_total}"


app = FastAPI(title="Mancala API", version="1.0.0")
app.state.requests_total = 0
app.state.move_requests_total = 0

app.add_middleware(
    CORSMiddleware,
    allow_origins=_cors_origins(),
    allow_methods=["GET", "POST", "OPTIONS"],
    allow_headers=["Content-Type"],
)

motor = MotorClient()


@app.middleware("http")
async def count_requests(request, call_next):
    app.state.requests_total += 1
    return await call_next(request)


@app.get("/healthz")
def healthz() -> dict[str, str]:
    return {"status": "ok"}


@app.get("/readyz")
async def readyz() -> dict[str, str]:
    if not await motor.is_ready():
        raise HTTPException(status_code=503, detail="motor unavailable")
    return {"status": "ready"}


@app.post("/move", response_model=MoveResponse)
async def move(req: MoveRequest) -> MoveResponse:
    try:
        raw_response = await motor.call_motor(req)
    except MotorUnavailable as exc:
        raise HTTPException(status_code=503, detail=f"motor unavailable: {exc}") from exc

    try:
        response = MoveResponse.model_validate(raw_response.model_dump() if hasattr(raw_response, "model_dump") else raw_response)
    except ValidationError as exc:
        raise HTTPException(status_code=503, detail="motor response is invalid") from exc

    app.state.move_requests_total += 1
    return response


@app.get("/metrics")
async def metrics() -> Response:
    body = "\n".join(_metrics_lines()) + "\n"
    try:
        body += await motor.metrics()
    except MotorUnavailable:
        body += "mancala_backend_motor_up 0\n"
    else:
        body += "mancala_backend_motor_up 1\n"
    return Response(content=body, media_type="text/plain; version=0.0.4")
