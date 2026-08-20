/**
 * @file DistanceCalibratePage.h
 * @brief Standalone raw-distance calibration (no filter) — tape measure vs sensor
 */
#ifndef DISTANCE_CALIBRATE_PAGE_H
#define DISTANCE_CALIBRATE_PAGE_H

#include <Arduino.h>

static const char WEB_PAGE_DISTANCE_CALIBRATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Distance Calibration</title>
<style>
:root{--bg:#eef2f7;--card:#fff;--ink:#1e293b;--muted:#64748b;--accent:#0284c7;--line:#dbe3ee;--ok:#059669;--err:#dc2626;--warn:#d97706}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;font-size:16px;line-height:1.45;background:var(--bg);color:var(--ink);min-height:100vh}
.wrap{max-width:640px;margin:0 auto;padding:18px}
a{color:var(--accent);font-weight:600;text-decoration:none}
header{margin-bottom:16px}
h1{font-size:1.45rem;font-weight:750}
.sub{color:var(--muted);margin-top:4px;font-size:.95rem}
.card{background:var(--card);border:2px solid var(--line);border-radius:14px;padding:18px;margin-bottom:14px}
.card h2{font-size:.8rem;text-transform:uppercase;letter-spacing:.05em;color:var(--muted);margin-bottom:12px}
.big{font-size:3rem;font-weight:800;color:var(--accent);line-height:1}
.unit{font-size:1rem;font-weight:600;color:var(--muted);margin-left:4px}
.row{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:12px}
@media(max-width:520px){.row{grid-template-columns:1fr}}
.stat{background:var(--bg);border:2px solid var(--line);border-radius:10px;padding:12px}
.stat .k{font-size:.8rem;font-weight:700;color:var(--muted);text-transform:uppercase}
.stat .v{font-size:1.45rem;font-weight:800;margin-top:2px}
label{display:block;font-size:.95rem;font-weight:600;color:var(--muted);margin-bottom:6px}
input{width:100%;padding:12px;border:2px solid var(--line);border-radius:10px;font-size:1.1rem;background:#fff}
input:focus{outline:none;border-color:var(--accent)}
.btns{display:flex;flex-wrap:wrap;gap:10px;margin-top:14px}
button{padding:12px 18px;border:none;border-radius:10px;background:var(--accent);color:#fff;font-size:1rem;font-weight:700;cursor:pointer}
button.sec{background:#eaf0f6;color:var(--ink);border:2px solid var(--line)}
button:disabled{opacity:.55;cursor:wait}
.hint{font-size:.92rem;color:var(--muted);margin-top:10px}
.status{margin-top:12px;min-height:1.3em;font-weight:600}
.status.ok{color:var(--ok)}.status.err{color:var(--err)}.status.warn{color:var(--warn)}
.pill{display:inline-block;padding:4px 10px;border-radius:99px;font-size:.8rem;font-weight:700;background:#e0f2fe;color:#0369a1}
.steps{margin:0;padding-left:1.2em;color:var(--muted);font-size:.95rem}
.steps li{margin:6px 0}
</style>
</head>
<body>
<div class="wrap">
  <header>
    <p><a href="/">← Back to monitor</a></p>
    <h1>Distance calibration</h1>
    <p class="sub">Raw sensor only — no median / Kalman filter. Units: <b>mm</b></p>
  </header>

  <div class="card">
    <h2>Live raw reading <span class="pill" id="poll">…</span></h2>
    <div class="big"><span id="raw">—</span><span class="unit">mm</span></div>
    <div class="row">
      <div class="stat"><div class="k">Current offset</div><div class="v" id="offset">—<span class="unit">mm</span></div></div>
      <div class="stat"><div class="k">With offset</div><div class="v" id="cal">—<span class="unit">mm</span></div></div>
    </div>
    <p class="hint">This is the unfiltered hardware distance. Aim the sensor at a fixed surface and wait until the number is steady.</p>
  </div>

  <div class="card">
    <h2>Tape measure</h2>
    <ol class="steps">
      <li>Measure the same distance with a tape (sensor face → surface).</li>
      <li>Enter that value in mm below.</li>
      <li>Press <b>Calculate &amp; save</b> — offset = tape − raw.</li>
    </ol>
    <div style="margin-top:14px">
      <label for="tape">Measured distance (mm)</label>
      <input id="tape" type="number" inputmode="decimal" step="1" min="1" max="10000" placeholder="e.g. 1200">
    </div>
    <div class="row" style="margin-top:12px">
      <div class="stat"><div class="k">Computed offset</div><div class="v" id="computed">—<span class="unit">mm</span></div></div>
      <div class="stat"><div class="k">Corrected distance</div><div class="v" id="corrected">—<span class="unit">mm</span></div></div>
    </div>
    <div class="btns">
      <button id="btn-calc" onclick="saveCalibration()">Calculate &amp; save</button>
      <button class="sec" onclick="clearOffset()">Reset offset to 0</button>
    </div>
    <div class="status" id="status"></div>
  </div>
</div>
<script>
const $=id=>document.getElementById(id);
const n=(v,d=0)=>Number.isFinite(+v)?(+v).toFixed(d):'—';
let lastRaw=null, busy=false;

function recompute(){
  const tape=parseFloat($('tape').value);
  if(!Number.isFinite(tape)||lastRaw==null){
    $('computed').innerHTML='—<span class="unit">mm</span>';
    $('corrected').innerHTML='—<span class="unit">mm</span>';
    return;
  }
  const off=tape-lastRaw;
  $('computed').innerHTML=n(off,1)+'<span class="unit">mm</span>';
  $('corrected').innerHTML=n(lastRaw+off,0)+'<span class="unit">mm</span>';
}
$('tape').addEventListener('input',recompute);

async function tick(){
  if(busy) return;
  busy=true;
  try{
    const r=await fetch('/api/calibrate/distance',{cache:'no-store'});
    const d=await r.json();
    if(!r.ok) throw new Error(d.message||('HTTP '+r.status));
    lastRaw=(d.rawMm!=null)?+d.rawMm:null;
    $('raw').textContent=lastRaw!=null?n(lastRaw,0):'—';
    $('offset').innerHTML=n(d.offsetMm,1)+'<span class="unit">mm</span>';
    $('cal').innerHTML=(d.calibratedMm!=null?n(d.calibratedMm,0):'—')+'<span class="unit">mm</span>';
    $('poll').textContent=d.ok?'Live raw':'No reading';
    $('poll').style.background=d.ok?'#d1fae5':'#fee2e2';
    $('poll').style.color=d.ok?'#065f46':'#991b1b';
    recompute();
  }catch(e){
    $('poll').textContent='Offline';
    $('status').className='status err';
    $('status').textContent=e.message||'Poll failed';
  }finally{busy=false;}
}

async function saveCalibration(){
  const tape=parseFloat($('tape').value);
  if(!Number.isFinite(tape)||tape<=0){
    $('status').className='status err';
    $('status').textContent='Enter a valid tape measurement in mm';
    return;
  }
  $('btn-calc').disabled=true;
  $('status').className='status warn';
  $('status').textContent='Saving…';
  try{
    const r=await fetch('/api/calibrate/distance',{
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({measuredMm:tape})
    });
    const d=await r.json().catch(()=>({}));
    if(!r.ok||d.success===false){
      throw new Error(d.message||'Save failed');
    }
    $('status').className='status ok';
    $('status').textContent='Saved offset '+n(d.offsetMm,1)+' mm  (raw '+n(d.rawMm,0)+' → tape '+n(d.measuredMm,0)+')';
    await tick();
  }catch(e){
    $('status').className='status err';
    $('status').textContent=e.message||'Save failed';
  }finally{$('btn-calc').disabled=false;}
}

async function clearOffset(){
  if(!confirm('Reset calibration offset to 0?')) return;
  $('btn-calc').disabled=true;
  try{
    const r=await fetch('/api/calibrate/distance',{
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({offsetMm:0})
    });
    const d=await r.json().catch(()=>({}));
    if(!r.ok||d.success===false) throw new Error(d.message||'Reset failed');
    $('tape').value='';
    $('status').className='status ok';
    $('status').textContent='Offset reset to 0';
    await tick();
  }catch(e){
    $('status').className='status err';
    $('status').textContent=e.message||'Reset failed';
  }finally{$('btn-calc').disabled=false;}
}

tick();
setInterval(tick,800);
</script>
</body>
</html>
)rawliteral";

#endif // DISTANCE_CALIBRATE_PAGE_H
