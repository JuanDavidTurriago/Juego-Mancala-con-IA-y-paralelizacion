from __future__ import annotations

from typing import Annotated

from pydantic import BaseModel, Field, field_validator


class MoveRequest(BaseModel):
    board: list[Annotated[int, Field(ge=0)]] = Field(min_length=14, max_length=14)
    side: Annotated[int, Field(ge=0, le=1)]
    depth: Annotated[int, Field(ge=1, le=32)] = 8
    threads: Annotated[int, Field(ge=1, le=64)] = 1

    @field_validator("board")
    @classmethod
    def validate_seed_total(cls, board: list[int]) -> list[int]:
        if sum(board) != 48:
            raise ValueError("board seed total must be 48")
        return board


class MoveStats(BaseModel):
    nodes: int = Field(ge=0)
    prunes: int = Field(ge=0)


class MoveResponse(BaseModel):
    move: int
    evaluation: int
    elapsed_ms: int = Field(ge=0)
    stats: MoveStats
    threads_used: int = Field(ge=1)
