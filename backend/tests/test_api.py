import os
import sys

from fastapi.testclient import TestClient

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from app import main
from app.schemas import MoveResponse


class FakeMotor:
    def __init__(self, ready=True):
        self.ready = ready

    def is_ready(self):
        return self.ready

    def move(self, req):
        return MoveResponse(
            move=3,
            evaluation=7,
            elapsed_ms=12,
            stats={"nodes": 100, "prunes": 10},
            threads_used=req.threads,
        )

    def metrics(self):
        return "mancala_motor_search_nodes_total 100\n"


client = TestClient(main.app)


def valid_payload():
    return {
        "board": [4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0],
        "side": 0,
        "depth": 8,
        "threads": 4,
    }


def test_healthz_ok():
    response = client.get("/healthz")
    assert response.status_code == 200
    assert response.json() == {"status": "ok"}


def test_readyz_ok(monkeypatch):
    monkeypatch.setattr(main, "motor", FakeMotor(ready=True))
    response = client.get("/readyz")
    assert response.status_code == 200
    assert response.json() == {"status": "ready"}


def test_readyz_unavailable(monkeypatch):
    monkeypatch.setattr(main, "motor", FakeMotor(ready=False))
    response = client.get("/readyz")
    assert response.status_code == 503


def test_move_delegates_to_motor(monkeypatch):
    monkeypatch.setattr(main, "motor", FakeMotor(ready=True))
    response = client.post("/move", json=valid_payload())
    assert response.status_code == 200
    assert response.json()["move"] == 3
    assert response.json()["stats"] == {"nodes": 100, "prunes": 10}
    assert response.json()["threads_used"] == 4


def test_move_rejects_invalid_board():
    payload = valid_payload()
    payload["board"] = payload["board"][:-1]
    response = client.post("/move", json=payload)
    assert response.status_code == 422


def test_cors_preflight_uses_explicit_origin():
    response = client.options(
        "/move",
        headers={
            "Origin": "http://localhost:8080",
            "Access-Control-Request-Method": "POST",
        },
    )
    assert response.status_code == 200
    assert response.headers["access-control-allow-origin"] == "http://localhost:8080"


def test_metrics_includes_backend_and_motor(monkeypatch):
    monkeypatch.setattr(main, "motor", FakeMotor(ready=True))
    response = client.get("/metrics")
    assert response.status_code == 200
    assert "mancala_backend_requests_total" in response.text
    assert "mancala_motor_search_nodes_total 100" in response.text
