const API_URL = (window.API_URL || "http://localhost:8000").replace(/\/+$/, "");

function parseBoard(text) {
  const val = JSON.parse(text);
  if (!Array.isArray(val) || val.length !== 14) throw new Error("board debe tener 14 enteros");
  return val;
}

async function ping() {
  try {
    const r = await fetch(`${API_URL}/healthz`);
    document.getElementById("status").textContent = r.ok ? "API: ok" : "API: error";
  } catch {
    document.getElementById("status").textContent = "API: sin conectar";
  }
}

async function doMove() {
  const out = document.getElementById("out");
  out.textContent = "Procesando...";
  try {
    const algo = document.getElementById("algo").value;
    const depth = Number(document.getElementById("depth").value);
    const simulations = Number(document.getElementById("sims").value);
    const threads = Number(document.getElementById("threads").value);
    const side = Number(document.getElementById("side").value);
    const board = parseBoard(document.getElementById("board").value);

    const payload = { board, side, algo, threads };
    if (algo === "alphabeta") payload.depth = depth;
    if (algo === "mcts") payload.simulations = simulations;

    const r = await fetch(`${API_URL}/move`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    const data = await r.json().catch(() => ({}));
    if (!r.ok) throw new Error(data.detail || `HTTP ${r.status}`);
    out.textContent = JSON.stringify(data, null, 2);
  } catch (e) {
    out.textContent = `Error: ${e.message}`;
  }
}

document.getElementById("btnMove").addEventListener("click", doMove);
ping();
setInterval(ping, 2000);

