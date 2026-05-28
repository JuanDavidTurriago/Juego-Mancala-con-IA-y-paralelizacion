from typing import Any, Dict, List, Literal, Optional

from pydantic import BaseModel, Field, field_validator, conint


Algo = Literal["alphabeta", "mcts"]


class MoveRequest(BaseModel):
    board: List[conint(ge=0)] = Field(..., min_length=14, max_length=14)
    side: Literal[0, 1]
    algo: Algo
    depth: Optional[conint(ge=1)] = None
    simulations: Optional[conint(ge=1)] = None
    threads: conint(ge=1) = 1

    @field_validator("board")
    @classmethod
    def validate_board_sum(cls, board: List[int]) -> List[int]:
        if sum(board) != 48:
            raise ValueError("board must sum to 48")
        return board


class MoveResponse(BaseModel):
    move: int
    evaluation: float
    elapsed_ms: int
    stats: Dict[str, Any]
    threads_used: conint(ge=1)

