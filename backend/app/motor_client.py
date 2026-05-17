import os
import time
from dataclasses import dataclass
from typing import Any, Dict

import httpx

from .schemas import MoveRequest, MoveResponse


class MotorUnavailable(Exception):
    pass


@dataclass
class MotorClient:
    base_url: str = os.getenv("MOTOR_URL", "http://motor-svc:9000")
    timeout_s: float = float(os.getenv("MOTOR_TIMEOUT_S", "10"))

    def is_ready(self) -> bool:
        try:
            with httpx.Client(timeout=self.timeout_s) as client:
                r = client.get(f"{self.base_url}/healthz")
                return r.status_code == 200
        except Exception:
            return False

    def move(self, req: MoveRequest) -> MoveResponse:
        start = time.time()
        try:
            with httpx.Client(timeout=self.timeout_s) as client:
                r = client.post(f"{self.base_url}/move", json=req.model_dump())
        except Exception as exc:
            raise MotorUnavailable("motor not reachable") from exc

        if r.status_code != 200:
            raise MotorUnavailable(f"motor error: {r.status_code}")

        data: Dict[str, Any] = r.json()
        elapsed_ms = int((time.time() - start) * 1000)
        data.setdefault("elapsed_ms", elapsed_ms)
        data.setdefault("threads_used", req.threads)
        return MoveResponse.model_validate(data)

