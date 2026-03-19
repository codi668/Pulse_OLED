const statusEl = document.getElementById("status");
const instantBpmEl = document.getElementById("instantBpm");
const avgBpmEl = document.getElementById("avgBpm");
const finalAvgEl = document.getElementById("finalAvg");
const finalLabelEl = document.getElementById("finalLabel");
const rawIrEl = document.getElementById("rawIr");
const calibratedIrEl = document.getElementById("calibratedIr");
const beatStateEl = document.getElementById("beatState");
const fingerStateEl = document.getElementById("fingerState");
const measurementStateEl = document.getElementById("measurementState");
const calibrationStateEl = document.getElementById("calibrationState");
const thresholdInfoEl = document.getElementById("thresholdInfo");
const detectOffsetEl = document.getElementById("detectOffset");
const releaseOffsetEl = document.getElementById("releaseOffset");
const saveThresholdEl = document.getElementById("saveThreshold");
const startMeasurementEl = document.getElementById("startMeasurement");
const startCalibrationEl = document.getElementById("startCalibration");
const chartCanvas = document.getElementById("chart");
const chartCtx = chartCanvas.getContext("2d");
const beatCanvas = document.getElementById("beatChart");
const beatCtx = beatCanvas.getContext("2d");

const chartPoints = [];
const beatTimes = [];
const beatWindowMs = 10000;
const pollIntervalMs = 200;

let measureMs = 10000;
let calibrationMs = 3000;
let maxPoints = Math.max(2, Math.ceil(measureMs / pollIntervalMs));

function formatBpm(value) {
  return value > 0 ? value.toFixed(1) : "--";
}

function formatDuration(ms) {
  const seconds = ms / 1000;
  return Number.isInteger(seconds) ? `${seconds}s` : `${seconds.toFixed(1)}s`;
}

function updateWindowConfig(config) {
  measureMs = config.measure_ms ?? measureMs;
  calibrationMs = config.calibration_ms ?? calibrationMs;
  maxPoints = Math.max(2, Math.ceil(measureMs / pollIntervalMs));
  finalLabelEl.textContent = `${formatDuration(measureMs)} Messung`;
  startMeasurementEl.textContent = `${formatDuration(measureMs)} Messung starten`;

  while (chartPoints.length > maxPoints) {
    chartPoints.shift();
  }
}

function drawChart() {
  const width = chartCanvas.width;
  const height = chartCanvas.height;

  chartCtx.clearRect(0, 0, width, height);
  chartCtx.fillStyle = "#fffaf2";
  chartCtx.fillRect(0, 0, width, height);

  chartCtx.strokeStyle = "#e3d4c0";
  chartCtx.lineWidth = 1;
  for (let i = 0; i < 4; i += 1) {
    const y = 20 + (i * (height - 40)) / 3;
    chartCtx.beginPath();
    chartCtx.moveTo(0, y);
    chartCtx.lineTo(width, y);
    chartCtx.stroke();
  }

  if (!chartPoints.length) {
    chartCtx.fillStyle = "#8f7560";
    chartCtx.font = "20px Georgia, serif";
    chartCtx.fillText("Messung starten, um den Verlauf zu sehen", 24, height / 2);
    return;
  }

  const min = 40;
  const max = 180;

  chartCtx.strokeStyle = "#cb5a2e";
  chartCtx.lineWidth = 3;
  chartCtx.beginPath();

  chartPoints.forEach((point, index) => {
    const x = (index / Math.max(1, maxPoints - 1)) * (width - 40) + 20;
    const clamped = Math.min(Math.max(point, min), max);
    const y = height - 20 - ((clamped - min) / (max - min)) * (height - 40);
    if (index === 0) {
      chartCtx.moveTo(x, y);
    } else {
      chartCtx.lineTo(x, y);
    }
  });

  chartCtx.stroke();
}

function drawBeatGraph(now) {
  const width = beatCanvas.width;
  const height = beatCanvas.height;

  beatCtx.clearRect(0, 0, width, height);
  beatCtx.fillStyle = "#fffaf2";
  beatCtx.fillRect(0, 0, width, height);

  beatCtx.strokeStyle = "#d8c4aa";
  beatCtx.lineWidth = 1;
  beatCtx.beginPath();
  beatCtx.moveTo(0, height - 24);
  beatCtx.lineTo(width, height - 24);
  beatCtx.stroke();

  beatCtx.strokeStyle = "#0f4c5c";
  beatCtx.lineWidth = 2.5;
  beatCtx.shadowColor = "rgba(15, 76, 92, 0.22)";
  beatCtx.shadowBlur = 10;

  beatTimes.forEach((time) => {
    const age = now - time;
    const x = width - 12 - (age / beatWindowMs) * (width - 24);
    if (x < 12 || x > width - 12) {
      return;
    }

    beatCtx.beginPath();
    beatCtx.moveTo(x - 16, height - 24);
    beatCtx.lineTo(x - 6, height - 24);
    beatCtx.lineTo(x - 2, 34);
    beatCtx.lineTo(x + 4, height - 24);
    beatCtx.lineTo(x + 18, height - 24);
    beatCtx.stroke();
  });

  beatCtx.shadowBlur = 0;
}

