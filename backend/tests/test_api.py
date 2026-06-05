import os
import sys

import httpx
import pytest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from app import main
from app.motor_client import MotorClientError
from app.schemas import MoveResponse


class FakeMotor:
    def __init__(self, ready=True, error=None):
        self.ready = ready
        self.error = error

    async def is_ready(self):
        return self.ready

    async def call_motor(self, req):
        if self.error:
            raise self.error
        return MoveResponse(
            movimiento=7,
            tablero_nuevo=[4, 4, 4, 4, 4, 4, 0, 0, 5, 5, 5, 5, 4, 0],
            lado_siguiente=0,
            terminal=False,
            ganador=None,
            estadisticas={
                "algoritmo": req.algoritmo,
                "profundidad": req.profundidad,
                "nodos": 100,
                "podas": 10,
                "tiempo_ms": 12,
                "hilos": req.hilos,
            },
        )

    async def metrics(self):
        return "mancala_motor_search_nodes_total 100\n"


def valid_payload():
    return {
        "tablero": [4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0],
        "lado": 1,
        "algoritmo": "alphabeta",
        "parametros": {"profundidad": 6, "hilos": 4},
    }


@pytest.mark.anyio
async def test_healthz_ok():
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.get("/healthz")
    assert response.status_code == 200
    assert response.json() == {"status": "ok"}


@pytest.mark.anyio
async def test_readyz_ok(monkeypatch):
    monkeypatch.setattr(main, "motor", FakeMotor(ready=True))
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.get("/readyz")
    assert response.status_code == 200
    assert response.json() == {"status": "ready"}


@pytest.mark.anyio
async def test_readyz_unavailable(monkeypatch):
    monkeypatch.setattr(main, "motor", FakeMotor(ready=False))
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.get("/readyz")
    assert response.status_code == 503
    assert response.json()["detail"] == "Motor no disponible"


@pytest.mark.anyio
async def test_reset_returns_initial_board():
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.post("/reset")
    assert response.status_code == 200
    assert response.json() == {
        "tablero": [4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0],
        "lado_siguiente": 0,
        "terminal": False,
        "ganador": None,
    }


@pytest.mark.anyio
async def test_move_delegates_to_motor(monkeypatch):
    monkeypatch.setattr(main, "motor", FakeMotor(ready=True))
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.post("/move", json=valid_payload())
    assert response.status_code == 200
    body = response.json()
    assert body["movimiento"] == 7
    assert body["tablero_nuevo"][7] == 0
    assert body["lado_siguiente"] == 0
    assert body["estadisticas"]["nodos"] == 100
    assert body["estadisticas"]["hilos"] == 4


@pytest.mark.anyio
async def test_move_accepts_legacy_payload(monkeypatch):
    monkeypatch.setattr(main, "motor", FakeMotor(ready=True))
    payload = {
        "board": [4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0],
        "side": 1,
        "algo": "alphabeta",
        "depth": 6,
        "threads": 2,
    }
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.post("/move", json=payload)
    assert response.status_code == 200
    assert response.json()["estadisticas"]["hilos"] == 2


@pytest.mark.anyio
async def test_move_applies_manual_human_move():
    payload = valid_payload()
    payload["lado"] = 0
    payload["parametros"]["movimiento"] = 2
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.post("/move", json=payload)
    assert response.status_code == 200
    body = response.json()
    assert body["movimiento"] == 2
    assert body["tablero_nuevo"] == [4, 4, 0, 5, 5, 5, 1, 4, 4, 4, 4, 4, 4, 0]
    assert body["lado_siguiente"] == 0
    assert body["estadisticas"]["algoritmo"] == "humano"


@pytest.mark.anyio
async def test_move_rejects_invalid_manual_move():
    payload = valid_payload()
    payload["lado"] = 0
    payload["parametros"]["movimiento"] = 7
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.post("/move", json=payload)
    assert response.status_code == 400
    assert "no pertenece" in response.json()["detail"]


@pytest.mark.anyio
async def test_move_rejects_invalid_board():
    payload = valid_payload()
    payload["tablero"] = payload["tablero"][:-1]
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.post("/move", json=payload)
    assert response.status_code == 422


@pytest.mark.anyio
async def test_move_rejects_invalid_seed_total():
    payload = valid_payload()
    payload["tablero"] = [3, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0]
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.post("/move", json=payload)
    assert response.status_code == 422


@pytest.mark.anyio
async def test_move_rejects_missing_depth_when_alphabeta():
    payload = valid_payload()
    payload["parametros"].pop("profundidad")
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.post("/move", json=payload)
    assert response.status_code == 422


@pytest.mark.anyio
async def test_move_supports_mcts_payload(monkeypatch):
    monkeypatch.setattr(main, "motor", FakeMotor(ready=True))
    payload = valid_payload()
    payload["algoritmo"] = "mcts"
    payload["parametros"] = {"simulaciones": 1000, "hilos": 4}
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.post("/move", json=payload)
    assert response.status_code == 200
    body = response.json()
    assert body["estadisticas"]["algoritmo"] == "mcts"


@pytest.mark.anyio
async def test_move_propagates_motor_error(monkeypatch):
    monkeypatch.setattr(main, "motor", FakeMotor(error=MotorClientError(503, "MCTS no disponible aún")))
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.post("/move", json=valid_payload())
    assert response.status_code == 503
    assert response.json()["detail"] == "MCTS no disponible aún"


@pytest.mark.anyio
async def test_cors_preflight_uses_explicit_origin():
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.options(
            "/move",
            headers={
                "Origin": "http://localhost:8080",
                "Access-Control-Request-Method": "POST",
            },
        )
    assert response.status_code == 200
    assert response.headers["access-control-allow-origin"] == "http://localhost:8080"


@pytest.mark.anyio
async def test_metrics_includes_backend_and_motor(monkeypatch):
    monkeypatch.setattr(main, "motor", FakeMotor(ready=True))
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.get("/metrics")
    assert response.status_code == 200
    assert "mancala_backend_requests_total" in response.text
    assert "mancala_motor_search_nodes_total 100" in response.text
