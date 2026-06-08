const defaultApiUrl =
  window.location.hostname === "localhost" || window.location.hostname === "127.0.0.1"
    ? "http://localhost:8000"
    : `${window.location.protocol}//api.${window.location.hostname}`;
const API_URL = (window.API_URL || defaultApiUrl).replace(/\/+$/, "");

const PLAYER_HUMAN = 0;
const PLAYER_ENGINE = 1;
const INITIAL_BOARD = [4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0];
const REQUEST_TIMEOUT_MS = 12000;

const elements = {
  apiStatus: document.getElementById("apiStatus"),
  resetButton: document.getElementById("resetButton"),
  finalResetButton: document.getElementById("finalResetButton"),
  topRow: document.getElementById("topRow"),
  bottomRow: document.getElementById("bottomRow"),
  storeP1: document.getElementById("storeP1"),
  storeP2: document.getElementById("storeP2"),
  turnLabel: document.getElementById("turnLabel"),
  gameStatus: document.getElementById("gameStatus"),
  message: document.getElementById("message"),
  algorithm: document.getElementById("algorithm"),
  depth: document.getElementById("depth"),
  simulations: document.getElementById("simulations"),
  threads: document.getElementById("threads"),
  depthField: document.getElementById("depthField"),
  simulationsField: document.getElementById("simulationsField"),
  statAlgorithm: document.getElementById("statAlgorithm"),
  statPrimaryLabel: document.getElementById("statPrimaryLabel"),
  statPrimary: document.getElementById("statPrimary"),
  statTime: document.getElementById("statTime"),
  statNodesLabel: document.getElementById("statNodesLabel"),
  statNodes: document.getElementById("statNodes"),
  statSecondaryLabel: document.getElementById("statSecondaryLabel"),
  statSecondary: document.getElementById("statSecondary"),
  statMove: document.getElementById("statMove"),
  finalPanel: document.getElementById("finalPanel"),
  finalText: document.getElementById("finalText"),
};

const state = {
  board: INITIAL_BOARD.slice(),
  sideToMove: PLAYER_HUMAN,
  busy: false,
  terminal: false,
  winner: null,
  lastMove: null,
  lastMoveOwner: null,
};

function algorithmLabel(value = elements.algorithm.value) {
  return value === "mcts" ? "MCTS" : "Alpha-Beta";
}

function currentParams(extra = {}) {
  const algorithm = elements.algorithm.value;
  const params = {
    hilos: Number(elements.threads.value || 1),
    ...extra,
  };

  if (algorithm === "mcts") {
    params.simulaciones = Number(elements.simulations.value || 1);
  } else {
    params.profundidad = Number(elements.depth.value || 1);
  }

  return {
    tablero: state.board,
    lado: state.sideToMove,
    algoritmo: algorithm,
    parametros: params,
  };
}

function setBusy(isBusy, label = "") {
  state.busy = isBusy;
  elements.gameStatus.textContent = label || (state.terminal ? "Finalizada" : "Listo");
  elements.algorithm.disabled = isBusy;
  elements.depth.disabled = isBusy;
  elements.simulations.disabled = isBusy;
  elements.threads.disabled = isBusy;
  elements.resetButton.disabled = isBusy;
  elements.finalResetButton.disabled = isBusy;
  renderBoard();
}

async function apiFetch(path, options = {}) {
  const controller = new AbortController();
  const timeoutId = window.setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS);

  try {
    const response = await fetch(`${API_URL}${path}`, {
      ...options,
      signal: controller.signal,
      headers: {
        "Content-Type": "application/json",
        ...(options.headers || {}),
      },
    });
    const data = await response.json().catch(() => ({}));
    if (!response.ok) {
      const detail = data.detail || data.error || `HTTP ${response.status}`;
      throw new Error(detail);
    }
    return data;
  } catch (error) {
    if (error.name === "AbortError") {
      throw new Error("El motor está tardando demasiado");
    }
    throw error;
  } finally {
    window.clearTimeout(timeoutId);
  }
}

function friendlyError(error) {
  const message = String(error.message || error);
  if (message.includes("Failed to fetch") || message.includes("NetworkError")) {
    return "No se pudo conectar con el backend";
  }
  return message;
}

function pitOwner(index) {
  if (index >= 0 && index <= 5) return PLAYER_HUMAN;
  if (index >= 7 && index <= 12) return PLAYER_ENGINE;
  return null;
}

function canClickPit(index) {
  return (
    !state.busy &&
    !state.terminal &&
    state.sideToMove === PLAYER_HUMAN &&
    pitOwner(index) === PLAYER_HUMAN &&
    state.board[index] > 0
  );
}

