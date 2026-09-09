#pragma once

// Single-page web UI, served entirely from ESP32 flash (no external assets,
// no CDN - must work standalone in AP mode with no internet). Talks to the
// device purely via the JSON API in WebPortal.cpp.

static const char INDEX_HTML[] PROGMEM = R"HTMLPAGE(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Relaiscontroller</title>
<style>
:root{
  --bg:#0f1720; --panel:#182430; --panel2:#1f2e3d; --text:#e7edf3; --muted:#8fa2b3;
  --accent:#3ddc97; --accent-dim:#245c46; --danger:#e5566d; --border:#2a3948;
  --on-bg:#1c4b3a; --on-fg:#3ddc97; --off-bg:#3a2530; --off-fg:#e5566d;
}
*{box-sizing:border-box;}
body{margin:0;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;
  background:var(--bg);color:var(--text);}
header{padding:16px 20px;display:flex;align-items:center;justify-content:space-between;
  background:var(--panel);border-bottom:1px solid var(--border);position:sticky;top:0;z-index:10;}
header h1{font-size:1.15rem;margin:0;font-weight:600;}
#conn-pill{font-size:0.75rem;padding:4px 10px;border-radius:999px;background:var(--panel2);color:var(--muted);}
#conn-pill.ok{color:var(--on-fg);}
nav{display:flex;gap:4px;padding:10px 12px 0;background:var(--panel);overflow-x:auto;}
nav button{flex:1;min-width:80px;background:none;border:none;color:var(--muted);
  padding:10px 8px;font-size:0.85rem;border-radius:10px 10px 0 0;cursor:pointer;}
nav button.active{background:var(--bg);color:var(--accent);font-weight:600;}
main{padding:16px;max-width:720px;margin:0 auto;padding-bottom:60px;}
section[hidden]{display:none!important;}
.card{background:var(--panel);border:1px solid var(--border);border-radius:14px;
  padding:16px;margin-bottom:14px;}
.card h2{margin:0 0 10px;font-size:1rem;}
.row{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:8px;flex-wrap:wrap;}
.muted{color:var(--muted);font-size:0.85rem;}
.pill{padding:3px 10px;border-radius:999px;font-size:0.8rem;font-weight:600;}
.pill.on{background:var(--on-bg);color:var(--on-fg);}
.pill.off{background:var(--off-bg);color:var(--off-fg);}
.btn{background:var(--panel2);color:var(--text);border:1px solid var(--border);
  border-radius:8px;padding:8px 12px;font-size:0.85rem;cursor:pointer;}
.btn:active{transform:scale(0.97);}
.btn.primary{background:var(--accent);color:#06251b;border-color:var(--accent);font-weight:600;}
.btn.danger{background:var(--off-bg);color:var(--off-fg);border-color:var(--off-fg);}
.btn-row{display:flex;gap:8px;flex-wrap:wrap;margin-top:8px;}
label{display:block;font-size:0.8rem;color:var(--muted);margin:10px 0 4px;}
input[type=text],input[type=password],input[type=number],input[type=time],
input[type=datetime-local],select{
  width:100%;padding:9px 10px;border-radius:8px;border:1px solid var(--border);
  background:var(--bg);color:var(--text);font-size:0.9rem;}
.weekdays{display:flex;gap:6px;flex-wrap:wrap;margin-top:6px;}
.weekdays label{display:flex;align-items:center;gap:4px;background:var(--panel2);
  padding:6px 8px;border-radius:8px;font-size:0.78rem;color:var(--text);margin:0;}
.weekdays input{width:auto;margin:0;}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:10px;}
.hint{font-size:0.75rem;color:var(--muted);margin-top:4px;}
.toast{position:fixed;bottom:16px;left:50%;transform:translateX(-50%);
  background:var(--panel2);border:1px solid var(--border);padding:10px 16px;
  border-radius:10px;font-size:0.85rem;opacity:0;transition:opacity .3s;pointer-events:none;}
.toast.show{opacity:1;}
.channel-tabs{display:flex;gap:6px;margin-bottom:12px;flex-wrap:wrap;}
.channel-tabs button{flex:1;min-width:60px;}
</style>
</head>
<body>
<header>
  <h1>Relaiscontroller</h1>
  <span id="conn-pill">...</span>
</header>
<nav>
  <button data-tab="dashboard" class="active">Status</button>
  <button data-tab="channels">Kanäle</button>
  <button data-tab="wifi">WLAN</button>
  <button data-tab="time">Zeit</button>
