const statusEl = document.getElementById("status");
const connectionBadgeEl = document.getElementById("connectionBadge");
const fingerBadgeEl = document.getElementById("fingerBadge");
const rangeBadgeEl = document.getElementById("rangeBadge");
const measureMetaEl = document.getElementById("measureMeta");
const measureFillEl = document.getElementById("measureFill");
const instantBpmEl = document.getElementById("instantBpm");
const avgBpmEl = document.getElementById("avgBpm");
const finalAvgEl = document.getElementById("finalAvg");
const finalLabelEl = document.getElementById("finalLabel");
const signalQualityEl = document.getElementById("signalQuality");
const signalQualityLabelEl = document.getElementById("signalQualityLabel");
const meaningChipEl = document.getElementById("meaningChip");
const meaningTitleEl = document.getElementById("meaningTitle");
const meaningCopyEl = document.getElementById("meaningCopy");
const meaningHintEl = document.getElementById("meaningHint");
const lastUpdateEl = document.getElementById("lastUpdate");
const qualitySummaryEl = document.getElementById("qualitySummary");
const qualityStateEl = document.getElementById("qualityState");
const qualityHelpEl = document.getElementById("qualityHelp");
const qualityBarFillEl = document.getElementById("qualityBarFill");
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
const beatSensitivityEl = document.getElementById("beatSensitivity");
const saveSensitivityEl = document.getElementById("saveSensitivity");
const sensitivityInfoEl = document.getElementById("sensitivityInfo");
const oledModeEl = document.getElementById("oledMode");
const saveOledModeEl = document.getElementById("saveOledMode");
const oledInfoEl = document.getElementById("oledInfo");
const startMeasurementEl = document.getElementById("startMeasurement");
const startCalibrationEl = document.getElementById("startCalibration");
const chartCanvas = document.getElementById("chart");
const chartCtx = chartCanvas.getContext("2d");
const beatCanvas = document.getElementById("beatChart");
const beatCtx = beatCanvas.getContext("2d");
const rangeCardEls = Array.from(document.querySelectorAll(".range-card"));

const chartPoints = [];
const beatTimes = [];
const beatWindowMs = 10000;
const pollIntervalMs = 200;

let measureMs = 10000;
let calibrationMs = 3000;
let maxPoints = Math.max(2, Math.ceil(measureMs / pollIntervalMs));

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

function setTone(element, tone) {
  element.dataset.tone = tone;
}

function formatBpm(value) {
  return value > 0 ? value.toFixed(1) : "--";
}

function formatDuration(ms) {
  const seconds = ms / 1000;
  return Number.isInteger(seconds) ? `${seconds}s` : `${seconds.toFixed(1)}s`;
}

function formatClock(date) {
  return date.toLocaleTimeString("de-DE", {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });
}

function activeDisplayBpm(data) {
  if ((data.final_avg ?? 0) > 0 && data.done) {
    return data.final_avg;
  }
  if ((data.avg_bpm ?? 0) > 0) {
    return data.avg_bpm;
  }
  return data.instant_bpm ?? 0;
}

