from __future__ import annotations

from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field, field_validator, model_validator


INITIAL_BOARD = [4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0]


def _normalize_algorithm(value: Any) -> str:
    normalized = str(value or "alphabeta").strip().lower()
    aliases = {
        "ab": "alphabeta",
        "alpha-beta": "alphabeta",
        "alpha_beta": "alphabeta",
        "alphabet": "alphabeta",
        "alfa-beta": "alphabeta",
        "alfabeta": "alphabeta",
    }
    return aliases.get(normalized, normalized)


class MoveRequest(BaseModel):
    """Public contract accepted by the backend.

    The canonical API uses Spanish field names requested for phase 3. The
    normalizer also accepts the phase 2 English names so older tests and curl
    snippets continue to work while the UI moves to the new contract.
    """

    model_config = ConfigDict(extra="ignore")

    tablero: list[int] = Field(min_length=14, max_length=14)
    lado: int = Field(ge=0, le=1)
    algoritmo: Literal["alphabeta", "mcts"] = "alphabeta"
    parametros: dict[str, Any] = Field(default_factory=dict)

    @model_validator(mode="before")
    @classmethod
    def accept_legacy_names(cls, data: Any) -> Any:
        if not isinstance(data, dict):
            return data

        normalized = dict(data)
        if "tablero" not in normalized and "board" in normalized:
            normalized["tablero"] = normalized["board"]
        if "lado" not in normalized and "side" in normalized:
            normalized["lado"] = normalized["side"]
        if "algoritmo" not in normalized and "algo" in normalized:
            normalized["algoritmo"] = normalized["algo"]

        params = dict(normalized.get("parametros") or {})
        if "profundidad" not in params and "depth" in normalized:
            params["profundidad"] = normalized["depth"]
        if "simulaciones" not in params and "simulations" in normalized:
            params["simulaciones"] = normalized["simulations"]
        if "hilos" not in params and "threads" in normalized:
            params["hilos"] = normalized["threads"]
        if "movimiento" not in params and "move" in normalized:
            params["movimiento"] = normalized["move"]
        normalized["parametros"] = params
        return normalized

    @field_validator("tablero")
    @classmethod
    def validate_board(cls, board: list[int]) -> list[int]:
        if any(not isinstance(value, int) or value < 0 for value in board):
            raise ValueError("tablero debe contener 14 enteros no negativos")
        if sum(board) != 48:
            raise ValueError("el total de semillas del tablero debe ser 48")
        return board

    @field_validator("algoritmo", mode="before")
    @classmethod
    def normalize_algorithm(cls, value: Any) -> str:
        return _normalize_algorithm(value)

    @model_validator(mode="after")
    def validate_params(self) -> "MoveRequest":
        params = dict(self.parametros)

        manual_move = params.get("movimiento")
        if manual_move is not None:
            if not isinstance(manual_move, int) or manual_move < 0 or manual_move > 13:
                raise ValueError("parametros.movimiento debe estar entre 0 y 13")

        if self.algoritmo == "alphabeta":
            depth = params.get("profundidad")
            if depth is None:
                if manual_move is None:
                    raise ValueError("parametros.profundidad es requerido para Alpha-Beta")
                depth = 1
                params["profundidad"] = depth
            if not isinstance(depth, int) or depth < 1 or depth > 32:
                raise ValueError("parametros.profundidad debe estar entre 1 y 32")
        else:
            simulations = params.get("simulaciones")
            if simulations is None:
                if manual_move is None:
                    raise ValueError("parametros.simulaciones es requerido para MCTS")
                simulations = 1
                params["simulaciones"] = simulations
            if not isinstance(simulations, int) or simulations < 1 or simulations > 100000:
                raise ValueError("parametros.simulaciones debe estar entre 1 y 100000")

        threads = params.get("hilos", 1)
        if not isinstance(threads, int) or threads < 1 or threads > 64:
            raise ValueError("parametros.hilos debe estar entre 1 y 64")
        params["hilos"] = threads
        self.parametros = params
        return self

    @property
    def profundidad(self) -> int | None:
        value = self.parametros.get("profundidad")
        return int(value) if value is not None else None

    @property
    def simulaciones(self) -> int | None:
        value = self.parametros.get("simulaciones")
        return int(value) if value is not None else None

    @property
    def hilos(self) -> int:
        return int(self.parametros.get("hilos", 1))

    @property
    def movimiento(self) -> int | None:
        value = self.parametros.get("movimiento")
        return int(value) if value is not None else None

    def motor_payload(self) -> dict[str, Any]:
        payload = {
            "tablero": self.tablero,
            "lado": self.lado,
            "algoritmo": self.algoritmo,
            "parametros": {
                "hilos": self.hilos,
            },
        }
        if self.algoritmo == "mcts":
            payload["parametros"]["simulaciones"] = self.simulaciones
        else:
            payload["parametros"]["profundidad"] = self.profundidad
        return payload

    def legacy_motor_payload(self) -> dict[str, Any]:
        payload = {
            "board": self.tablero,
            "side": self.lado,
            "algo": self.algoritmo,
            "threads": self.hilos,
        }
        if self.algoritmo == "mcts":
            payload["simulations"] = self.simulaciones
        else:
            payload["depth"] = self.profundidad
        return payload


class MoveResponse(BaseModel):
    movimiento: int
    tablero_nuevo: list[int] = Field(min_length=14, max_length=14)
    lado_siguiente: int = Field(ge=0, le=1)
    terminal: bool
    ganador: int | None = Field(default=None, ge=-1, le=1)
    estadisticas: dict[str, Any]


class ResetResponse(BaseModel):
    tablero: list[int] = Field(default_factory=lambda: INITIAL_BOARD.copy())
    lado_siguiente: int = 0
    terminal: bool = False
    ganador: int | None = None