</nav>
<main>
  <section id="tab-dashboard"><div id="dash-cards">Lade...</div></section>

  <section id="tab-channels" hidden>
    <div class="channel-tabs" id="ch-tabs"></div>
    <div id="ch-form-container"></div>
  </section>

  <section id="tab-wifi" hidden>
    <div class="card">
      <h2>WLAN-Status</h2>
      <div class="row"><span class="muted">Modus</span><span id="wifi-mode">-</span></div>
      <div class="row"><span class="muted">Verbindung</span><span id="wifi-conn">-</span></div>
    </div>
    <div class="card">
      <h2>Ziel-WLAN einrichten</h2>
      <label>SSID</label>
      <input type="text" id="wifi-ssid" autocomplete="off">
      <label>Passwort</label>
      <input type="password" id="wifi-pass" autocomplete="off">
      <div class="btn-row">
        <button class="btn primary" onclick="submitWifi()">Verbinden</button>
        <button class="btn danger" onclick="forceAp()">Zurück zu AP-Modus</button>
      </div>
      <p class="hint">Nach dem Verbinden ist die Oberfläche über die neue IP-Adresse
      im Zielnetzwerk erreichbar (siehe Router). Bei Fehlschlag bleibt das Gerät im AP-Modus.</p>
    </div>
    <div class="card">
      <h2>WLAN deaktivieren</h2>
      <p class="hint">Schaltet das WLAN-Modul vollständig aus. Die Relaissteuerung läuft
      unverändert nach RTC weiter. Diese Oberfläche ist danach nicht mehr erreichbar -
      zum Reaktivieren kurz den Reset-Taster drücken (normaler Verbindungsaufbau) oder
      3 Sekunden halten (direkt in den AP-Modus).</p>
      <div class="btn-row">
        <button class="btn danger" onclick="disableWifi()">WLAN deaktivieren</button>
      </div>
    </div>
  </section>

  <section id="tab-time" hidden>
    <div class="card">
      <h2>Uhrzeit</h2>
      <div class="row"><span class="muted">RTC-Status</span><span id="time-valid">-</span></div>
      <div class="row"><span class="muted">Aktuelle Zeit</span><span id="time-now">-</span></div>
      <div class="row"><span class="muted">Letzter NTP-Sync</span><span id="time-ntp">-</span></div>
    </div>
    <div class="card">
      <h2>Manuell setzen</h2>
      <label>Datum &amp; Uhrzeit</label>
      <input type="datetime-local" id="time-input" step="1">
      <div class="btn-row">
        <button class="btn primary" onclick="submitTime()">Übernehmen</button>
        <button class="btn" onclick="document.getElementById('time-input').value=localIsoNow()">Jetzt</button>
      </div>
    </div>
  </section>
</main>
<div class="toast" id="toast"></div>

<script>
const WEEKDAY_LABELS = ["So","Mo","Di","Mi","Do","Fr","Sa"]; // bit i = 1<<i, matches tm_wday
const MODE_NAMES = {0:"Deaktiviert",1:"Intervall",2:"Feste Tageszeit",3:"Sonnenauf-/untergang"};
let currentChannel = 0;
let configCache = null;

function $(id){return document.getElementById(id);}

function toast(msg){
  const t = $('toast');
  t.textContent = msg;
  t.classList.add('show');
  setTimeout(()=>t.classList.remove('show'), 2200);
}

function pad(n){return n.toString().padStart(2,'0');}
function minToHHMM(min){min=((min%1440)+1440)%1440;return pad(Math.floor(min/60))+':'+pad(min%60);}
function hhmmToMin(s){const [h,m]=s.split(':').map(Number);return h*60+m;}
function localIsoNow(){
  const d=new Date(); d.setSeconds(0,0);
  d.setMinutes(d.getMinutes()-d.getTimezoneOffset());
  return d.toISOString().slice(0,19);
}

// ---------------- Navigation ----------------
document.querySelectorAll('nav button').forEach(btn=>{
  btn.addEventListener('click', ()=>{
    document.querySelectorAll('nav button').forEach(b=>b.classList.remove('active'));
    btn.classList.add('active');
    document.querySelectorAll('main section').forEach(s=>s.hidden = true);
    $('tab-'+btn.dataset.tab).hidden = false;
    if(btn.dataset.tab==='wifi') loadWifi();
    if(btn.dataset.tab==='time') loadTime();
  });
});