async function loadConfig() {
  const response = await fetch("/api/config", { cache: "no-store" });
  if (!response.ok) {
    throw new Error("config_load_failed");
  }

  const config = await response.json();
  updateWindowConfig(config);
}

async function loadThresholds() {
  const response = await fetch("/api/threshold", { cache: "no-store" });
  if (!response.ok) {
    throw new Error("threshold_load_failed");
  }

  const data = await response.json();
  detectOffsetEl.value = String(data.detect_offset ?? "");
  releaseOffsetEl.value = String(data.release_offset ?? "");
  thresholdInfoEl.textContent = `Aktive Schwelle: ${data.active_threshold ?? "--"}`;
}

async function saveThresholds() {
  const params = new URLSearchParams();
  params.set("detect_offset", detectOffsetEl.value.trim());
  params.set("release_offset", releaseOffsetEl.value.trim());

  saveThresholdEl.disabled = true;
  try {
    const response = await fetch("/api/threshold", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8" },
      body: params.toString(),
    });

    if (!response.ok) {
      statusEl.textContent = await response.text();
      return;
    }

    statusEl.textContent = "Threshold gespeichert";
    await loadThresholds();
  } catch (error) {
    statusEl.textContent = "Threshold konnte nicht gespeichert werden";
  } finally {
    saveThresholdEl.disabled = false;
  }
}

async function startMeasurement() {
  try {
    const response = await fetch("/api/start", { method: "POST" });
    if (!response.ok) {
      statusEl.textContent = "Kalibrierung erst abschliessen";
      return;
    }

    chartPoints.length = 0;
    finalAvgEl.textContent = "--";
    drawChart();
  } catch (error) {
    statusEl.textContent = "Messung konnte nicht gestartet werden";
  }
}

async function startCalibration() {
  try {
    await fetch("/api/calibrate", { method: "POST" });
    chartPoints.length = 0;
    beatTimes.length = 0;
    finalAvgEl.textContent = "--";
    drawChart();
    drawBeatGraph(Date.now());
    statusEl.textContent = `Kalibrierung gestartet (${formatDuration(calibrationMs)})`;
  } catch (error) {
    statusEl.textContent = "Kalibrierung konnte nicht gestartet werden";
  }
}

function updateStatus(data) {
  if (data.calibrating) {
    const remaining = Math.max(0, calibrationMs - (data.calibration_elapsed_ms ?? 0));
    statusEl.textContent = `Kalibrierung laeuft, Finger weg (${remaining} ms)`;
    return;
  }

  if (!data.finger) {
    statusEl.textContent = "Finger auflegen";
    return;
  }

  if (data.active) {
    const elapsed = Math.min(data.elapsed_ms ?? 0, measureMs);
    statusEl.textContent = `Messung laeuft (${elapsed} / ${measureMs} ms)`;
    return;
  }

  statusEl.textContent = data.avg_bpm > 0 ? "Signal stabil" : "Suche Puls";
}

async function poll() {
  const now = Date.now();
  try {
    const response = await fetch("/api/pulse", { cache: "no-store" });
    const data = await response.json();

    updateWindowConfig(data);
    updateStatus(data);

    instantBpmEl.textContent = formatBpm(data.instant_bpm ?? 0);
    avgBpmEl.textContent = formatBpm(data.avg_bpm ?? 0);
    finalAvgEl.textContent = data.done && data.final_avg > 0 ? `${data.final_avg.toFixed(1)}` : "--";

    rawIrEl.textContent = String(data.raw_ir ?? "--");
    calibratedIrEl.textContent = String(data.calibrated_ir ?? "--");
    beatStateEl.textContent = data.beat ? "ja" : "nein";
    fingerStateEl.textContent = data.finger ? "anliegend" : "weg";
    measurementStateEl.textContent = data.active
      ? `${Math.min(data.elapsed_ms ?? 0, measureMs)} ms`
      : data.done
        ? "fertig"
        : "bereit";
    calibrationStateEl.textContent = data.calibrating
      ? `${Math.max(0, calibrationMs - (data.calibration_elapsed_ms ?? 0))} ms`
      : data.calibrated
        ? "ok"
        : "offen";
    thresholdInfoEl.textContent = `Aktive Schwelle: ${data.finger_threshold ?? "--"}`;

    if (data.beat) {
      beatTimes.push(now);
    }
    while (beatTimes.length && now - beatTimes[0] > beatWindowMs) {
      beatTimes.shift();
    }
    drawBeatGraph(now);

    if (data.active) {
      const point = (data.instant_bpm ?? 0) > 0 ? data.instant_bpm : data.avg_bpm;
      chartPoints.push(point > 0 ? point : 0);
      while (chartPoints.length > maxPoints) {
        chartPoints.shift();
      }
      drawChart();
    }
  } catch (error) {
    statusEl.textContent = "Keine Verbindung";
  }
}

saveThresholdEl.addEventListener("click", saveThresholds);
startMeasurementEl.addEventListener("click", startMeasurement);
startCalibrationEl.addEventListener("click", startCalibration);

drawChart();
drawBeatGraph(Date.now());

Promise.all([loadConfig(), loadThresholds()])
  .then(() => poll())
  .catch(() => {
    statusEl.textContent = "Konfiguration konnte nicht geladen werden";
  });

setInterval(poll, pollIntervalMs);
