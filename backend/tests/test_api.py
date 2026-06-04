import os
import sys

import httpx
import pytest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from app import main
from app.schemas import MoveResponse


class FakeMotor:
    def __init__(self, ready=True):
        self.ready = ready

    async def is_ready(self):
        return self.ready

    async def call_motor(self, req):
        if req.algo == "mcts":
            return {
                "algo": "mcts",
                "move": 3,
                "evaluation": 7,
                "elapsed_ms": 12,
                "stats": {"algo": "mcts", "rollouts": req.simulations or 1, "win_rate": 0.64},
                "threads_used": req.threads,
            }
        return MoveResponse(
            algo="alphabeta",
            move=3,
            evaluation=7,
            elapsed_ms=12,
            stats={"algo": "alphabeta", "nodes": 100, "prunes": 10},
            threads_used=req.threads,
        )

    async def metrics(self):
        return "mancala_motor_search_nodes_total 100\n"


def valid_payload():
    return {
        "board": [4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0],
        "side": 0,
        "algo": "alphabeta",
        "depth": 8,
        "threads": 4,
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


@pytest.mark.anyio
async def test_move_delegates_to_motor(monkeypatch):
    monkeypatch.setattr(main, "motor", FakeMotor(ready=True))
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.post("/move", json=valid_payload())
    assert response.status_code == 200
    body = response.json()
    assert body["algo"] == "alphabeta"
    assert body["move"] == 3
    assert body["stats"] == {"algo": "alphabeta", "nodes": 100, "prunes": 10}
    assert body["threads_used"] == 4


@pytest.mark.anyio
async def test_move_rejects_invalid_board():
    payload = valid_payload()
    payload["board"] = payload["board"][:-1]
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.post("/move", json=payload)
    assert response.status_code == 422


@pytest.mark.anyio
async def test_move_rejects_invalid_seed_total():
    payload = valid_payload()
    payload["board"] = [3, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0]
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.post("/move", json=payload)
    assert response.status_code == 422


@pytest.mark.anyio
async def test_move_rejects_missing_depth_when_alphabeta():
    payload = valid_payload()
    payload.pop("depth")
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.post("/move", json=payload)
    assert response.status_code == 422


@pytest.mark.anyio
async def test_move_supports_mcts_payload(monkeypatch):
    monkeypatch.setattr(main, "motor", FakeMotor(ready=True))
    payload = valid_payload()
    payload["algo"] = "mcts"
    payload.pop("depth")
    payload["simulations"] = 1000
    transport = httpx.ASGITransport(app=main.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.post("/move", json=payload)
    assert response.status_code == 200
    body = response.json()
    assert body["algo"] == "mcts"
    assert body["stats"]["algo"] == "mcts"
    assert body["stats"]["rollouts"] == 1000


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