function describePulse(value) {
  if (!(value > 0)) {
    return {
      badge: "wartet",
      title: "Warte auf stabile Messung",
      copy: "Sobald ein plausibler Puls erkannt ist, wird hier der aktuelle Bereich direkt eingeordnet.",
      hint: "Am besten den Finger mittig auflegen und fuer ein paar Sekunden ruhig halten.",
      zone: "",
      tone: "neutral",
      chip: "Noch keine Daten",
    };
  }

  if (value < 50) {
    return {
      badge: "sehr niedrig",
      title: "Sehr niedriger Pulsbereich",
      copy: "Das kann in tiefer Ruhe oder bei trainierten Personen normal sein. Wenn der Wert fuer dich untypisch ist, miss erneut in entspannter Haltung.",
      hint: "Bei Schwindel oder auffaelligem Gefuehl ist das eine Sache fuer eine echte medizinische Klaerung, nicht fuer den Sensor allein.",
      zone: "very-low",
      tone: "cool",
      chip: "Sehr niedrig",
    };
  }

  if (value < 60) {
    return {
      badge: "ruhig",
      title: "Ruhiger Pulsbereich",
      copy: "Das passt oft zu einer entspannten Messung und kann besonders bei guter Fitness voellig normal sein.",
      hint: "Wenn du gerade gesessen oder ruhig geatmet hast, ist dieser Bereich meist unauffaellig.",
      zone: "low",
      tone: "cool",
      chip: "Ruhig",
    };
  }

  if (value < 85) {
    return {
      badge: "typischer Ruhebereich",
      title: "Typischer Ruhepuls",
      copy: "Das ist haeufig ein plausibler Bereich fuer eine ruhige Alltagsmessung ohne groessere Bewegung.",
      hint: "Fuer Vergleichswerte immer in aehnlicher Haltung und unter aehnlichen Bedingungen messen.",
      zone: "normal",
      tone: "stable",
      chip: "Stabil",
    };
  }

  if (value < 100) {
    return {
      badge: "erhoeht",
      title: "Leicht erhoehter Bereich",
      copy: "Das kann durch Stress, Bewegung, Koffein oder unruhigen Fingerkontakt entstehen und ist nicht automatisch problematisch.",
      hint: "Wenn du den Ruhewert wissen willst, miss noch einmal nach ein paar Minuten in entspannter Haltung.",
      zone: "elevated",
      tone: "warm",
      chip: "Erhoeht",
    };
  }

  return {
    badge: "deutlich erhoeht",
    title: "Deutlich erhoehter Puls",
    copy: "Dieser Bereich passt eher zu Aktivitaet oder Anspannung. In echter Ruhe sollte der Wert mit stabilem Kontakt erneut geprueft werden.",
    hint: "Die Anzeige ist eine Orientierung. Die Bedeutung haengt stark davon ab, ob du gerade aktiv warst oder wirklich in Ruhe misst.",
    zone: "high",
    tone: "hot",
    chip: "Hoch",
  };
}

