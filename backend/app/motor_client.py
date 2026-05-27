from dataclasses import dataclass
from typing import Any, Dict

from .schemas import MoveRequest, MoveResponse


class MotorUnavailable(Exception):
    pass


def call_motor(req: MoveRequest) -> Dict[str, Any]:
    return {
        "move": 3,
        "evaluation": 7,
        "elapsed_ms": 124,
        "stats": {
            "algo": "alphabeta",
            "nodes": 1845210,
            "prunes": 312083,
            "threads_used": req.threads,
        },
    }


@dataclass
class MotorClient:
    def is_ready(self) -> bool:
        return True

    def move(self, req: MoveRequest) -> MoveResponse:
        data = call_motor(req)
        return MoveResponse.model_validate(data)

