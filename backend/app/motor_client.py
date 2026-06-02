from __future__ import annotations

from dataclasses import dataclass
import os

import httpx

from .schemas import MoveRequest, MoveResponse


class MotorUnavailable(Exception):
    """Raised when the C++ motor cannot be reached or returns invalid data."""


@dataclass
class MotorClient:
    motor_url: str = os.getenv("MOTOR_URL", "http://localhost:9000")
    timeout_seconds: float = float(os.getenv("MOTOR_TIMEOUT_SECONDS", "10"))

    def _url(self, path: str) -> str:
        return f"{self.motor_url.rstrip('/')}{path}"

    def is_ready(self) -> bool:
        try:
            response = httpx.get(self._url("/readyz"), timeout=2.0)
            return response.status_code == 200
        except httpx.HTTPError:
            return False

    def move(self, req: MoveRequest) -> MoveResponse:
        try:
            response = httpx.post(
                self._url("/move"),
                json=req.model_dump(),
                timeout=self.timeout_seconds,
            )
            response.raise_for_status()
            return MoveResponse.model_validate(response.json())
        except (httpx.HTTPError, ValueError) as exc:
            raise MotorUnavailable("motor service did not respond successfully") from exc

    def metrics(self) -> str:
        try:
            response = httpx.get(self._url("/metrics"), timeout=2.0)
            response.raise_for_status()
            return response.text
        except httpx.HTTPError as exc:
            raise MotorUnavailable("motor metrics are unavailable") from exc