function seedDots(count) {
  const visible = Math.min(count, 18);
  return Array.from({ length: visible }, () => "<i></i>").join("");
}

function renderPit(index) {
  const button = document.createElement("button");
  button.type = "button";
  button.className = "pit";
  button.dataset.index = String(index);
  button.disabled = !canClickPit(index);

  const owner = pitOwner(index);
  if (owner === state.sideToMove && !state.terminal) {
    button.classList.add("is-turn");
  }
  if (state.lastMove === index) {
    button.classList.add(state.lastMoveOwner === PLAYER_ENGINE ? "is-engine-move" : "is-human-move");
  }

  // Número visual 1-6 desde la perspectiva del jugador (no el índice interno)
  const pitLabel = owner === PLAYER_HUMAN ? index + 1 : 13 - index;

  button.innerHTML = `
    <span class="pit-index" aria-label="Hoyo ${pitLabel}">${pitLabel}</span>
    <strong>${state.board[index]}</strong>
    <span class="seed-dots" aria-hidden="true">${seedDots(state.board[index])}</span>
  `;

  button.addEventListener("click", () => handleHumanMove(index));
  return button;
}

function renderBoard() {
  elements.topRow.innerHTML = "";
  elements.bottomRow.innerHTML = "";

  for (let index = 12; index >= 7; index -= 1) {
    elements.topRow.appendChild(renderPit(index));
  }
  for (let index = 0; index <= 5; index += 1) {
    elements.bottomRow.appendChild(renderPit(index));
  }

  elements.storeP1.textContent = state.board[6];
  elements.storeP2.textContent = state.board[13];
  elements.turnLabel.textContent = state.sideToMove === PLAYER_HUMAN ? "Tú (Jugador 1)" : "IA (Jugador 2)";
}

function renderStats(data = null) {
  const selectedAlgorithm = elements.algorithm.value;
  elements.statAlgorithm.textContent = algorithmLabel(selectedAlgorithm);

  if (!data) {
    if (selectedAlgorithm === "mcts") {
      elements.statPrimaryLabel.textContent = "Simulaciones";
      elements.statPrimary.textContent = elements.simulations.value;
      elements.statNodesLabel.textContent = "Rollouts";
      elements.statNodes.textContent = "0";
      elements.statSecondaryLabel.textContent = "Win rate";
      elements.statSecondary.textContent = "0%";
    } else {
      elements.statPrimaryLabel.textContent = "Profundidad";
      elements.statPrimary.textContent = elements.depth.value;
      elements.statNodesLabel.textContent = "Nodos";
      elements.statNodes.textContent = "0";
      elements.statSecondaryLabel.textContent = "Podas";
      elements.statSecondary.textContent = "0";
    }
    elements.statTime.textContent = "0 ms";
    elements.statMove.textContent = state.lastMove ?? "-";
    return;
  }

  const stats = data.estadisticas || {};
  const algorithm = stats.algoritmo || selectedAlgorithm;
  elements.statAlgorithm.textContent = algorithm === "mcts" ? "MCTS" : algorithm === "humano" ? "Humano" : "Alpha-Beta";
  elements.statTime.textContent = `${stats.tiempo_ms ?? 0} ms`;
  elements.statMove.textContent = data.movimiento ?? "-";

  if (algorithm === "mcts") {
    elements.statPrimaryLabel.textContent = "Simulaciones";
    elements.statPrimary.textContent = stats.simulaciones ?? elements.simulations.value;
    elements.statNodesLabel.textContent = "Rollouts";
    elements.statNodes.textContent = stats.rollouts ?? 0;
    elements.statSecondaryLabel.textContent = "Win rate";
    elements.statSecondary.textContent = `${((stats.win_rate ?? 0) * 100).toFixed(1)}%`;
  } else {
    elements.statPrimaryLabel.textContent = "Profundidad";
    elements.statPrimary.textContent = stats.profundidad ?? elements.depth.value;
    elements.statNodesLabel.textContent = "Nodos";
    elements.statNodes.textContent = stats.nodos ?? 0;
    elements.statSecondaryLabel.textContent = "Podas";
    elements.statSecondary.textContent = stats.podas ?? 0;
  }
}

