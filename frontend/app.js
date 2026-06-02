const defaultApiUrl =
  window.location.hostname === "localhost" || window.location.hostname === "127.0.0.1"
    ? "http://localhost:8000"
    : `${window.location.protocol}//api.${window.location.hostname}`;
const API_URL = (window.API_URL || defaultApiUrl).replace(/\/+$/, "");

const boardInput = document.getElementById("board");
const statusEl = document.getElementById("status");

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

function renderPit(index, seeds) {
  const pit = document.createElement("button");
  pit.className = "pit";
  pit.type = "button";
  pit.dataset.index = String(index);
  pit.innerHTML = `<span>${index}</span><strong>${seeds}</strong>`;
  pit.addEventListener("click", () => {
    const side = index <= 5 ? 0 : 1;
    document.getElementById("side").value = String(side);
  });
  return pit;
}

function renderBoard(board) {
  const top = document.getElementById("topRow");
  const bottom = document.getElementById("bottomRow");
  top.innerHTML = "";
  bottom.innerHTML = "";
  document.getElementById("store0").textContent = board[6];
  document.getElementById("store1").textContent = board[13];

  for (let index = 12; index >= 7; index -= 1) {
    top.appendChild(renderPit(index, board[index]));
  }
  for (let index = 0; index <= 5; index += 1) {
    bottom.appendChild(renderPit(index, board[index]));
  }
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
  document.getElementById("resultMove").textContent = data.move ?? "-";
  document.getElementById("resultEvaluation").textContent = data.evaluation ?? "-";
  document.getElementById("resultElapsed").textContent = `${data.elapsed_ms ?? "-"} ms`;
  document.getElementById("resultNodes").textContent = data.stats?.nodes ?? "-";
  document.getElementById("resultPrunes").textContent = data.stats?.prunes ?? "-";
  document.getElementById("resultThreads").textContent = data.threads_used ?? "-";
}

async function requestMove() {
  const out = document.getElementById("out");
  out.textContent = "Procesando...";
  try {
    const board = parseBoard();
    renderBoard(board);
    const payload = {
      board,
      side: Number(document.getElementById("side").value),
      depth: Number(document.getElementById("depth").value),
      threads: Number(document.getElementById("threads").value),
    };
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
  } catch {
    // Keep the last valid board visible while the user edits JSON.
  }
});
document.getElementById("btnMove").addEventListener("click", requestMove);
renderBoard(JSON.parse(boardInput.value));
ping();
setInterval(ping, 3000);
