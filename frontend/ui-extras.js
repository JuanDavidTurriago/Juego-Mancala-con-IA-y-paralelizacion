// ─────────────────────────────────────────────────────────────────────────────
// ui-extras.js — Mejoras UX sobre la interfaz base
// Este archivo extiende la funcionalidad sin modificar app.js
// ─────────────────────────────────────────────────────────────────────────────

(function () {
  "use strict";

  // ── Modal de guía ────────────────────────────────────────────────────────
  const modal     = document.getElementById("guideModal");
  const btnHelp   = document.getElementById("helpButton");
  const btnClose  = document.getElementById("guideClose");
  const btnStart  = document.getElementById("guideStart");

  function openGuide() {
    modal.showModal();
    document.body.style.overflow = "hidden";
  }

  function closeGuide() {
    modal.close();
    document.body.style.overflow = "";
  }

  btnHelp.addEventListener("click", openGuide);
  btnClose.addEventListener("click", closeGuide);
  btnStart.addEventListener("click", closeGuide);

  // Cerrar al hacer clic en el backdrop
  modal.addEventListener("click", function (e) {
    if (e.target === modal) closeGuide();
  });

  // Cerrar con Escape
  modal.addEventListener("cancel", closeGuide);

  // Mostrar guía la primera vez que se visita
  if (!sessionStorage.getItem("guideShown")) {
    sessionStorage.setItem("guideShown", "1");
    requestAnimationFrame(openGuide);
  }

  // ── Selector de dificultad → controla input depth ──────────────────────
  const difficultySelect = document.getElementById("difficulty");
  const depthInput       = document.getElementById("depth");
  const simInput         = document.getElementById("simulations");
  const algorithmSelect  = document.getElementById("algorithm");
  const difficultyField  = document.getElementById("difficultyField");

  // Mapa dificultad para Alpha-Beta (profundidad) y MCTS (simulaciones)
  const DIFFICULTY_DEPTH = { "4": 4, "8": 8, "12": 12, "16": 16 };
  const DIFFICULTY_MCTS  = { "4": 2000, "8": 10000, "12": 50000, "16": 100000 };

  function syncDifficulty() {
    const val = difficultySelect.value;
    const algo = algorithmSelect.value;
    if (algo === "mcts") {
      simInput.value = DIFFICULTY_MCTS[val] ?? 10000;
    } else {
      depthInput.value = DIFFICULTY_DEPTH[val] ?? 8;
    }
    // Disparar el evento input para que renderStats del app.js se actualice
    depthInput.dispatchEvent(new Event("input"));
    simInput.dispatchEvent(new Event("input"));
  }

  difficultySelect.addEventListener("change", syncDifficulty);

  // Actualizar dificultad cuando cambia el algoritmo
  algorithmSelect.addEventListener("change", function () {
    syncDifficulty();
    // Mostrar/ocultar selector de dificultad siempre
    difficultyField.classList.remove("hidden");
  });

  // Inicializar
  syncDifficulty();

  // ── Toggle de parámetros avanzados ──────────────────────────────────────
  const advancedToggle  = document.getElementById("advancedToggle");
  const advancedSection = document.querySelector(".advanced-hidden");

  advancedToggle.addEventListener("click", function () {
    const isOpen = advancedSection.classList.toggle("open");
    advancedToggle.textContent = isOpen
      ? "▾ Parámetros avanzados"
      : "▸ Parámetros avanzados";
    advancedToggle.setAttribute("aria-expanded", String(isOpen));

    // Si abre la sección avanzada, restaurar los valores desde el input oculto
    if (isOpen) {
      difficultySelect.parentElement.style.display = "none";
    } else {
      difficultySelect.parentElement.style.display = "";
      syncDifficulty();
    }
  });

  // ── Sincronizar badge de turno (emoji jugador/IA) ─────────────────────
  const turnBadge  = document.getElementById("turnBadge");
  const turnLabel  = document.getElementById("turnLabel");
  const gameStatus = document.getElementById("gameStatus");

  // Observar cambios en el turnLabel para actualizar el badge
  const labelObserver = new MutationObserver(function () {
    const text = turnLabel.textContent || "";
    if (text.includes("2") || text.toLowerCase().includes("ia")) {
      turnBadge.textContent = "🤖";
      turnBadge.style.transform = "scale(1.1)";
    } else {
      turnBadge.textContent = "👤";
      turnBadge.style.transform = "scale(1)";
    }
  });

  labelObserver.observe(turnLabel, { childList: true, characterData: true, subtree: true });

  // Observar estado para feedback visual en strip
  const statusObserver = new MutationObserver(function () {
    const text = gameStatus.textContent || "";
    const strip = document.querySelector(".turn-strip");
    if (!strip) return;
    strip.classList.toggle("strip-thinking", text.includes("pensando"));
    strip.classList.toggle("strip-done",     text.includes("Finalizada"));
  });

  statusObserver.observe(gameStatus, { childList: true, characterData: true, subtree: true });

})();