function renderFinalState() {
  if (!state.terminal) {
    elements.finalPanel.classList.add("hidden");
    elements.finalText.textContent = "";
    return;
  }

  const p1Score = state.board[6];
  const p2Score = state.board[13];
  let winnerText = "La partida terminó en empate";
  if (state.winner === PLAYER_HUMAN) {
    winnerText = "¡Jugador 1 ha ganado!";
  } else if (state.winner === PLAYER_ENGINE) {
    winnerText = "¡Jugador 2 ha ganado!";
  }

  elements.finalText.textContent = `${winnerText} Puntaje final: Jugador 1 ${p1Score} - Jugador 2 ${p2Score}.`;
  elements.finalPanel.classList.remove("hidden");
  elements.message.textContent = elements.finalText.textContent;
}

function applyBackendMove(data, owner) {
  state.board = data.tablero_nuevo.slice();
  state.sideToMove = data.lado_siguiente;
  state.terminal = Boolean(data.terminal);
  state.winner = data.ganador;
  state.lastMove = data.movimiento;
  state.lastMoveOwner = owner;

  renderBoard();
  renderStats(data);
  renderFinalState();
}

async function handleHumanMove(index) {
  if (!canClickPit(index)) return;

  elements.message.textContent = "";
  setBusy(true, "Aplicando jugada");

  try {
    const payload = currentParams({ movimiento: index });
    const data = await apiFetch("/move", {
      method: "POST",
      body: JSON.stringify(payload),
    });
    applyBackendMove(data, PLAYER_HUMAN);

    if (state.terminal) {
      setBusy(false, "Finalizada");
      return;
    }

    if (state.sideToMove === PLAYER_ENGINE) {
      await requestEngineMove();
    } else {
      elements.message.textContent = "Turno extra para Jugador 1";
      setBusy(false, "Turno humano");
    }
  } catch (error) {
    elements.message.textContent = friendlyError(error);
    setBusy(false, "Error");
  }
}

async function requestEngineMove() {
  let keepPlaying = true;

  while (keepPlaying && !state.terminal) {
    setBusy(true, "Motor pensando");
    const payload = currentParams();
    const data = await apiFetch("/move", {
      method: "POST",
      body: JSON.stringify(payload),
    });
    applyBackendMove(data, PLAYER_ENGINE);

    keepPlaying = !state.terminal && state.sideToMove === PLAYER_ENGINE;
    if (keepPlaying) {
      elements.message.textContent = "Turno extra para Jugador 2";
      await new Promise((resolve) => window.setTimeout(resolve, 450));
    }
  }

  if (!state.terminal) {
    elements.message.textContent = "Turno de Jugador 1";
    setBusy(false, "Turno humano");
  } else {
    setBusy(false, "Finalizada");
  }
}

async function resetGame() {
  setBusy(true, "Reiniciando");
  try {
    const data = await apiFetch("/reset", { method: "POST", body: "{}" });
    state.board = (data.tablero || INITIAL_BOARD).slice();
    state.sideToMove = data.lado_siguiente ?? PLAYER_HUMAN;
    state.terminal = Boolean(data.terminal);
    state.winner = data.ganador ?? null;
    state.lastMove = null;
    state.lastMoveOwner = null;
    elements.message.textContent = "";
    renderBoard();
    renderStats();
    renderFinalState();
    setBusy(false, "Turno humano");
  } catch (error) {
    state.board = INITIAL_BOARD.slice();
    state.sideToMove = PLAYER_HUMAN;
    state.terminal = false;
    state.winner = null;
    state.lastMove = null;
    state.lastMoveOwner = null;
    elements.message.textContent = friendlyError(error);
    renderBoard();
    renderStats();
    renderFinalState();
    setBusy(false, "Turno humano");
  }
}

async function ping() {
  try {
    const response = await fetch(`${API_URL}/healthz`);
    elements.apiStatus.textContent = response.ok ? "API: conectada" : "API: esperando";
    elements.apiStatus.classList.toggle("is-ok", response.ok);
  } catch {
    elements.apiStatus.textContent = "API: sin conexión";
    elements.apiStatus.classList.remove("is-ok");
  }
}

function syncAlgorithmFields() {
  const isMcts = elements.algorithm.value === "mcts";
  elements.depthField.classList.toggle("hidden", isMcts);
  elements.simulationsField.classList.toggle("hidden", !isMcts);
  renderStats();
}

elements.algorithm.addEventListener("change", syncAlgorithmFields);
elements.depth.addEventListener("input", renderStats);
elements.simulations.addEventListener("input", renderStats);
elements.threads.addEventListener("input", renderStats);
elements.resetButton.addEventListener("click", resetGame);
elements.finalResetButton.addEventListener("click", resetGame);

renderBoard();
renderStats();
syncAlgorithmFields();
ping();
window.setInterval(ping, 4000);