function describeSignal(data) {
  if (!data.finger) {
    return {
      score: 0,
      label: "kein Kontakt",
      summary: "Finger fehlt",
      help: "Finger mittig auflegen und den Sensor nicht verkanten.",
      tone: "neutral",
    };
  }

  const rawMargin = (data.raw_ir ?? 0) - (data.finger_threshold ?? 0);
  const calibrated = data.calibrated_ir ?? 0;
  let score = Math.round((rawMargin / 70000) * 65 + (calibrated / 50000) * 35);
  score = clamp(score, 12, 100);

  if ((data.avg_bpm ?? 0) > 0) {
    score = Math.max(score, 78);
  }
  if (data.beat) {
    score = Math.max(score, 90);
  }

  if (score < 35) {
    return {
      score,
      label: "schwach",
      summary: "Schwaches Signal",
      help: "Finger ruhiger und etwas mittiger auflegen.",
      tone: "warm",
    };
  }

  if (score < 65) {
    return {
      score,
      label: "brauchbar",
      summary: "Messung moeglich",
      help: "Der Sensor sieht genug Signal, aber etwas ruhigerer Kontakt hilft.",
      tone: "cool",
    };
  }

  if (score < 85) {
    return {
      score,
      label: "gut",
      summary: "Guter Kontakt",
      help: "So laesst sich die Puls-Erkennung meist sauber verfolgen.",
      tone: "stable",
    };
  }

  return {
    score,
    label: "sehr gut",
    summary: "Sehr sauber",
    help: "Kontakt und Fingerlage sind aktuell sehr stabil.",
    tone: "stable",
  };
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
  const min = 40;
  const max = 180;

  chartCtx.clearRect(0, 0, width, height);
  chartCtx.fillStyle = "#f7fbfc";
  chartCtx.fillRect(0, 0, width, height);

  chartCtx.strokeStyle = "rgba(23, 35, 43, 0.08)";
  chartCtx.lineWidth = 1;
  for (let i = 0; i < 5; i += 1) {
    const y = 20 + (i * (height - 40)) / 4;
    chartCtx.beginPath();
    chartCtx.moveTo(18, y);
    chartCtx.lineTo(width - 18, y);
    chartCtx.stroke();

    const labelValue = max - (i * (max - min)) / 4;
    chartCtx.fillStyle = "#6c7b86";
    chartCtx.font = '12px "Trebuchet MS", sans-serif';
    chartCtx.fillText(String(Math.round(labelValue)), width - 52, y - 6);
  }

  const validPoints = chartPoints.filter((value) => value > 0);
  if (!validPoints.length) {
    chartCtx.fillStyle = "#6c7b86";
    chartCtx.font = '20px "Palatino Linotype", serif';
    chartCtx.fillText("Messung starten, um den Verlauf zu sehen", 24, height / 2);
    return;
  }

  const gradient = chartCtx.createLinearGradient(0, 0, 0, height);
  gradient.addColorStop(0, "rgba(220, 107, 77, 0.30)");
  gradient.addColorStop(1, "rgba(220, 107, 77, 0.02)");

  const points = chartPoints.map((point, index) => {
    const x = (index / Math.max(1, maxPoints - 1)) * (width - 40) + 20;
    const clamped = clamp(point, min, max);
    const y = height - 20 - ((clamped - min) / (max - min)) * (height - 40);
    return { x, y, value: point };
  });

  chartCtx.beginPath();
  let started = false;
  for (const point of points) {
    if (point.value <= 0) {
      continue;
    }
    if (!started) {
      chartCtx.moveTo(point.x, point.y);
      started = true;
    } else {
      chartCtx.lineTo(point.x, point.y);
    }
  }

  const linePoints = points.filter((point) => point.value > 0);
  if (linePoints.length > 1) {
    chartCtx.save();
    chartCtx.lineWidth = 4;
    chartCtx.strokeStyle = "#dc6b4d";
    chartCtx.shadowColor = "rgba(220, 107, 77, 0.18)";
    chartCtx.shadowBlur = 18;
    chartCtx.stroke();
    chartCtx.restore();

    chartCtx.beginPath();
    chartCtx.moveTo(linePoints[0].x, height - 20);
    for (const point of linePoints) {
      chartCtx.lineTo(point.x, point.y);
    }
    chartCtx.lineTo(linePoints[linePoints.length - 1].x, height - 20);
    chartCtx.closePath();
    chartCtx.fillStyle = gradient;
    chartCtx.fill();
  }

  const lastPoint = linePoints[linePoints.length - 1];
  chartCtx.beginPath();
  chartCtx.arc(lastPoint.x, lastPoint.y, 6, 0, Math.PI * 2);
  chartCtx.fillStyle = "#dc6b4d";
  chartCtx.fill();
  chartCtx.lineWidth = 3;
  chartCtx.strokeStyle = "rgba(255, 255, 255, 0.9)";
  chartCtx.stroke();
}

function drawBeatGraph(now) {
  const width = beatCanvas.width;
  const height = beatCanvas.height;

  beatCtx.clearRect(0, 0, width, height);
  beatCtx.fillStyle = "#f7fbfc";
  beatCtx.fillRect(0, 0, width, height);

  beatCtx.strokeStyle = "rgba(23, 35, 43, 0.08)";
  beatCtx.lineWidth = 1;
  beatCtx.beginPath();
  beatCtx.moveTo(20, height - 28);
  beatCtx.lineTo(width - 20, height - 28);
  beatCtx.stroke();

  for (let i = 1; i < 5; i += 1) {
    const x = 20 + (i * (width - 40)) / 5;
    beatCtx.beginPath();
    beatCtx.moveTo(x, 18);
    beatCtx.lineTo(x, height - 20);
    beatCtx.stroke();
  }

  if (!beatTimes.length) {
    beatCtx.fillStyle = "#6c7b86";
    beatCtx.font = '18px "Palatino Linotype", serif';
    beatCtx.fillText("Noch keine Beats im aktuellen Zeitfenster", 24, height / 2);
    return;
  }

  for (const time of beatTimes) {
    const age = now - time;
    const x = width - 16 - (age / beatWindowMs) * (width - 32);
    if (x < 12 || x > width - 12) {
      continue;
    }

    const alpha = clamp(1 - age / beatWindowMs, 0.22, 1);
    beatCtx.strokeStyle = `rgba(30, 103, 120, ${alpha})`;
    beatCtx.lineWidth = 2.6;
    beatCtx.beginPath();
    beatCtx.moveTo(x - 18, height - 28);
    beatCtx.lineTo(x - 8, height - 28);
    beatCtx.lineTo(x - 2, 38);
    beatCtx.lineTo(x + 4, height - 28);
    beatCtx.lineTo(x + 18, height - 28);
    beatCtx.stroke();
  }
}

