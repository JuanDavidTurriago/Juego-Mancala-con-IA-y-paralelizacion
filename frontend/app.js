const defaultApiUrl =
  window.location.hostname === "localhost" || window.location.hostname === "127.0.0.1"
    ? "http://localhost:8000"
    : `${window.location.protocol}//api.${window.location.hostname}`;
const API_URL = (window.API_URL || defaultApiUrl).replace(/\/+$/, "");

const boardInput = document.getElementById("board");
const statusEl = document.getElementById("status");
const algoSelect = document.getElementById("algo");
const depthGroup = document.getElementById("depthGroup");
const simulationsGroup = document.getElementById("simulationsGroup");
const hintEl = document.getElementById("hint");

let lastBoard = [4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0];
let selectedPitIndex = null;

function parseBoard() {
  const value = JSON.parse(boardInput.value);
  if (!Array.isArray(value) || value.length !== 14) {
    throw new Error("board must contain 14 integers");
  }
  if (value.some((item) => !Number.isInteger(item) || item < 0)) {
    throw new Error("board values must be non-negative integers");
  }
  return value;
}

function activeSide() {
  return Number(document.getElementById("side").value);
}

function renderPit(index, seeds) {
  const pit = document.createElement("button");
  pit.className = "pit";
  pit.type = "button";
  pit.dataset.index = String(index);
  pit.innerHTML = `<span>${index}</span><strong>${seeds}</strong>`;
  const side = index <= 5 ? 0 : index >= 7 && index <= 12 ? 1 : null;
  const isActivePit = side !== null && side === activeSide();
  if (isActivePit) {
    pit.classList.add("is-active");
    pit.addEventListener("click", () => {
      document.getElementById("side").value = String(side);
      selectedPitIndex = index;
      renderBoard(lastBoard);
      requestMove();
    });
  } else {
    pit.disabled = true;
  }
  if (selectedPitIndex === index) {
    pit.classList.add("is-selected");
  }
  return pit;
}

function renderBoard(board) {
  const top = document.getElementById("topRow");
  const bottom = document.getElementById("bottomRow");
  top.innerHTML = "";
  bottom.innerHTML = "";
  document.getElementById("store0").textContent = board[6];
  document.getElementById("store1").textContent = board[13];
  lastBoard = board.slice();

  for (let index = 12; index >= 7; index -= 1) {
    top.appendChild(renderPit(index, board[index]));
  }
  for (let index = 0; index <= 5; index += 1) {
    bottom.appendChild(renderPit(index, board[index]));
  }
}

function currentPayload() {
  const algo = algoSelect.value;
  const payload = {
    board: parseBoard(),
    side: activeSide(),
    algo,
    threads: Number(document.getElementById("threads").value),
  };
  if (algo === "mcts") {
    payload.simulations = Number(document.getElementById("simulations").value);
  } else {
    payload.depth = Number(document.getElementById("depth").value);
  }
  return payload;
}

async function ping() {
  try {
    const response = await fetch(`${API_URL}/readyz`);
    statusEl.textContent = response.ok ? "API: ready" : "API: waiting";
    statusEl.classList.toggle("is-ok", response.ok);
  } catch {
    statusEl.textContent = "API: offline";
    statusEl.classList.remove("is-ok");
  }
}

function renderResult(data) {
  document.getElementById("resultAlgo").textContent = data.algo ?? "-";
  document.getElementById("resultMove").textContent = data.move ?? "-";
  document.getElementById("resultEvaluation").textContent = data.evaluation ?? "-";
  document.getElementById("resultElapsed").textContent = `${data.elapsed_ms ?? "-"} ms`;
  if (data.stats?.algo === "mcts") {
    document.getElementById("resultStats").textContent = `rollouts ${data.stats.rollouts} | win_rate ${(data.stats.win_rate * 100).toFixed(1)}%`;
  } else if (data.stats?.algo === "alphabeta") {
    document.getElementById("resultStats").textContent = `nodes ${data.stats.nodes} | prunes ${data.stats.prunes}`;
  } else {
    document.getElementById("resultStats").textContent = "-";
  }
  document.getElementById("resultThreads").textContent = data.threads_used ?? "-";
}

async function requestMove() {
  const out = document.getElementById("out");
  out.textContent = "Procesando...";
  try {
    const payload = currentPayload();
    renderBoard(payload.board);
    const response = await fetch(`${API_URL}/move`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    const data = await response.json().catch(() => ({}));
    if (!response.ok) {
      throw new Error(data.detail || `HTTP ${response.status}`);
    }
    renderResult(data);
    out.textContent = JSON.stringify(data, null, 2);
  } catch (error) {
    out.textContent = `Error: ${error.message}`;
  }
}

boardInput.addEventListener("input", () => {
  try {
    renderBoard(parseBoard());
    hintEl.textContent = "Tablero actualizado. Haz clic en un hoyo activo o pulsa Calcular jugada.";
  } catch {
    // Keep the last valid board visible while the user edits JSON.
    hintEl.textContent = "El JSON del tablero es inválido.";
  }
});
algoSelect.addEventListener("change", () => {
  const isMcts = algoSelect.value === "mcts";
  depthGroup.classList.toggle("hidden", isMcts);
  simulationsGroup.classList.toggle("hidden", !isMcts);
});
document.getElementById("btnMove").addEventListener("click", requestMove);
document.getElementById("side").addEventListener("change", () => {
  selectedPitIndex = null;
  renderBoard(lastBoard);
});

renderBoard(JSON.parse(boardInput.value));
algoSelect.dispatchEvent(new Event("change"));
ping();
setInterval(ping, 3000);
