/*
 * ══════════════════════════════════════════════════════════
 *   RADAR SONAR v1.0 — Sensor Ultrassônico HC-SR04
 *   Plataforma: ESP32 + WebServer + ElegantOTA
 * ══════════════════════════════════════════════════════════
 *
 *   Interface estilo radar militar com:
 *   - Sweep rotativo animado (canvas)
 *   - Anéis concêntricos de distância
 *   - Alvo pulsante ao detectar objeto
 *   - Paleta verde fósforo (sonar clássico)
 *   - Buzzer passivo + detecção rápida (raw)
 *   - OTA via /update
 *
 * ══════════════════════════════════════════════════════════
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ElegantOTA.h>

// ─── CREDENCIAIS WiFi ───────────────────────────────────
const char* ssid     = "SUA_REDE";
const char* password = "SUA_SENHA";

// ─── PINOS ──────────────────────────────────────────────
const int PINO_LED    = 2;
const int PINO_BUZZER = 32;
const int PINO_TRIG   = 27;
const int PINO_ECHO   = 26;

// ─── CONFIG ─────────────────────────────────────────────
const float  DIST_ALARME       = 5.0;
const long   INTERVALO_LEITURA = 100;
const int    FILTRO_AMOSTRAS   = 3;
const float  DIST_FALLBACK     = 100.0;
const long   TIMEOUT_ECHO_US   = 12000;
const int    FREQ_ALERTA       = 2700;

// ─── ESTADO ─────────────────────────────────────────────
WebServer server(80);
volatile float distanciaRaw     = DIST_FALLBACK;
volatile float distanciaDisplay = DIST_FALLBACK;
bool           alarmeAtivo  = true;
unsigned long  tempoAnterior = 0;
float          bufferFiltro[FILTRO_AMOSTRAS];
int            indiceFiltro  = 0;
bool           bufferCheio   = false;
bool           bipEstado     = false;
unsigned long  bipAnterior   = 0;
const long     bipIntervalo  = 150;
unsigned long  otaProgresso  = 0;

// ─── OTA CALLBACKS ──────────────────────────────────────
void onOTAStart() {
  Serial.println("⬆ OTA: Upload...");
  noTone(PINO_BUZZER);
}
void onOTAProgress(size_t cur, size_t total) {
  unsigned long agora = millis();
  if (agora - otaProgresso > 500) {
    otaProgresso = agora;
    Serial.printf("  ⬆ %d%%\n", (cur * 100) / total);
    digitalWrite(PINO_LED, !digitalRead(PINO_LED));
  }
}
void onOTAEnd(bool ok) {
  Serial.println(ok ? "✓ OTA OK!" : "✗ OTA Falhou");
  digitalWrite(PINO_LED, ok ? HIGH : LOW);
}

// ─── SENSOR ─────────────────────────────────────────────
float lerDistanciaRaw_() {
  digitalWrite(PINO_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PINO_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PINO_TRIG, LOW);
  long dur = pulseIn(PINO_ECHO, HIGH, TIMEOUT_ECHO_US);
  return (dur > 0) ? dur * 0.034f / 2.0f : DIST_FALLBACK;
}

void atualizarSensor() {
  float raw = lerDistanciaRaw_();
  distanciaRaw = raw;

  bufferFiltro[indiceFiltro] = raw;
  indiceFiltro = (indiceFiltro + 1) % FILTRO_AMOSTRAS;
  if (indiceFiltro == 0) bufferCheio = true;

  int n = bufferCheio ? FILTRO_AMOSTRAS : indiceFiltro;
  float soma = 0;
  for (int i = 0; i < n; i++) soma += bufferFiltro[i];
  distanciaDisplay = soma / n;
}

// ─── ALARME ─────────────────────────────────────────────
void atualizarAlarme() {
  bool perigo = alarmeAtivo && distanciaRaw > 0.5f && distanciaRaw <= DIST_ALARME;
  if (perigo) {
    digitalWrite(PINO_LED, HIGH);
    unsigned long agora = millis();
    if (agora - bipAnterior >= bipIntervalo) {
      bipAnterior = agora;
      bipEstado = !bipEstado;
      bipEstado ? tone(PINO_BUZZER, FREQ_ALERTA) : noTone(PINO_BUZZER);
    }
  } else {
    noTone(PINO_BUZZER);
    digitalWrite(PINO_LED, LOW);
    bipEstado = false;
  }
}

// ═══════════════════════════════════════════════════════════
//  INTERFACE — RADAR SONAR
// ═══════════════════════════════════════════════════════════

String getHTML() {
  String h = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>SONAR v4</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=VT323&family=Chakra+Petch:wght@400;600;700&display=swap');

*{margin:0;padding:0;box-sizing:border-box}

:root{
  --green:#00ff88;
  --green2:#00cc66;
  --greendim:#00ff8818;
  --greenlow:#00ff8808;
  --dark:#020a06;
  --panel:#041a0e;
  --red:#ff2244;
}

body{
  background:var(--dark);
  color:var(--green);
  font-family:'Chakra Petch',monospace;
  display:flex;
  flex-direction:column;
  align-items:center;
  justify-content:center;
  min-height:100vh;
  overflow:hidden;
}

/* Noise overlay */
body::after{
  content:'';position:fixed;inset:0;
  background:url("data:image/svg+xml,%3Csvg viewBox='0 0 256 256' xmlns='http://www.w3.org/2000/svg'%3E%3Cfilter id='n'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='0.9' numOctaves='4' stitchTiles='stitch'/%3E%3C/filter%3E%3Crect width='100%25' height='100%25' filter='url(%23n)' opacity='0.04'/%3E%3C/svg%3E");
  pointer-events:none;z-index:100;opacity:0.5;
}