function updateMeaning(data) {
  const meaning = describePulse(activeDisplayBpm(data));
  meaningChipEl.textContent = meaning.chip;
  meaningTitleEl.textContent = meaning.title;
  meaningCopyEl.textContent = meaning.copy;
  meaningHintEl.textContent = meaning.hint;
  rangeBadgeEl.textContent = `Bereich: ${meaning.badge}`;
  setTone(meaningChipEl, meaning.tone);
  setTone(rangeBadgeEl, meaning.tone);

  for (const card of rangeCardEls) {
    card.classList.toggle("active", meaning.zone !== "" && card.dataset.zone === meaning.zone);
  }
}

function updateSignal(data) {
  const signal = describeSignal(data);
  signalQualityEl.textContent = `${signal.score}%`;
  signalQualityLabelEl.textContent = signal.label;
  qualitySummaryEl.textContent = `${signal.score}%`;
  qualityStateEl.textContent = signal.summary;
  qualityHelpEl.textContent = signal.help;
  qualityBarFillEl.style.width = `${signal.score}%`;
  setTone(qualityBarFillEl, signal.tone);
  return signal;
}

function updateProgress(data) {
  let ratio = 0;
  let meta = "bereit";
  let tone = "neutral";

  if (data.calibrating) {
    ratio = clamp((data.calibration_elapsed_ms ?? 0) / calibrationMs, 0, 1);
    meta = `Kalibrierung ${Math.round(ratio * 100)}%`;
    tone = "cool";
  } else if (data.active) {
    ratio = clamp((data.elapsed_ms ?? 0) / measureMs, 0, 1);
    meta = `Messung ${Math.round(ratio * 100)}%`;
    tone = "warm";
  } else if (data.done) {
    ratio = 1;
    meta = "Messung fertig";
    tone = "stable";
  }

  measureMetaEl.textContent = meta;
  measureFillEl.style.width = `${Math.round(ratio * 100)}%`;
  setTone(measureFillEl, tone);
}

function setConnectionState(isConnected) {
  connectionBadgeEl.textContent = isConnected ? "Verbindung: online" : "Verbindung: offline";
  setTone(connectionBadgeEl, isConnected ? "stable" : "neutral");
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

  if ((data.avg_bpm ?? 0) > 0) {
    statusEl.textContent = "Signal stabil";
    return;
  }

  statusEl.textContent = "Suche Puls";
}

