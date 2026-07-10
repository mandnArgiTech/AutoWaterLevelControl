/**
 * @file BatteryCalibratePage.h
 * @brief A0 battery voltage calibration page (from ADCA1115Calibration)
 */
#ifndef BATTERY_CALIBRATE_PAGE_H
#define BATTERY_CALIBRATE_PAGE_H

#include <Arduino.h>

static const char WEB_PAGE_BATTERY_CALIBRATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Battery A0 Calibration</title>
<style>
:root{--bg:#eef2f7;--card:#fff;--text:#1e293b;--muted:#64748b;--accent:#0284c7;--border:#dbe3ee;--ok:#059669;--err:#dc2626;--radius:12px}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;font-size:16px;line-height:1.5;background:var(--bg);color:var(--text);min-height:100vh}
.wrap{max-width:720px;margin:0 auto;padding:20px}
a{color:var(--accent);text-decoration:none;font-weight:600}
header{margin-bottom:20px}
h1{font-size:1.5rem;font-weight:650}
.sub{color:var(--muted);margin-top:4px}
.card{background:var(--card);border:1px solid var(--border);border-radius:var(--radius);padding:20px;margin-bottom:16px}
.card h2{font-size:.8rem;text-transform:uppercase;letter-spacing:.06em;color:var(--muted);margin-bottom:14px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:14px}
@media(max-width:560px){.grid{grid-template-columns:1fr}}
.metric{font-size:1.6rem;font-weight:700}
.unit{font-size:.9rem;color:var(--muted);font-weight:500;margin-left:4px}
label{display:block;font-size:.85rem;color:var(--muted);margin-bottom:6px}
input{width:100%;padding:10px 12px;border:1px solid var(--border);border-radius:8px;font-size:1rem;background:#f8fafc}
.actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px}
button{padding:10px 16px;border:none;border-radius:8px;background:var(--accent);color:#fff;font-size:.95rem;font-weight:600;cursor:pointer}
button.secondary{background:#fff;color:var(--text);border:1px solid var(--border)}
button:disabled{opacity:.6;cursor:not-allowed}
.status{font-size:.9rem;margin-top:12px;min-height:1.3em}
.status.ok{color:var(--ok)}.status.err{color:var(--err)}
.hint{font-size:.9rem;color:var(--muted);margin-top:8px}
.nav{margin-bottom:16px}
</style>
</head>
<body>
<div class="wrap">
<p class="nav"><a href="/">&larr; Dashboard</a></p>
<header>
<h1>Battery A0 Calibration</h1>
<p class="sub">Match the reading to your multimeter (D1 Mini A0 path)</p>
</header>
<div class="card">
<h2>Live Reading</h2>
<div class="grid">
<div><div class="hint">Pack voltage</div><div class="metric" id="measV">--<span class="unit">V</span></div></div>
<div><div class="hint">Current offset</div><div class="metric" id="offV">--<span class="unit">V</span></div></div>
<div><div class="hint">ADC raw</div><div class="metric" id="rawV">--</div></div>
<div><div class="hint">A0 pin</div><div class="metric" id="pinV">--<span class="unit">V</span></div></div>
</div>
</div>
<div class="card">
<h2>Calibrate</h2>
<label for="actual">Actual pack voltage from multimeter (V)</label>
<input type="number" id="actual" step="0.001" min="0.1" max="30" placeholder="e.g. 6.412">
<p class="hint">Enter the true pack voltage, then Calibrate. Firmware averages 16 samples and saves the offset.</p>
<div class="actions">
<button id="calBtn" type="button">Calibrate</button>
<button class="secondary" id="resetBtn" type="button">Reset Offset</button>
<button class="secondary" id="refreshBtn" type="button">Refresh</button>
</div>
<p class="status" id="calStatus"></p>
</div>
</div>
<script>
(function(){
const $=id=>document.getElementById(id);
async function load(){
try{
const r=await fetch('/api/battery/calibrate');
const j=await r.json();
if(!j.ok){$('calStatus').className='status err';$('calStatus').textContent=j.error||'Failed';return}
$('measV').innerHTML=(j.measured!=null?j.measured.toFixed(3):'--')+'<span class="unit">V</span>';
$('offV').innerHTML=(j.offset!=null?j.offset.toFixed(3):'--')+'<span class="unit">V</span>';
$('rawV').textContent=j.raw!=null?j.raw:'--';
$('pinV').innerHTML=(j.a0PinV!=null?j.a0PinV.toFixed(3):'--')+'<span class="unit">V</span>';
}catch(e){$('calStatus').className='status err';$('calStatus').textContent='Cannot reach device'}
}
async function calibrate(reset){
const actual=reset?null:parseFloat($('actual').value);
if(!reset&&(!(actual>0))){$('calStatus').className='status err';$('calStatus').textContent='Enter a valid multimeter voltage';return}
$('calBtn').disabled=true;$('resetBtn').disabled=true;
$('calStatus').className='status';$('calStatus').textContent=reset?'Resetting...':'Calibrating (averaging samples)...';
try{
const body=reset?{reset:true}:{actual:actual};
const r=await fetch('/api/battery/calibrate',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
const j=await r.json();
if(j.ok){
$('calStatus').className='status ok';
$('calStatus').textContent=reset
?'Offset cleared'
:('Calibrated: measured '+j.measured.toFixed(3)+' V → actual '+j.actual.toFixed(3)+' V (offset '+j.offset.toFixed(3)+' V)');
await load();
}else{$('calStatus').className='status err';$('calStatus').textContent=j.error||'Calibration failed'}
}catch(e){$('calStatus').className='status err';$('calStatus').textContent='Request failed'}
$('calBtn').disabled=false;$('resetBtn').disabled=false;
}
$('calBtn').onclick=()=>calibrate(false);
$('resetBtn').onclick=()=>calibrate(true);
$('refreshBtn').onclick=load;
load();
setInterval(load,2000);
})();
</script>
</body>
</html>
)rawliteral";

#endif // BATTERY_CALIBRATE_PAGE_H
