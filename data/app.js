const bpmEl = document.getElementById("bpm");
const statusEl = document.getElementById("status");
const finalEl = document.getElementById("final");
const rawEl = document.getElementById("raw");
const beatEl = document.getElementById("beat");
const startBtn = document.getElementById("start");
const canvas = document.getElementById("chart");
const ctx = canvas.getContext("2d");
const beatCanvas = document.getElementById("beatChart");
const beatCtx = beatCanvas.getContext("2d");

const points = [];
const maxPoints = 50; // ~10s at 200ms

const beatWindowMs = 10000;
const beatTimes = [];

function draw() {
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.strokeStyle = "#2f81f7";
  ctx.lineWidth = 2;
  ctx.beginPath();

  const max = 180;
  const min = 40;

  points.forEach((p, i) => {
    const x = (i / (maxPoints - 1)) * (canvas.width - 10) + 5;
    const clamped = Math.min(Math.max(p, min), max);
    const y = canvas.height - ((clamped - min) / (max - min)) * (canvas.height - 10) - 5;
    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  });

  ctx.stroke();
}

function drawBeatGraph(now) {
  beatCtx.clearRect(0, 0, beatCanvas.width, beatCanvas.height);
  beatCtx.strokeStyle = "#2f81f7";
  beatCtx.lineWidth = 2;

  // baseline
  beatCtx.beginPath();
  beatCtx.moveTo(5, beatCanvas.height - 10);
  beatCtx.lineTo(beatCanvas.width - 5, beatCanvas.height - 10);
  beatCtx.stroke();

  // draw steep half-sine spikes for each beat
  beatCtx.strokeStyle = "#f85149";
  beatCtx.shadowColor = "rgba(248, 81, 73, 0.45)";
  beatCtx.shadowBlur = 8;
  beatTimes.forEach((t) => {
    const age = now - t;
    const x = beatCanvas.width - 5 - (age / beatWindowMs) * (beatCanvas.width - 10);
    if (x < 5 || x > beatCanvas.width - 5) return;

    const width = 32; // spike width in px
    const height = beatCanvas.height - 34;
    const startX = x - width / 2;

    beatCtx.beginPath();
    for (let i = 0; i <= 20; i++) {
      const p = i / 20;
      const sx = startX + p * width;
      const s = Math.sin(p * Math.PI);
      const shaped = Math.pow(s, 0.45); // steeper sides
      const sy = (beatCanvas.height - 10) - shaped * height;
      if (i === 0) beatCtx.moveTo(sx, sy); else beatCtx.lineTo(sx, sy);
    }
    beatCtx.stroke();
  });
  beatCtx.shadowBlur = 0;
}

async function poll() {
  const now = Date.now();
  try {
    const res = await fetch("/api/pulse", { cache: "no-store" });
    const data = await res.json();

    rawEl.textContent = data.raw_ir ?? "--";
    beatEl.textContent = data.beat ? "ja" : "nein";

    if (data.beat) {
      beatTimes.push(now);
    }

    while (beatTimes.length && now - beatTimes[0] > beatWindowMs) {
      beatTimes.shift();
    }

    drawBeatGraph(now);

    if (!data.finger || data.bpm <= 0) {
      bpmEl.textContent = "--";
      statusEl.textContent = "Finger auflegen";
    } else {
      bpmEl.textContent = data.bpm.toFixed(1);
      statusEl.textContent = data.active ? "Messung laeuft…" : "Signal ok";
    }

    if (data.active) {
      const v = data.bpm > 0 ? data.bpm : 0;
      points.push(v);
      while (points.length > maxPoints) points.shift();
      draw();
    }

    if (data.done) {
      finalEl.textContent = data.final_avg > 0 ? data.final_avg.toFixed(1) + " BPM" : "--";
    }
  } catch (err) {
    statusEl.textContent = "Keine Verbindung";
  }
}

startBtn.addEventListener("click", async () => {
  try {
    await fetch("/api/start", { method: "POST" });
    points.length = 0;
    finalEl.textContent = "--";
  } catch (err) {}
});

setInterval(poll, 200);
poll();
