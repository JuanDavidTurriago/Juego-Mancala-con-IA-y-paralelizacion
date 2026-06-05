from __future__ import annotations

from dataclasses import dataclass

from .schemas import INITIAL_BOARD

PLAYER0_PITS = range(0, 6)
PLAYER1_PITS = range(7, 13)
PLAYER0_STORE = 6
PLAYER1_STORE = 13


class InvalidMove(ValueError):
    """Raised when a requested Kalah move is illegal for the current board."""


@dataclass(frozen=True)
class AppliedMove:
    movimiento: int
    tablero_nuevo: list[int]
    lado_siguiente: int
    terminal: bool
    ganador: int | None
    turno_extra: bool


def initial_board() -> list[int]:
    return INITIAL_BOARD.copy()


def store_index(side: int) -> int:
    return PLAYER0_STORE if side == 0 else PLAYER1_STORE


def opponent_store_index(side: int) -> int:
    return PLAYER1_STORE if side == 0 else PLAYER0_STORE


def own_pits(side: int) -> range:
    return PLAYER0_PITS if side == 0 else PLAYER1_PITS


def is_player_pit(side: int, pit: int) -> bool:
    return pit in own_pits(side)


def opposite_pit(pit: int) -> int:
    return 12 - pit


def legal_moves(board: list[int], side: int) -> list[int]:
    return [pit for pit in own_pits(side) if board[pit] > 0]


def side_empty(board: list[int], side: int) -> bool:
    return sum(board[pit] for pit in own_pits(side)) == 0


def sweep_remaining_seeds(board: list[int]) -> None:
    if side_empty(board, 0):
        for pit in PLAYER1_PITS:
            board[PLAYER1_STORE] += board[pit]
            board[pit] = 0
    if side_empty(board, 1):
        for pit in PLAYER0_PITS:
            board[PLAYER0_STORE] += board[pit]
            board[pit] = 0


def terminal_winner(board: list[int]) -> int | None:
    if not (side_empty(board, 0) or side_empty(board, 1)):
        return None

    final_board = board.copy()
    sweep_remaining_seeds(final_board)
    if final_board[PLAYER0_STORE] > final_board[PLAYER1_STORE]:
        return 0
    if final_board[PLAYER1_STORE] > final_board[PLAYER0_STORE]:
        return 1
    return -1


def board_score(board: list[int], side: int) -> int:
    own_store = board[store_index(side)]
    opp_store = board[opponent_store_index(side)]
    own_side = sum(board[pit] for pit in own_pits(side))
    opp_side = sum(board[pit] for pit in own_pits(1 - side))
    return (own_store - opp_store) + int(0.5 * (own_side - opp_side))


def apply_move(board: list[int], side: int, pit: int) -> AppliedMove:
    if side not in (0, 1):
        raise InvalidMove("lado invalido")
    if len(board) != 14:
        raise InvalidMove("el tablero debe tener 14 posiciones")
    if sum(board) != 48:
        raise InvalidMove("el total de semillas debe ser 48")
    if not is_player_pit(side, pit):
        raise InvalidMove("el hoyo no pertenece al jugador activo")
    if board[pit] <= 0:
        raise InvalidMove("el hoyo seleccionado esta vacio")

    next_board = board.copy()
    seeds = next_board[pit]
    next_board[pit] = 0
    current = pit

    while seeds > 0:
        current = (current + 1) % 14
        if current == opponent_store_index(side):
            continue
        next_board[current] += 1
        seeds -= 1

    extra_turn = current == store_index(side)
    landed_in_empty_own_pit = (
        not extra_turn
        and is_player_pit(side, current)
        and board[current] == 0
        and next_board[current] == 1
    )
    if landed_in_empty_own_pit:
        opposite = opposite_pit(current)
        if next_board[opposite] > 0:
            captured = next_board[current] + next_board[opposite]
            next_board[current] = 0
            next_board[opposite] = 0
            next_board[store_index(side)] += captured

    lado_siguiente = side if extra_turn else 1 - side
    terminal = side_empty(next_board, 0) or side_empty(next_board, 1)
    ganador = None
    if terminal:
        sweep_remaining_seeds(next_board)
        if next_board[PLAYER0_STORE] > next_board[PLAYER1_STORE]:
            ganador = 0
        elif next_board[PLAYER1_STORE] > next_board[PLAYER0_STORE]:
            ganador = 1
        else:
            ganador = -1

    return AppliedMove(
        movimiento=pit,
        tablero_nuevo=next_board,
        lado_siguiente=lado_siguiente,
        terminal=terminal,
        ganador=ganador,
        turno_extra=extra_turn,
    )
