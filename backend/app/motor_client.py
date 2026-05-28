from dataclasses import dataclass
import os
from typing import Any, Dict

import httpx

from .schemas import MoveRequest, MoveResponse


class MotorUnavailable(Exception):
    """Raised when the real C++ motor cannot be reached."""


def _mock_motor_response(req: MoveRequest) -> Dict[str, Any]:
    return {
        "move": 3,
        "evaluation": 7,
        "elapsed_ms": 124,
        "stats": {
            "algo": req.algo,
            "nodes": 1845210,
            "prunes": 312083,
        },
        "threads_used": req.threads,
    }


def call_motor(req: MoveRequest, motor_url: str | None = None, use_mock: bool = True) -> Dict[str, Any]:
    if use_mock:
        return _mock_motor_response(req)

    if not motor_url:
        raise MotorUnavailable("MOTOR_URL is not configured")

    try:
        response = httpx.post(f"{motor_url.rstrip('/')}/move", json=req.model_dump(), timeout=5.0)
        response.raise_for_status()
        return response.json()
    except httpx.HTTPError as exc:
        raise MotorUnavailable("motor service did not respond successfully") from exc


@dataclass
class MotorClient:
    motor_url: str | None = os.getenv("MOTOR_URL")
    use_mock: bool = os.getenv("MOTOR_USE_MOCK", "true").lower() != "false"

    def is_ready(self) -> bool:
        if self.use_mock:
            return True
        if not self.motor_url:
            return False
        try:
            response = httpx.get(f"{self.motor_url.rstrip('/')}/healthz", timeout=2.0)
            return response.status_code == 200
        except httpx.HTTPError:
            return False

    def move(self, req: MoveRequest) -> MoveResponse:
        data = call_motor(req, self.motor_url, self.use_mock)
        return MoveResponse.model_validate(data)

