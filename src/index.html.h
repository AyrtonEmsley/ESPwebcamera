#pragma once

const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <title>ESP-CAM</title>
  <style>
    :root {
      --bg: #14120f;
      --panel: #1e1b16;
      --ink: #f2ead8;
      --muted: #9a8f7a;
      --line: #3a342a;
      --accent: #d4a017;
      --ok: #8fbf6a;
      --danger: #c45c4a;
    }
    * { box-sizing: border-box; }
    html, body {
      margin: 0;
      background: var(--bg);
      color: var(--ink);
      font-family: "Segoe UI", system-ui, sans-serif;
    }
    main {
      max-width: 42rem;
      margin: 0 auto;
      padding: 1rem 1rem 2rem;
    }
    header {
      display: flex;
      align-items: baseline;
      justify-content: space-between;
      gap: 0.75rem;
      margin-bottom: 0.85rem;
    }
    h1 {
      margin: 0;
      font-size: 1.15rem;
      letter-spacing: 0.08em;
      text-transform: uppercase;
    }
    #badge {
      font-size: 0.78rem;
      color: var(--muted);
      border: 1px solid var(--line);
      border-radius: 999px;
      padding: 0.25rem 0.7rem;
      white-space: nowrap;
    }
    #badge.ap { color: var(--accent); border-color: var(--accent); }
    #badge.sta { color: var(--ok); border-color: #4a6a38; }
    .frame {
      position: relative;
      background: #0b0a08;
      border: 1px solid var(--line);
      border-radius: 0.7rem;
      overflow: hidden;
      min-height: 180px;
    }
    .frame img {
      display: block;
      width: 100%;
      height: auto;
      background: #000;
    }
    #cam-error {
      display: none;
      position: absolute;
      inset: 0;
      align-items: center;
      justify-content: center;
      text-align: center;
      padding: 1rem;
      color: var(--muted);
      background: #0b0a08;
    }
    #cam-error.show { display: flex; }
    .wake-btn {
      margin-left: 0.6rem;
      min-width: auto;
      min-height: 2.2rem;
      padding: 0 0.8rem;
      font-size: 0.8rem;
    }
    #log {
      max-height: 12rem;
      overflow: auto;
      font-size: 0.88rem;
    }
    #log .empty { color: var(--muted); }
    #log .item {
      display: flex;
      justify-content: space-between;
      gap: 0.75rem;
      padding: 0.4rem 0;
      border-bottom: 1px solid var(--line);
    }
    #log .item:last-child { border-bottom: 0; }
    .row {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 0.75rem;
      margin-top: 0.85rem;
    }
    .card {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 0.7rem;
      padding: 0.85rem 0.95rem;
    }
    .card.wide { grid-column: 1 / -1; }
    .label {
      font-size: 0.7rem;
      letter-spacing: 0.14em;
      text-transform: uppercase;
      color: var(--muted);
      margin-bottom: 0.35rem;
    }
    #distance {
      font-variant-numeric: tabular-nums;
      font-size: 2rem;
      line-height: 1.1;
    }
    #distance span {
      font-size: 0.9rem;
      color: var(--muted);
      margin-left: 0.2rem;
    }
    #angle-readout {
      font-variant-numeric: tabular-nums;
      font-size: 1.35rem;
    }
    .pan-controls {
      display: flex;
      align-items: center;
      gap: 0.55rem;
      margin-top: 0.55rem;
    }
    button {
      appearance: none;
      border: 1px solid var(--line);
      background: #2a251e;
      color: var(--ink);
      min-width: 2.6rem;
      min-height: 2.6rem;
      border-radius: 0.5rem;
      font-size: 1.2rem;
    }
    button:active { background: #3b342a; }
    input[type="range"] {
      flex: 1;
      accent-color: var(--accent);
    }
    @media (max-width: 520px) {
      .row { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <main>
    <header>
      <h1>ESP-CAM</h1>
      <div id="badge">Connecting…</div>
    </header>
    <div class="frame">
      <img id="stream" alt="Live camera">
      <div id="cam-error">Waiting for motion. Camera is asleep to save power.</div>
    </div>
    <div class="row">
      <div class="card">
        <div class="label">Distance</div>
        <div id="distance">—<span>cm</span></div>
      </div>
      <div class="card">
        <div class="label">Pan</div>
        <div id="angle-readout">90°</div>
      </div>
      <div class="card wide">
        <div class="label">Aim</div>
        <div class="pan-controls">
          <button type="button" id="left" aria-label="Pan left">◀</button>
          <input id="angle" type="range" min="0" max="180" value="90">
          <button type="button" id="right" aria-label="Pan right">▶</button>
        </div>
      </div>
      <div class="card wide">
        <div class="label">Motion log <button type="button" class="wake-btn" id="wake">Wake camera</button></div>
        <div id="log"><div class="empty">No motion yet</div></div>
      </div>
    </div>
  </main>
  <script>
    const stream = document.getElementById("stream");
    const camError = document.getElementById("cam-error");
    const badge = document.getElementById("badge");
    const distanceEl = document.getElementById("distance");
    const logEl = document.getElementById("log");
    const angleSlider = document.getElementById("angle");
    const angleReadout = document.getElementById("angle-readout");
    let angle = 90;
    let lastSent = 0;
    let streamOn = false;

    function clamp(n) { return Math.max(0, Math.min(180, n)); }

    function ago(ms) {
      const s = Math.round(ms / 1000);
      if (s < 60) return s + "s ago";
      const m = Math.round(s / 60);
      if (m < 60) return m + "m ago";
      return Math.round(m / 60) + "h ago";
    }

    function setStream(on) {
      if (on && !streamOn) {
        streamOn = true;
        stream.src = "http://" + location.hostname + ":81/stream?t=" + Date.now();
      } else if (!on && streamOn) {
        streamOn = false;
        stream.removeAttribute("src");
      }
    }

    function clamp(n) { return Math.max(0, Math.min(180, n)); }

    async function sendAngle(next) {
      angle = clamp(next);
      angleSlider.value = String(angle);
      angleReadout.textContent = angle + "°";
      try {
        await fetch("/control?angle=" + angle, { cache: "no-store" });
      } catch (e) {}
    }

    angleSlider.addEventListener("input", () => {
      const now = Date.now();
      angle = Number(angleSlider.value);
      angleReadout.textContent = angle + "°";
      if (now - lastSent < 60) return;
      lastSent = now;
      sendAngle(angle);
    });
    angleSlider.addEventListener("change", () => sendAngle(Number(angleSlider.value)));
    document.getElementById("left").onclick = () => sendAngle(angle - 10);
    document.getElementById("right").onclick = () => sendAngle(angle + 10);

    async function refreshStatus() {
      try {
        const res = await fetch("/status", { cache: "no-store" });
        const data = await res.json();
        badge.className = data.ap ? "ap" : "sta";
        badge.textContent = (data.ap ? "Hotspot · " : "Home Wi-Fi · ") + data.ip;
        if (typeof data.angle === "number") {
          angle = data.angle;
          angleSlider.value = String(angle);
          angleReadout.textContent = angle + "°";
        }
        if (data.failed) {
          camError.textContent = "Camera not found. Check the ribbon and 5V power.";
          camError.classList.add("show");
          setStream(false);
        } else if (data.camera && data.live) {
          camError.classList.remove("show");
          setStream(true);
        } else {
          camError.textContent = "Waiting for motion. Camera is asleep to save power.";
          camError.classList.add("show");
          setStream(false);
        }
      } catch (e) {
        badge.className = "";
        badge.textContent = "Offline";
      }
    }

    async function refreshDistance() {
      try {
        const res = await fetch("/distance", { cache: "no-store" });
        const data = await res.json();
        if (data.cm === null || data.cm === undefined) {
          distanceEl.innerHTML = "—<span>cm</span>";
        } else {
          distanceEl.innerHTML = Number(data.cm).toFixed(1) + "<span>cm</span>";
        }
      } catch (e) {}
    }

    async function refreshEvents() {
      try {
        const res = await fetch("/events", { cache: "no-store" });
        const data = await res.json();
        const items = data.events || [];
        if (!items.length) {
          logEl.innerHTML = '<div class="empty">No motion yet</div>';
          return;
        }
        logEl.innerHTML = items.map((ev) => {
          const when = ev.t ? ev.t : ago(ev.ago_ms);
          const dist = (ev.cm === null || ev.cm === undefined) ? "—" : Number(ev.cm).toFixed(1) + " cm";
          return '<div class="item"><span>' + when + '</span><span>' + dist + '</span></div>';
        }).join("");
      } catch (e) {}
    }

    document.getElementById("wake").onclick = async () => {
      try { await fetch("/wake", { cache: "no-store" }); } catch (e) {}
      refreshStatus();
    };

    refreshStatus();
    refreshDistance();
    refreshEvents();
    setInterval(refreshDistance, 250);
    setInterval(refreshStatus, 1000);
    setInterval(refreshEvents, 1000);
  </script>
</body>
</html>
)rawliteral";
