// ui-extras.js — extensiones UX sin modificar app.js
(function () {
  "use strict";

  // ── Modal ──────────────────────────────────────────────────────────────
  const modal    = document.getElementById("guideModal");
  const btnHelp  = document.getElementById("helpButton");
  const btnClose = document.getElementById("guideClose");
  const btnStart = document.getElementById("guideStart");

  function openGuide()  { modal.showModal(); }
  function closeGuide() { modal.close(); }

  btnHelp .addEventListener("click", openGuide);
  btnClose.addEventListener("click", closeGuide);
  btnStart.addEventListener("click", closeGuide);
  modal   .addEventListener("click", e => { if (e.target === modal) closeGuide(); });
  modal   .addEventListener("cancel", closeGuide);

  // Primera visita: mostrar guía automáticamente
  if (!sessionStorage.getItem("guideShown")) {
    sessionStorage.setItem("guideShown", "1");
    requestAnimationFrame(openGuide);
  }

  // ── Selector de dificultad → controla depth/simulations ───────────────
  const diffSel  = document.getElementById("difficulty");
  const depthIn  = document.getElementById("depth");
  const simsIn   = document.getElementById("simulations");
  const algoSel  = document.getElementById("algorithm");

  const AB_DEPTH = { "4": 4, "8": 8, "12": 12, "16": 16 };
  const MC_SIMS  = { "4": 2000, "8": 10000, "12": 50000, "16": 100000 };

  function syncDifficulty() {
    const v = diffSel.value;
    if (algoSel.value === "mcts") {
      simsIn.value = MC_SIMS[v] ?? 10000;
      simsIn.dispatchEvent(new Event("input"));
    } else {
      depthIn.value = AB_DEPTH[v] ?? 8;
      depthIn.dispatchEvent(new Event("input"));
    }
  }

  diffSel.addEventListener("change", syncDifficulty);
  algoSel.addEventListener("change", syncDifficulty);
  syncDifficulty();

  // ── Badge de turno (Material Symbol person / smart_toy) ───────────────
  const badge     = document.getElementById("turnBadge");
  const turnLabel = document.getElementById("turnLabel");
  const gameStatus = document.getElementById("gameStatus");
  const statusBar  = document.querySelector(".glass-panel");

  new MutationObserver(() => {
    const t = turnLabel.textContent || "";
    if (t.includes("IA") || t.includes("2")) {
      badge.textContent = "smart_toy";
    } else {
      badge.textContent = "person";
    }
  }).observe(turnLabel, { childList: true, characterData: true, subtree: true });

  new MutationObserver(() => {
    const s = gameStatus.textContent || "";
    if (statusBar) statusBar.classList.toggle("thinking-pulse", s.includes("pensando"));
  }).observe(gameStatus, { childList: true, characterData: true, subtree: true });

})();