// ---------------- Dashboard ----------------
async function loadStatus(){
  try{
    const r = await fetch('/api/status');
    const d = await r.json();
    $('conn-pill').textContent = d.rtcValid ? 'RTC OK' : 'RTC UNGÜLTIG';
    $('conn-pill').className = d.rtcValid ? 'ok' : '';
    // Preserve any in-progress duration input across this refresh - the
    // periodic poll below would otherwise blow away what the user just typed.
    const prevDur = d.channels.map(c => ({
      min: document.getElementById('dur-min-'+c.index)?.value,
      sec: document.getElementById('dur-sec-'+c.index)?.value,
    }));
    const html = d.channels.map((c,i)=>`
      <div class="card">
        <div class="row">
          <h2 style="margin:0">Kanal ${c.index+1}</h2>
          <span class="pill ${c.on?'on':'off'}">${c.on?'AN':'AUS'}</span>
        </div>
        <div class="row"><span class="muted">Modus</span><span>${c.modeName}</span></div>
        <div class="row"><span class="muted">Nächste Schaltzeit</span><span>${c.nextSwitch||'-'}</span></div>
        ${c.override.active?`<div class="row"><span class="muted">Override</span>
          <span>${c.override.state?'AN':'AUS'} ${c.override.indefinite?'(bis auf Weiteres)':'bis '+c.override.endsAt}</span></div>`:''}
        <div class="row">
          <span class="muted">Override-Dauer (0:0 = bis auf Weiteres)</span>
          <span style="display:flex;gap:4px;align-items:center">
            <input type="number" min="0" value="${prevDur[i].min ?? 5}" id="dur-min-${c.index}" style="width:60px" title="Minuten">
            <span class="muted">Min</span>
            <input type="number" min="0" max="59" value="${prevDur[i].sec ?? 0}" id="dur-sec-${c.index}" style="width:60px" title="Sekunden">
            <span class="muted">Sek</span>
          </span>
        </div>
        <div class="btn-row">
          <button class="btn" onclick="override(${c.index},'on')">Override AN</button>
          <button class="btn" onclick="override(${c.index},'off')">Override AUS</button>
          ${c.override.active?`<button class="btn danger" onclick="override(${c.index},'clear')">Aufheben</button>`:''}
        </div>
      </div>`).join('');
    $('dash-cards').innerHTML = html;
  }catch(e){
    $('conn-pill').textContent = 'Offline';
    $('conn-pill').className = '';
  }
}

async function override(index, action){
  const minutes = parseInt($('dur-min-'+index).value, 10) || 0;
  const seconds = parseInt($('dur-sec-'+index).value, 10) || 0;
  await fetch('/api/override', {method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({index, action, durationSeconds: minutes*60 + seconds})});
  toast('Override aktualisiert');
  loadStatus();
}

// ---------------- Channels ----------------
function renderChannelTabs(){
  $('ch-tabs').innerHTML = [0,1,2,3].map(i=>
    `<button class="btn ${i===currentChannel?'primary':''}" onclick="selectChannel(${i})">Kanal ${i+1}</button>`).join('');
}

function selectChannel(i){
  currentChannel = i;
  renderChannelTabs();
  renderChannelForm();
}

function renderChannelForm(){
  const c = configCache.channels[currentChannel];
  const wdBoxes = WEEKDAY_LABELS.map((lbl,i)=>
    `<label><input type="checkbox" data-wd="${i}" ${(c.weekdayMask&(1<<i))?'checked':''}>${lbl}</label>`).join('');

  $('ch-form-container').innerHTML = `
    <div class="card">
      <label>Modus</label>
      <select id="f-mode">
        <option value="0" ${c.mode==0?'selected':''}>Deaktiviert</option>
        <option value="1" ${c.mode==1?'selected':''}>Intervall</option>
        <option value="2" ${c.mode==2?'selected':''}>Feste Tageszeit</option>
        <option value="3" ${c.mode==3?'selected':''}>Sonnenauf-/untergang</option>
      </select>

      <label>Wochentage</label>
      <div class="weekdays" id="f-weekdays">${wdBoxes}</div>

      <div id="mode-interval" hidden>
        <div class="grid2">
          <div><label>Intervall (Min)</label><input type="number" id="f-intervalMinutes" min="1" value="${c.intervalMinutes}"></div>
          <div><label>Einschaltdauer (Sek)</label><input type="number" id="f-onSeconds" min="1" value="${c.onSeconds}"></div>
        </div>
        <label><input type="checkbox" id="f-windowEnabled" ${c.windowEnabled?'checked':''} style="width:auto;display:inline-block;margin-right:6px;">Zeitfenster einschränken</label>
        <div class="grid2">
          <div><label>Von</label><input type="time" id="f-windowStart" value="${minToHHMM(c.windowStartMin)}"></div>
          <div><label>Bis</label><input type="time" id="f-windowEnd" value="${minToHHMM(c.windowEndMin===1440?0:c.windowEndMin)}"></div>
        </div>
      </div>

      <div id="mode-fixed" hidden>
        <div class="grid2">
          <div><label>Einschaltzeit</label><input type="time" id="f-fixedOnMin" value="${minToHHMM(c.fixedOnMin)}"></div>
          <div><label>Ausschaltzeit</label><input type="time" id="f-fixedOffMin" value="${minToHHMM(c.fixedOffMin)}"></div>
        </div>
      </div>

      <div id="mode-sun" hidden>
        <label>Verhalten</label>
        <select id="f-onAtSunrise">
          <option value="0" ${!c.onAtSunrise?'selected':''}>AN bei Sonnenuntergang, AUS bei Sonnenaufgang</option>
          <option value="1" ${c.onAtSunrise?'selected':''}>AN bei Sonnenaufgang, AUS bei Sonnenuntergang</option>
        </select>
        <div class="grid2">
          <div><label>Offset Sonnenaufgang (Min)</label><input type="number" id="f-sunriseOffsetMin" value="${c.sunriseOffsetMin}"></div>
          <div><label>Offset Sonnenuntergang (Min)</label><input type="number" id="f-sunsetOffsetMin" value="${c.sunsetOffsetMin}"></div>
        </div>
        <p class="hint">Negativer Offset = früher, positiver Offset = später. Referenzort: Bern.</p>
      </div>

      <div class="btn-row"><button class="btn primary" onclick="saveChannel()">Speichern</button></div>
    </div>`;

  $('f-mode').addEventListener('change', updateModeVisibility);
  updateModeVisibility();
}

