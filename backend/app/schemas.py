from __future__ import annotations

from typing import Annotated, Literal

from pydantic import BaseModel, Field, field_validator, model_validator


class MoveRequest(BaseModel):
    board: list[Annotated[int, Field(ge=0)]] = Field(min_length=14, max_length=14)
    side: Annotated[int, Field(ge=0, le=1)]
    algo: Literal["alphabeta", "mcts"] = "alphabeta"
    depth: Annotated[int | None, Field(ge=1, le=32)] = None
    simulations: Annotated[int | None, Field(ge=1, le=100000)] = None
    threads: Annotated[int, Field(ge=1, le=64)] = 1

    @field_validator("board")
    @classmethod
    def validate_seed_total(cls, board: list[int]) -> list[int]:
        if sum(board) != 48:
            raise ValueError("board seed total must be 48")
        return board

    @field_validator("algo", mode="before")
    @classmethod
    def normalize_algo(cls, value: str) -> str:
        if value == "alphabet":
            return "alphabeta"
        return value

    @model_validator(mode="after")
    def validate_algorithm_fields(self) -> "MoveRequest":
        if self.algo == "alphabeta" and self.depth is None:
            raise ValueError("depth is required when algo is alphabeta")
        if self.algo == "mcts" and self.simulations is None:
            raise ValueError("simulations is required when algo is mcts")
        return self


class AlphaBetaStats(BaseModel):
    algo: Literal["alphabeta"] = "alphabeta"
    nodes: int = Field(ge=0)
    prunes: int = Field(ge=0)


class MCTSStats(BaseModel):
    algo: Literal["mcts"] = "mcts"
    rollouts: int = Field(ge=0)
    win_rate: float = Field(ge=0, le=1)


class MoveResponse(BaseModel):
    algo: Literal["alphabeta", "mcts"]
    move: int
    evaluation: int
    elapsed_ms: int = Field(ge=0)
    stats: Annotated[AlphaBetaStats | MCTSStats, Field(discriminator="algo")]
    threads_used: int = Field(ge=1)
