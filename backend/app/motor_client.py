from __future__ import annotations

from dataclasses import dataclass
from typing import Any
import os

import httpx

from .schemas import MoveRequest, MoveResponse


class MotorUnavailable(Exception):
    """Raised when the C++ motor cannot be reached or returns invalid data."""


@dataclass
class MotorClient:
    motor_url: str = os.getenv("MOTOR_URL", "http://localhost:9000")
    timeout_seconds: float = float(os.getenv("MOTOR_TIMEOUT_SECONDS", "10"))
    use_mock: bool = os.getenv("USE_MOCK", "false").lower() == "true"

    def _url(self, path: str) -> str:
        return f"{self.motor_url.rstrip('/')}{path}"

    def _board_score(self, req: MoveRequest) -> int:
        if req.side == 0:
            own = sum(req.board[0:6]) + req.board[6]
            opp = sum(req.board[7:13]) + req.board[13]
        else:
            own = sum(req.board[7:13]) + req.board[13]
            opp = sum(req.board[0:6]) + req.board[6]
        return own - opp

    def _best_pit(self, req: MoveRequest) -> int:
        pits = range(0, 6) if req.side == 0 else range(7, 13)
        for index in pits:
            if req.board[index] > 0:
                return index
        return -1

    def _mock_response(self, req: MoveRequest) -> MoveResponse:
        evaluation = self._board_score(req)
        move = self._best_pit(req)
        if req.algo == "mcts":
            payload: dict[str, Any] = {
                "algo": "mcts",
                "move": move,
                "evaluation": evaluation,
                "elapsed_ms": max(1, req.simulations or 1),
                "stats": {
                    "algo": "mcts",
                    "rollouts": req.simulations or 1,
                    "win_rate": max(0.0, min(1.0, 0.5 + (evaluation / 96.0))),
                },
                "threads_used": req.threads,
            }
            return MoveResponse.model_validate(payload)

        payload = {
            "algo": "alphabeta",
            "move": move,
            "evaluation": evaluation,
            "elapsed_ms": max(1, int(abs(evaluation) + (req.depth or 1))),
            "stats": {
                "algo": "alphabeta",
                "nodes": max(1, (req.depth or 1) * 12),
                "prunes": max(0, (req.depth or 1) * 3),
            },
            "threads_used": req.threads,
        }
        return MoveResponse.model_validate(payload)

    def _normalize_response(self, payload: dict[str, Any], req: MoveRequest) -> MoveResponse:
        stats = payload.get("stats") if isinstance(payload.get("stats"), dict) else {}
        if req.algo == "mcts":
            if {"rollouts", "win_rate"}.issubset(stats):
                stats_payload = {"algo": "mcts", **stats}
            else:
                evaluation = float(payload.get("evaluation", 0))
                stats_payload = {
                    "algo": "mcts",
                    "rollouts": req.simulations or int(stats.get("rollouts", 1) or 1),
                    "win_rate": max(0.0, min(1.0, 0.5 + (evaluation / 96.0))),
                }
            algo = "mcts"
        else:
            if {"nodes", "prunes"}.issubset(stats):
                stats_payload = {"algo": "alphabeta", **stats}
            else:
                stats_payload = {
                    "algo": "alphabeta",
                    "nodes": int(stats.get("nodes", 0) or 0),
                    "prunes": int(stats.get("prunes", 0) or 0),
                }
            algo = "alphabeta"

        normalized = {
            "algo": algo,
            "move": payload.get("move", -1),
            "evaluation": payload.get("evaluation", 0),
            "elapsed_ms": payload.get("elapsed_ms", 0),
            "stats": stats_payload,
            "threads_used": payload.get("threads_used", 1),
        }
        return MoveResponse.model_validate(normalized)

    async def is_ready(self) -> bool:
        if self.use_mock:
            return True
        try:
            async with httpx.AsyncClient(timeout=2.0) as client:
                response = await client.get(self._url("/readyz"))
            return response.status_code == 200
        except httpx.HTTPError:
            return False

    async def call_motor(self, req: MoveRequest) -> MoveResponse:
        if self.use_mock:
            return self._mock_response(req)

        try:
            async with httpx.AsyncClient(timeout=self.timeout_seconds) as client:
                response = await client.post(self._url("/move"), json=req.model_dump(exclude_none=True))
            response.raise_for_status()
            payload = response.json()
            if not isinstance(payload, dict):
                raise MotorUnavailable("motor service returned invalid JSON")
            return self._normalize_response(payload, req)
        except httpx.ConnectError as exc:
            raise MotorUnavailable("motor service is unreachable") from exc
        except (httpx.HTTPError, ValueError, TypeError) as exc:
            raise MotorUnavailable("motor service did not respond successfully") from exc

    async def metrics(self) -> str:
        if self.use_mock:
            return "mancala_motor_mock_enabled 1\n"
        try:
            async with httpx.AsyncClient(timeout=2.0) as client:
                response = await client.get(self._url("/metrics"))
            response.raise_for_status()
            return response.text
        except httpx.HTTPError as exc:
            raise MotorUnavailable("motor metrics are unavailable") from exc