.hud{
  width:94vmin;max-width:500px;
  display:flex;flex-direction:column;align-items:center;gap:12px;
  position:relative;z-index:1;
}

/* ── Header ── */
.top-bar{
  width:100%;display:flex;justify-content:space-between;align-items:center;
  padding:8px 4px;
  border-bottom:1px solid #00ff8822;
}
.top-bar h1{
  font-family:'VT323',monospace;
  font-size:16px;letter-spacing:4px;color:#00ff8899;
  text-transform:uppercase;
}
.top-bar .live{
  font-size:11px;color:#00ff8855;
  display:flex;align-items:center;gap:6px;
}
.top-bar .live::before{
  content:'';width:6px;height:6px;border-radius:50%;
  background:var(--green);box-shadow:0 0 6px var(--green);
  animation:livepulse 1.5s infinite;
}
@keyframes livepulse{0%,100%{opacity:1}50%{opacity:.2}}

/* ── Radar canvas ── */
.radar-wrap{
  position:relative;
  width:90vmin;max-width:460px;
  aspect-ratio:1;
}
#radar{
  width:100%;height:100%;
  border-radius:50%;
  background:radial-gradient(circle,#041a0e 0%,#020a06 100%);
  box-shadow:
    0 0 60px #00ff8808,
    0 0 2px #00ff8833,
    inset 0 0 80px #00000088;
}

/* ── Readout sobreposto ── */
.readout{
  position:absolute;
  bottom:18%;left:50%;transform:translateX(-50%);
  text-align:center;
  pointer-events:none;
}
.readout .val{
  font-family:'VT323',monospace;
  font-size:clamp(48px,12vmin,72px);
  line-height:1;
  text-shadow:0 0 20px #00ff8866;
  transition:color .2s;
}
.readout .val.danger{
  color:var(--red);
  text-shadow:0 0 20px #ff224466;
}
.readout .lbl{
  font-size:11px;color:#00ff8844;
  letter-spacing:3px;text-transform:uppercase;
  margin-top:2px;
}