function updateBadges(data, signal) {
  fingerBadgeEl.textContent = data.finger ? "Finger: anliegend" : "Finger: weg";
  setTone(fingerBadgeEl, data.finger ? signal.tone : "neutral");
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

async function loadSensitivity() {
  const response = await fetch("/api/sensitivity", { cache: "no-store" });
  if (!response.ok) {
    throw new Error("sensitivity_load_failed");
  }

  const data = await response.json();
  beatSensitivityEl.value = String(data.beat_sensitivity ?? 5);
  sensitivityInfoEl.textContent =
    `Aktuell: ${data.beat_sensitivity ?? "--"} von 10 | Toleranz ${data.beat_tolerance_percent ?? "--"}%`;
}

function oledModeLabel(value) {
  switch (Number(value)) {
    case 0:
      return "SSD1306 128x64";
    case 1:
      return "SSD1306 ALT0 128x64";
    case 2:
      return "SH1106 128x64";
    case 3:
      return "SSD1306 128x32";
    default:
      return "unbekannt";
  }
}

async function loadOledMode() {
  const response = await fetch("/api/oled", { cache: "no-store" });
  if (!response.ok) {
    throw new Error("oled_load_failed");
  }

  const data = await response.json();
  oledModeEl.value = String(data.driver_mode ?? 0);
  oledInfoEl.textContent = `Aktueller Modus: ${oledModeLabel(data.driver_mode)}`;
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

async function saveSensitivity() {
  const params = new URLSearchParams();
  params.set("beat_sensitivity", beatSensitivityEl.value.trim());

  saveSensitivityEl.disabled = true;
  try {
    const response = await fetch("/api/sensitivity", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8" },
      body: params.toString(),
    });

    if (!response.ok) {
      statusEl.textContent = await response.text();
      return;
    }

    statusEl.textContent = "Puls-Empfindlichkeit gespeichert";
    await loadSensitivity();
  } catch (error) {
    statusEl.textContent = "Puls-Empfindlichkeit konnte nicht gespeichert werden";
  } finally {
    saveSensitivityEl.disabled = false;
  }
}

async function saveOledMode() {
  const params = new URLSearchParams();
  params.set("driver_mode", oledModeEl.value);

  saveOledModeEl.disabled = true;
  try {
    const response = await fetch("/api/oled", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8" },
      body: params.toString(),
    });

    if (!response.ok) {
      statusEl.textContent = await response.text();
      return;
    }

    statusEl.textContent = `OLED umgestellt auf ${oledModeLabel(oledModeEl.value)}`;
    await loadOledMode();
  } catch (error) {
    statusEl.textContent = "OLED-Modus konnte nicht gespeichert werden";
  } finally {
    saveOledModeEl.disabled = false;
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

async function poll() {
  const now = Date.now();
  try {
    const response = await fetch("/api/pulse", { cache: "no-store" });
    const data = await response.json();

    setConnectionState(true);
    updateWindowConfig(data);
    updateStatus(data);
    updateProgress(data);

    instantBpmEl.textContent = formatBpm(data.instant_bpm ?? 0);
    avgBpmEl.textContent = formatBpm(data.avg_bpm ?? 0);
    finalAvgEl.textContent = data.done && data.final_avg > 0 ? data.final_avg.toFixed(1) : "--";

    rawIrEl.textContent = String(data.raw_ir ?? "--");
    calibratedIrEl.textContent = String(data.calibrated_ir ?? "--");
    beatStateEl.textContent = data.beat ? "jetzt" : data.avg_bpm > 0 ? "stabil" : "wartet";
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
    sensitivityInfoEl.textContent =
      `Aktuell: ${data.beat_sensitivity ?? "--"} von 10 | Toleranz ${data.beat_tolerance_percent ?? "--"}%`;

    const signal = updateSignal(data);
    updateBadges(data, signal);
    updateMeaning(data);

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
    }

    if (chartPoints.length || data.done) {
      drawChart();
    }

    lastUpdateEl.textContent = formatClock(new Date(now));
  } catch (error) {
    setConnectionState(false);
    statusEl.textContent = "Keine Verbindung";
  }
}

saveThresholdEl.addEventListener("click", saveThresholds);
saveSensitivityEl.addEventListener("click", saveSensitivity);
saveOledModeEl.addEventListener("click", saveOledMode);
startMeasurementEl.addEventListener("click", startMeasurement);
startCalibrationEl.addEventListener("click", startCalibration);

drawChart();
drawBeatGraph(Date.now());
updateMeaning({});
setConnectionState(false);

Promise.all([loadConfig(), loadThresholds(), loadSensitivity(), loadOledMode()])
  .then(() => poll())
  .catch(() => {
    statusEl.textContent = "Konfiguration konnte nicht geladen werden";
  });

setInterval(poll, pollIntervalMs);
