import os
import sys

from fastapi.testclient import TestClient

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from app.main import app


client = TestClient(app)


def valid_payload():
    return {
        "board": [4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0],
        "side": 0,
        "algo": "alphabeta",
        "depth": 8,
        "threads": 4,
    }


def test_healthz():
    response = client.get("/healthz")
    assert response.status_code == 200
    assert response.json() == {"status": "ok"}


def test_readyz():
    response = client.get("/readyz")
    assert response.status_code == 200
    assert response.json() == {"status": "ready"}


def test_metrics():
    response = client.get("/metrics")
    assert response.status_code == 200
    assert response.json() == {}


def test_move_mock_response():
    response = client.post("/move", json=valid_payload())
    assert response.status_code == 200
    assert response.json() == {
        "move": 3,
        "evaluation": 7.0,
        "elapsed_ms": 124,
        "stats": {
            "algo": "alphabeta",
            "nodes": 1845210,
            "prunes": 312083,
        },
        "threads_used": 4,
    }


def test_move_board_sum_validation():
    payload = valid_payload()
    payload["board"] = [4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 1]
    response = client.post("/move", json=payload)
    assert response.status_code == 422