/* ── Alert banner ── */
.alert-banner{
  display:none;
  width:100%;
  padding:8px;
  text-align:center;
  font-family:'VT323',monospace;
  font-size:18px;
  letter-spacing:6px;
  color:var(--red);
  border:1px solid #ff224444;
  border-radius:6px;
  background:#ff224410;
  animation:alertflash .4s infinite;
}
@keyframes alertflash{0%,100%{opacity:.3}50%{opacity:1}}

/* ── Controles ── */
.ctrl-row{
  width:100%;display:flex;gap:8px;
}
.btn{
  flex:1;
  background:transparent;
  border:1px solid #00ff8833;
  color:var(--green);
  padding:10px 12px;
  border-radius:6px;
  cursor:pointer;
  font-family:'Chakra Petch',monospace;
  font-size:11px;letter-spacing:1px;
  text-transform:uppercase;
  transition:all .2s;
}
.btn:hover{background:#00ff8810;border-color:var(--green)}
.btn:active{transform:scale(.97)}
.btn.off{border-color:#ff224444;color:#ff224488}

.btn-ota{
  flex:1;background:transparent;
  border:1px solid #ffa50033;color:#ffa50088;
  padding:10px 12px;border-radius:6px;cursor:pointer;
  font-family:'Chakra Petch',monospace;font-size:11px;
  letter-spacing:1px;text-transform:uppercase;
  text-decoration:none;text-align:center;
  transition:all .2s;
}
.btn-ota:hover{background:#ffa50010;border-color:#ffa500;color:#ffa500}

/* ── Footer ── */
.bot-bar{
  width:100%;display:flex;justify-content:space-between;
  padding:8px 4px;
  border-top:1px solid #00ff8815;
  font-size:10px;color:#00ff8822;letter-spacing:1px;
}

/* ── Scanlines ── */
.scanlines{
  position:fixed;inset:0;
  background:repeating-linear-gradient(0deg,transparent,transparent 2px,rgba(0,255,136,.015) 2px,rgba(0,255,136,.015) 4px);
  pointer-events:none;z-index:99;
}
</style>
</head>
<body>
<div class="scanlines"></div>

<div class="hud">
  <div class="top-bar">
    <h1>⟐ Sonar Radar v4</h1>
    <div class="live" id="connStatus">Online</div>
  </div>

  <div class="radar-wrap">
    <canvas id="radar"></canvas>
    <div class="readout">
      <div class="val" id="d">--.-</div>
      <div class="lbl">centímetros</div>
    </div>
  </div>

  <div id="alert" class="alert-banner">⚠ COLISÃO IMINENTE ⚠</div>

  <div class="ctrl-row">
    <button class="btn" id="btnAlarme" onclick="toggleAlarme()">🔊 Alarme: ON</button>
    <a class="btn-ota" href="/update" target="_blank">⬆ Firmware</a>
  </div>

  <div class="bot-bar">
    <span>v4.0</span>
    <span id="minMax">Min: -- | Max: --</span>
    <span id="rangeLabel">Alcance: 60cm</span>
  </div>
</div>

<script>
// ── Canvas setup ──
const canvas = document.getElementById('radar');
const ctx = canvas.getContext('2d');
const RANGE = 60; // cm máximo no radar

function resize(){
  const s = canvas.getBoundingClientRect();
  const dpr = window.devicePixelRatio||1;
  canvas.width = s.width*dpr;
  canvas.height = s.height*dpr;
  ctx.scale(dpr,dpr);
}
resize();
window.addEventListener('resize',resize);

// ── Estado ──
let dist = 100;
let minV = Infinity, maxV = -Infinity;
let angle = 0;
let targets = []; // ghost blips
let alarmeOn = true;

// ── Desenho principal ──
function draw(){
  const W = canvas.getBoundingClientRect().width;
  const H = canvas.getBoundingClientRect().height;
  const cx = W/2, cy = H/2;
  const R = Math.min(cx,cy)*0.92;

  ctx.clearRect(0,0,W,H);

  // ── Anéis concêntricos ──
  for(let i=1;i<=4;i++){
    const r = (R/4)*i;
    ctx.beginPath();
    ctx.arc(cx,cy,r,0,Math.PI*2);
    ctx.strokeStyle = i===4 ? '#00ff8825' : '#00ff8812';
    ctx.lineWidth = 1;
    ctx.stroke();

    // Labels de distância
    if(i<4){
      const labelDist = Math.round((RANGE/4)*i);
      ctx.fillStyle='#00ff8828';
      ctx.font='10px Chakra Petch, monospace';
      ctx.fillText(labelDist+'cm', cx+r-24, cy-4);
    }
  }

  // ── Cruz central ──
  ctx.strokeStyle='#00ff8815';
  ctx.lineWidth=1;
  ctx.beginPath();ctx.moveTo(cx-R,cy);ctx.lineTo(cx+R,cy);ctx.stroke();
  ctx.beginPath();ctx.moveTo(cx,cy-R);ctx.lineTo(cx,cy+R);ctx.stroke();

  // ── Linhas diagonais ──
  for(let a=Math.PI/4;a<Math.PI*2;a+=Math.PI/2){
    ctx.beginPath();
    ctx.moveTo(cx,cy);
    ctx.lineTo(cx+Math.cos(a)*R,cy+Math.sin(a)*R);
    ctx.strokeStyle='#00ff8808';
    ctx.stroke();
  }

  // ── Sweep (varredura) ──
  angle += 0.025;
  if(angle > Math.PI*2) angle -= Math.PI*2;

  const sweepGrad = ctx.createConicalGradient
    ? null // fallback
    : null;

  // Sweep trail (arco com gradiente)
  const trail = 0.6; // radianos de trail
  for(let i=0;i<20;i++){
    const a = angle - (trail/20)*i;
    const alpha = (1 - i/20)*0.15;
    ctx.beginPath();
    ctx.moveTo(cx,cy);
    ctx.arc(cx,cy,R,a-0.02,a+0.02);
    ctx.closePath();
    ctx.fillStyle=`rgba(0,255,136,${alpha})`;
    ctx.fill();
  }

  // Sweep line principal
  ctx.beginPath();
  ctx.moveTo(cx,cy);
  ctx.lineTo(cx+Math.cos(angle)*R, cy+Math.sin(angle)*R);
  ctx.strokeStyle='#00ff88aa';
  ctx.lineWidth=2;
  ctx.shadowBlur=15;
  ctx.shadowColor='#00ff88';
  ctx.stroke();
  ctx.shadowBlur=0;

  // ── Ponto central ──
  ctx.beginPath();
  ctx.arc(cx,cy,3,0,Math.PI*2);
  ctx.fillStyle='#00ff88';
  ctx.fill();

  // ── Alvo (blip) ──
  if(dist < RANGE && dist > 0){
    // Posição do alvo no sweep atual
    const targetR = (dist/RANGE)*R;
    const tx = cx + Math.cos(angle)*targetR;
    const ty = cy + Math.sin(angle)*targetR;

    // Adiciona ghost blip
    targets.push({x:tx, y:ty, life:1.0, danger: dist<=5});
  }

  // Desenha ghost blips (fade out)
  for(let i=targets.length-1;i>=0;i--){
    const t=targets[i];
    t.life -= 0.008;
    if(t.life<=0){targets.splice(i,1);continue;}

    const color = t.danger
      ? `rgba(255,34,68,${t.life*0.9})`
      : `rgba(0,255,136,${t.life*0.7})`;
    const glow = t.danger
      ? `rgba(255,34,68,${t.life*0.3})`
      : `rgba(0,255,136,${t.life*0.2})`;

    // Glow
    ctx.beginPath();
    ctx.arc(t.x,t.y,8*t.life,0,Math.PI*2);
    ctx.fillStyle=glow;
    ctx.fill();

    // Ponto
    ctx.beginPath();
    ctx.arc(t.x,t.y,3*t.life,0,Math.PI*2);
    ctx.fillStyle=color;
    ctx.fill();
  }

  // ── Limite de alarme (anel vermelho em 5cm) ──
  const alarmeR = (DIST_ALARME/RANGE)*R;
  ctx.beginPath();
  ctx.arc(cx,cy,alarmeR,0,Math.PI*2);
  ctx.strokeStyle='#ff224425';
  ctx.setLineDash([4,8]);
  ctx.lineWidth=1;
  ctx.stroke();
  ctx.setLineDash([]);

  // Label do anel de alarme
  ctx.fillStyle='#ff224433';
  ctx.font='9px Chakra Petch, monospace';
  ctx.fillText('5cm',cx+alarmeR+4,cy-4);

  requestAnimationFrame(draw);
}

const DIST_ALARME = 5;

// ── Fetch ──
function fetchData(){
  fetch('/data').then(r=>r.text()).then(val=>{
    const d=parseFloat(val);
    if(isNaN(d))return;

    dist=d;
    document.getElementById('connStatus').textContent='Online';

    const el=document.getElementById('d');
    el.textContent=d.toFixed(1);
    el.className = (d>0&&d<=5) ? 'val danger' : 'val';

    document.getElementById('alert').style.display=(d>0&&d<=5)?'block':'none';

    if(d<99){
      if(d<minV)minV=d;
      if(d>maxV)maxV=d;
      document.getElementById('minMax').textContent=
        'Min: '+minV.toFixed(1)+' | Max: '+maxV.toFixed(1);
    }

    setTimeout(fetchData,100);
  }).catch(()=>{
    document.getElementById('connStatus').textContent='Offline';
    setTimeout(fetchData,2000);
  });
}

function toggleAlarme(){
  fetch('/toggle').then(r=>r.json()).then(data=>{
    alarmeOn=data.alarme;
    const btn=document.getElementById('btnAlarme');
    btn.textContent=alarmeOn?'🔊 Alarme: ON':'🔇 Alarme: OFF';
    btn.className=alarmeOn?'btn':'btn off';
  }).catch(()=>{});
}

draw();
fetchData();
</script>
</body>
</html>
  )rawliteral";

  return h;
}

// ═══════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  Serial.println("\n══════════════════════════════════");
  Serial.println("  SONAR RADAR v4.0 + OTA");
  Serial.println("══════════════════════════════════");

  pinMode(PINO_TRIG,   OUTPUT);
  pinMode(PINO_ECHO,   INPUT);
  pinMode(PINO_LED,    OUTPUT);
  pinMode(PINO_BUZZER, OUTPUT);
  noTone(PINO_BUZZER);
  digitalWrite(PINO_LED, LOW);

  for (int i = 0; i < FILTRO_AMOSTRAS; i++)
    bufferFiltro[i] = DIST_FALLBACK;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("WiFi");

  int t = 0;
  while (WiFi.status() != WL_CONNECTED && t < 40) {
    delay(500); Serial.print("."); t++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ " + WiFi.localIP().toString());
    Serial.println("  OTA: http://" + WiFi.localIP().toString() + "/update");
  } else {
    Serial.println("\n✗ Sem WiFi. Reiniciando...");
    delay(5000); ESP.restart();
  }

  ElegantOTA.begin(&server);
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", getHTML());
  });
  server.on("/data", HTTP_GET, []() {
    server.send(200, "text/plain", String(distanciaDisplay, 1));
  });
  server.on("/toggle", HTTP_GET, []() {
    alarmeAtivo = !alarmeAtivo;
    if (!alarmeAtivo) noTone(PINO_BUZZER);
    String json = "{\"alarme\":" + String(alarmeAtivo ? "true" : "false") + "}";
    server.send(200, "application/json", json);
  });

  server.begin();
  Serial.println("✓ Servidor OK\n");
}

// ═══════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════

void loop() {
  server.handleClient();
  ElegantOTA.loop();

  unsigned long agora = millis();
  if (agora - tempoAnterior >= INTERVALO_LEITURA) {
    tempoAnterior = agora;
    atualizarSensor();
  }
  atualizarAlarme();
}
