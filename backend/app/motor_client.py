from __future__ import annotations

from dataclasses import dataclass, field
from time import perf_counter
from typing import Any
import os

import httpx

from .game import InvalidMove, apply_move, board_score, legal_moves, side_empty, sweep_remaining_seeds
from .schemas import MoveRequest, MoveResponse


class MotorClientError(Exception):
    def __init__(self, status_code: int, message: str) -> None:
        super().__init__(message)
        self.status_code = status_code
        self.message = message


class MotorUnavailable(MotorClientError):
    def __init__(self, message: str = "Motor no disponible") -> None:
        super().__init__(503, message)


def _env_bool(name: str, default: bool) -> bool:
    value = os.getenv(name)
    if value is None:
        return default
    return value.strip().lower() in {"1", "true", "yes", "y", "on"}


@dataclass
class MotorClient:
    motor_url: str = os.getenv("MOTOR_URL", "http://motor:8080")
    timeout_seconds: float = float(os.getenv("MOTOR_TIMEOUT_SECONDS", "10"))
    use_mock: bool = field(default_factory=lambda: _env_bool("USE_MOCK", False))

    def _url(self, path: str) -> str:
        return f"{self.motor_url.rstrip('/')}{path}"

    def _mock_best_move(self, req: MoveRequest) -> int:
        moves = legal_moves(req.tablero, req.lado)
        if not moves:
            return -1

        best_move = moves[0]
        best_score = -10**9
        for move in moves:
            try:
                applied = apply_move(req.tablero, req.lado, move)
            except InvalidMove:
                continue

            score = board_score(applied.tablero_nuevo, req.lado)
            if applied.turno_extra:
                score += 5
            if applied.terminal and applied.ganador == req.lado:
                score += 1000
            if score > best_score:
                best_score = score
                best_move = move
        return best_move

    def _terminal_response(self, req: MoveRequest, elapsed_ms: int) -> MoveResponse:
        board = req.tablero.copy()
        terminal = side_empty(board, 0) or side_empty(board, 1)
        ganador: int | None = None
        if terminal:
            sweep_remaining_seeds(board)
            if board[6] > board[13]:
                ganador = 0
            elif board[13] > board[6]:
                ganador = 1
            else:
                ganador = -1

        stats = self._stats(req, elapsed_ms, nodes=0, prunes=0, rollouts=0, win_rate=0.5)
        return MoveResponse(
            movimiento=-1,
            tablero_nuevo=board,
            lado_siguiente=req.lado,
            terminal=terminal,
            ganador=ganador,
            estadisticas=stats,
        )

    def _stats(
        self,
        req: MoveRequest,
        elapsed_ms: int,
        *,
        nodes: int | None = None,
        prunes: int | None = None,
        rollouts: int | None = None,
        win_rate: float | None = None,
        evaluation: int | None = None,
    ) -> dict[str, Any]:
        base: dict[str, Any] = {
            "algoritmo": req.algoritmo,
            "tiempo_ms": elapsed_ms,
            "hilos": req.hilos,
        }
        if evaluation is not None:
            base["evaluacion"] = evaluation

        if req.algoritmo == "mcts":
            base["simulaciones"] = req.simulaciones
            base["rollouts"] = rollouts if rollouts is not None else req.simulaciones
            base["win_rate"] = win_rate if win_rate is not None else 0.5
        else:
            base["profundidad"] = req.profundidad
            base["nodos"] = nodes if nodes is not None else max(1, (req.profundidad or 1) * 12)
            base["podas"] = prunes if prunes is not None else max(0, (req.profundidad or 1) * 3)
        return base

    def _response_from_move(
        self,
        req: MoveRequest,
        move: int,
        *,
        elapsed_ms: int,
        stats_payload: dict[str, Any] | None = None,
        evaluation: int | None = None,
    ) -> MoveResponse:
        if move < 0:
            return self._terminal_response(req, elapsed_ms)

        try:
            applied = apply_move(req.tablero, req.lado, move)
        except InvalidMove as exc:
            raise MotorClientError(500, f"Respuesta invalida del motor: {exc}") from exc

        stats_payload = stats_payload or {}
        if req.algoritmo == "mcts":
            stats = self._stats(
                req,
                elapsed_ms,
                rollouts=int(stats_payload.get("rollouts", req.simulaciones or 0)),
                win_rate=float(stats_payload.get("win_rate", 0.5)),
                evaluation=evaluation,
            )
        else:
            stats = self._stats(
                req,
                elapsed_ms,
                nodes=int(stats_payload.get("nodes", stats_payload.get("nodos", 0)) or 0),
                prunes=int(stats_payload.get("prunes", stats_payload.get("podas", 0)) or 0),
                evaluation=evaluation,
            )

        return MoveResponse(
            movimiento=applied.movimiento,
            tablero_nuevo=applied.tablero_nuevo,
            lado_siguiente=applied.lado_siguiente,
            terminal=applied.terminal,
            ganador=applied.ganador,
            estadisticas=stats,
        )

    def _mock_response(self, req: MoveRequest) -> MoveResponse:
        start = perf_counter()
        move = self._mock_best_move(req)
        elapsed_ms = max(1, int((perf_counter() - start) * 1000))
        evaluation = board_score(req.tablero, req.lado)

        if req.algoritmo == "mcts":
            rollouts = req.simulaciones or 1
            win_rate = max(0.0, min(1.0, 0.5 + (evaluation / 96.0)))
            stats_payload = {"rollouts": rollouts, "win_rate": win_rate}
        else:
            depth = req.profundidad or 1
            stats_payload = {"nodes": max(1, depth * 12), "prunes": max(0, depth * 3)}

        return self._response_from_move(
            req,
            move,
            elapsed_ms=elapsed_ms,
            stats_payload=stats_payload,
            evaluation=evaluation,
        )

    def _extract_error_message(self, response: httpx.Response) -> str:
        try:
            payload = response.json()
        except ValueError:
            return response.text.strip() or f"HTTP {response.status_code}"

        if isinstance(payload, dict):
            detail = payload.get("detail") or payload.get("error") or payload.get("message")
            if detail:
                return str(detail)
        return f"HTTP {response.status_code}"

    def _normalize_motor_response(self, payload: dict[str, Any], req: MoveRequest) -> MoveResponse:
        move = payload.get("movimiento", payload.get("move"))
        if not isinstance(move, int):
            raise MotorClientError(500, "Respuesta invalida del motor: movimiento ausente")

        stats = payload.get("estadisticas", payload.get("stats", {}))
        if not isinstance(stats, dict):
            stats = {}

        elapsed = payload.get("elapsed_ms", stats.get("tiempo_ms", 0))
        elapsed_ms = int(elapsed or 0)
        evaluation = payload.get("evaluation", stats.get("evaluacion"))
        evaluation = int(evaluation) if evaluation is not None else None

        return self._response_from_move(
            req,
            move,
            elapsed_ms=elapsed_ms,
            stats_payload=stats,
            evaluation=evaluation,
        )

    async def is_ready(self) -> bool:
        if self.use_mock:
            return True
        try:
            async with httpx.AsyncClient(timeout=2.0) as client:
                response = await client.get(self._url("/healthz"))
            return response.status_code == 200
        except httpx.HTTPError:
            return False

    async def call_motor(self, req: MoveRequest) -> MoveResponse:
        if self.use_mock:
            return self._mock_response(req)

        try:
            async with httpx.AsyncClient(timeout=self.timeout_seconds) as client:
                response = await client.post(self._url("/move"), json=req.motor_payload())
        except (httpx.TimeoutException, httpx.ConnectError, httpx.NetworkError) as exc:
            raise MotorUnavailable("Motor no disponible") from exc
        except httpx.RequestError as exc:
            raise MotorUnavailable("Motor no disponible") from exc

        if response.status_code == 400:
            raise MotorClientError(400, self._extract_error_message(response))
        if response.status_code == 501:
            raise MotorClientError(503, "MCTS no disponible aún")
        if 500 <= response.status_code:
            raise MotorClientError(500, "Error interno del motor")
        if not 200 <= response.status_code < 300:
            raise MotorClientError(500, self._extract_error_message(response))

        try:
            payload = response.json()
        except ValueError as exc:
            raise MotorClientError(500, "Respuesta invalida del motor: JSON invalido") from exc
        if not isinstance(payload, dict):
            raise MotorClientError(500, "Respuesta invalida del motor")

        return self._normalize_motor_response(payload, req)

    async def metrics(self) -> str:
        if self.use_mock:
            return "mancala_motor_mock_enabled 1\n"
        try:
            async with httpx.AsyncClient(timeout=2.0) as client:
                response = await client.get(self._url("/metrics"))
            response.raise_for_status()
            return response.text
        except httpx.HTTPError as exc:
            raise MotorUnavailable("Motor no disponible") from exc
