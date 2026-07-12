#pragma once

// Minimal on-chip control page. Self-contained (no external assets) so it
// works on the cube's own AP with no internet. The full React app remains
// the rich dev UI; this is the "standing in the dust with a phone" surface.

const char kIndexHtml[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>cube-light</title>
<style>
  body{font-family:system-ui;background:#0d0d10;color:#ddd;margin:0;padding:24px;max-width:420px;margin:auto}
  h1{font-size:18px;margin:12px 0 20px}
  label{display:block;margin:16px 0 6px;font-size:13px;color:#999}
  select,input[type=range],input[type=number]{width:100%;box-sizing:border-box;background:#1a1a20;color:#eee;border:1px solid #333;border-radius:6px;padding:8px;font-size:15px}
  .row{display:flex;gap:8px;align-items:center}
  .row output{min-width:48px;text-align:right;font-variant-numeric:tabular-nums}
  a{color:#7ab0ff}
  .stat{font-size:12px;color:#777;margin-top:24px;line-height:1.7}
</style></head><body>
<h1>cube-light</h1>
<label>Pattern</label>
<select id="pattern"></select>
<label>Brightness</label>
<div class="row"><input type="range" id="bright" min="0" max="100" step="1"><output id="brightv"></output></div>
<label>Power budget (mA, 0 = no limit)</label>
<input type="number" id="supply" min="0" step="100">
<label>Color order</label>
<select id="order"><option>RGB</option><option>GRB</option><option>BRG</option><option>RBG</option><option>GBR</option><option>BGR</option></select>
<div class="stat" id="stat"></div>
<p><a href="/wifi">WiFi &amp; security settings</a></p>
<script>
const $=id=>document.getElementById(id);
async function post(url){await fetch(url,{method:'POST'})}
async function refresh(){
  const s=await (await fetch('/api/status')).json();
  const sel=$('pattern');
  if(sel.options.length===0) for(const p of s.patterns){const o=document.createElement('option');o.value=o.textContent=p;sel.appendChild(o)}
  sel.value=s.pattern;
  $('bright').value=Math.round(s.brightness*100);$('brightv').textContent=$('bright').value+'%';
  $('supply').value=s.supplyMA; $('order').value=s.colorOrder;
  $('stat').textContent=`ip ${s.ip} · rssi ${s.rssi}dBm · ${s.fps}fps target · v${s.version}`;
}
$('pattern').onchange=e=>post('/api/pattern?id='+encodeURIComponent(e.target.value));
$('bright').oninput=e=>{$('brightv').textContent=e.target.value+'%'};
$('bright').onchange=e=>post('/api/brightness?v='+(e.target.value/100));
$('supply').onchange=e=>post('/api/supply?ma='+e.target.value);
$('order').onchange=e=>post('/api/order?v='+e.target.value);
refresh();
</script></body></html>)HTML";

const char kWifiHtml[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>cube-light wifi</title>
<style>
  body{font-family:system-ui;background:#0d0d10;color:#ddd;margin:0;padding:24px;max-width:420px;margin:auto}
  h1{font-size:18px;margin:12px 0 8px}
  p{font-size:13px;color:#999;line-height:1.5}
  label{display:block;margin:14px 0 6px;font-size:13px;color:#999}
  input{width:100%;box-sizing:border-box;background:#1a1a20;color:#eee;border:1px solid #333;border-radius:6px;padding:8px;font-size:15px}
  button{margin-top:20px;width:100%;padding:10px;border-radius:6px;border:none;background:#2a5aa5;color:#fff;font-size:15px}
  a{color:#7ab0ff}
</style></head><body>
<h1>WiFi &amp; security</h1>
<p>Leave SSID blank to run as a standalone access point. The AP/OTA
password protects both the "cube-light" hotspot and wireless flashing
(min 8 characters). The cube reboots after saving.</p>
<form method="POST" action="/wifi">
<label>Join network — SSID</label><input name="ssid" value="%SSID%">
<label>Network password</label><input name="pass" type="password" placeholder="(unchanged)">
<label>AP / OTA password</label><input name="appass" type="password" placeholder="(unchanged)">
<button>Save &amp; reboot</button>
</form>
<p><a href="/">&larr; back</a></p>
</body></html>)HTML";