function updateModeVisibility(){
  const mode = parseInt($('f-mode').value, 10);
  $('mode-interval').hidden = mode !== 1;
  $('mode-fixed').hidden = mode !== 2;
  $('mode-sun').hidden = mode !== 3;
}

async function loadConfig(){
  const r = await fetch('/api/config');
  configCache = await r.json();
  renderChannelTabs();
  renderChannelForm();
}

async function saveChannel(){
  let weekdayMask = 0;
  document.querySelectorAll('#f-weekdays input').forEach(cb=>{
    if(cb.checked) weekdayMask |= (1 << parseInt(cb.dataset.wd,10));
  });
  const body = {
    index: currentChannel,
    mode: parseInt($('f-mode').value,10),
    weekdayMask,
    windowEnabled: $('f-windowEnabled').checked,
    windowStartMin: hhmmToMin($('f-windowStart').value||'00:00'),
    windowEndMin: hhmmToMin($('f-windowEnd').value||'00:00') || 1440,
    intervalMinutes: parseInt($('f-intervalMinutes').value,10)||1,
    onSeconds: parseInt($('f-onSeconds').value,10)||1,
    fixedOnMin: hhmmToMin($('f-fixedOnMin').value||'00:00'),
    fixedOffMin: hhmmToMin($('f-fixedOffMin').value||'00:00'),
    sunriseOffsetMin: parseInt($('f-sunriseOffsetMin').value,10)||0,
    sunsetOffsetMin: parseInt($('f-sunsetOffsetMin').value,10)||0,
    onAtSunrise: $('f-onAtSunrise').value === '1'
  };
  await fetch('/api/config', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(body)});
  toast('Kanal ' + (currentChannel+1) + ' gespeichert');
  loadConfig();
  loadStatus();
}

// ---------------- WiFi ----------------
async function loadWifi(){
  const r = await fetch('/api/wifi');
  const d = await r.json();
  $('wifi-mode').textContent = d.mode;
  $('wifi-conn').textContent = d.summary;
}

async function submitWifi(){
  const ssid = $('wifi-ssid').value.trim();
  const password = $('wifi-pass').value;
  if(!ssid){toast('SSID erforderlich');return;}
  await fetch('/api/wifi', {method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({ssid, password})});
  toast('Verbindungsversuch gestartet...');
  setTimeout(loadWifi, 4000);
}

async function forceAp(){
  await fetch('/api/wifi', {method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({forceAp:true})});
  toast('AP-Modus aktiviert');
  setTimeout(loadWifi, 1500);
}

async function disableWifi(){
  if(!confirm('WLAN wirklich deaktivieren? Diese Oberfläche ist danach nicht mehr erreichbar, ' +
    'bis der Reset-Taster gedrückt wird. Die Relais laufen unverändert weiter.')) return;
  await fetch('/api/wifi', {method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({disableWifi:true})});
  toast('WLAN wird deaktiviert...');
}

// ---------------- Time ----------------
async function loadTime(){
  const r = await fetch('/api/time');
  const d = await r.json();
  $('time-valid').textContent = d.rtcValid ? 'Gültig' : 'UNGÜLTIG (Failsafe aktiv)';
  $('time-now').textContent = d.now || '-';
  $('time-ntp').textContent = d.ntpLastSync || 'Noch nie';
}

async function submitTime(){
  const v = $('time-input').value;
  if(!v){toast('Bitte Zeit wählen');return;}
  const [datePart, timePart] = v.split('T');
  const [year,month,day] = datePart.split('-').map(Number);
  const [hour,minute,second] = timePart.split(':').map(Number);
  await fetch('/api/time', {method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({year,month,day,hour,minute,second: second||0})});
  toast('Zeit gesetzt');
  loadTime();
}

// ---------------- Init ----------------
loadConfig();
loadStatus();
setInterval(loadStatus, 5000);
</script>
</body>
</html>
)HTMLPAGE";
