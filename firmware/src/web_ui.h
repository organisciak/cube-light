#pragma once

// Minimal on-chip control pages. Self-contained (no external assets) so they
// work on the cube's own AP with no internet. The full React app remains the
// rich dev UI; this is the "standing in the dust with a phone" surface.

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
  h2{font-size:14px;margin:26px 0 4px;color:#bbb}
  p{font-size:13px;color:#999;line-height:1.5;margin:6px 0}
  label{display:block;margin:14px 0 6px;font-size:13px;color:#999}
  input[type=text],input[type=password]{width:100%;box-sizing:border-box;background:#1a1a20;color:#eee;border:1px solid #333;border-radius:6px;padding:8px;font-size:15px}
  .pw{position:relative}
  .pw button{position:absolute;right:6px;top:6px;bottom:6px;background:#2a2a33;color:#bbb;border:none;border-radius:4px;padding:0 10px;font-size:12px}
  button.act{margin-top:12px;width:100%;padding:10px;border-radius:6px;border:none;background:#2a5aa5;color:#fff;font-size:15px}
  button.sec{background:#26262e;color:#ccc}
  #nets{margin:8px 0 0;padding:0;list-style:none;max-height:180px;overflow-y:auto}
  #nets li{padding:8px;background:#17171d;border:1px solid #2a2a33;border-radius:6px;margin-bottom:4px;font-size:14px;display:flex;justify-content:space-between;cursor:pointer}
  #nets li span{color:#777;font-size:12px}
  #testresult{font-size:13px;margin-top:8px;min-height:18px}
  .ok{color:#6fd66f}.bad{color:#e07a6a}
  .chk{display:flex;align-items:center;gap:8px;font-size:13px;color:#999;margin-top:10px}
  a{color:#7ab0ff}
</style></head><body>
<h1>WiFi &amp; security</h1>

<h2>Join a network</h2>
<p>Leave SSID blank to run as a standalone hotspot only. If joining fails,
the cube always brings its hotspot back within ~30 seconds — you can't
lock yourself out.</p>
<button class="act sec" id="scan">Scan for networks</button>
<ul id="nets"></ul>
<form method="POST" action="/wifi" id="f">
<label>SSID</label><input type="text" name="ssid" id="ssid" value="%SSID%">
<label>Network password</label>
<div class="pw"><input type="password" name="pass" id="pass" placeholder="(unchanged)"><button type="button" data-for="pass">show</button></div>
<button class="act sec" type="button" id="test">Test connection</button>
<div id="testresult"></div>

<h2>Hotspot &amp; flashing</h2>
<label>AP / OTA password (min 8 chars)</label>
<div class="pw"><input type="password" name="appass" id="appass" placeholder="(unchanged)"><button type="button" data-for="appass">show</button></div>

<h2>Settings console</h2>
<p>Optional password for these pages (username <b>cube</b>) — keeps others
on the same network from changing settings.</p>
<label>Console password</label>
<div class="pw"><input type="password" name="uipass" id="uipass" placeholder="%UIPASS%"><button type="button" data-for="uipass">show</button></div>
<label class="chk"><input type="checkbox" name="clearui" value="1">Remove console password</label>

<button class="act">Save &amp; reboot</button>
</form>
<p><a href="/">&larr; back</a></p>
<script>
const $=id=>document.getElementById(id);
document.querySelectorAll('.pw button').forEach(b=>{
  b.onclick=()=>{const i=$(b.dataset.for);const show=i.type==='password';i.type=show?'text':'password';b.textContent=show?'hide':'show'};
});
$('scan').onclick=async()=>{
  $('scan').textContent='Scanning…';$('scan').disabled=true;
  try{
    const nets=await (await fetch('/api/scan')).json();
    const ul=$('nets');ul.innerHTML='';
    for(const n of nets){
      const li=document.createElement('li');
      li.innerHTML=`${n.ssid}<span>${n.rssi}dBm${n.open?' · open':''}</span>`;
      li.onclick=()=>{$('ssid').value=n.ssid;$('pass').focus()};
      ul.appendChild(li);
    }
    if(!nets.length) ul.innerHTML='<li>No networks found</li>';
  }catch(e){$('nets').innerHTML='<li>Scan failed</li>'}
  $('scan').textContent='Scan for networks';$('scan').disabled=false;
};
$('test').onclick=async()=>{
  const r=$('testresult');
  r.className='';r.textContent='Testing… (if you are on the cube’s hotspot it may drop for a moment — stay on this page)';
  await fetch('/api/wifitest?ssid='+encodeURIComponent($('ssid').value)+'&pass='+encodeURIComponent($('pass').value),{method:'POST'});
  for(let i=0;i<25;i++){
    await new Promise(res=>setTimeout(res,1200));
    try{
      const s=await (await fetch('/api/wifitest')).json();
      if(s.state==='ok'){r.className='ok';r.textContent='✓ Connected — got IP '+s.ip+'. Save to make it stick.';return}
      if(s.state==='fail'){r.className='bad';r.textContent='✗ Could not join — check the password (use show) and try again.';return}
    }catch(e){/* transient AP drop while testing */}
  }
  r.className='bad';r.textContent='Test timed out.';
};
</script></body></html>)HTML";

// Guest-facing game controller: 6-direction D-pad for snake/pacman.
// Deliberately NOT auth-gated (see /api/game) so party guests can play
// without the console password.
const char kSnakeHtml[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>cube snake</title>
<style>
  body{font-family:system-ui;background:#0d0d10;color:#ddd;margin:0;padding:16px;
       display:flex;flex-direction:column;align-items:center;min-height:96vh;justify-content:center;
       -webkit-user-select:none;user-select:none;touch-action:manipulation}
  h1{font-size:16px;color:#999;margin:0 0 18px;font-weight:500}
  .pad{display:grid;grid-template-columns:repeat(3,88px);grid-template-rows:repeat(3,88px);gap:10px}
  .pad button,.zrow button{background:#1c1c24;border:1px solid #34343f;border-radius:14px;
       color:#e8e8f0;font-size:30px;touch-action:manipulation;cursor:pointer}
  .pad button:active,.zrow button:active{background:#2a5aa5}
  .blank{visibility:hidden}
  .zrow{display:flex;gap:10px;margin-top:14px;width:284px}
  .zrow button{flex:1;height:70px;font-size:20px}
  #st{margin-top:20px;font-size:13px;color:#777;min-height:16px}
</style></head><body>
<h1>cube-light · game pad</h1>
<div class="pad">
  <span class="blank"></span><button data-d="2">▲</button><span class="blank"></span>
  <button data-d="1">◀</button><button data-d="3">▼</button><button data-d="0">▶</button>
</div>
<div class="zrow"><button data-d="4">Z ▲ up</button><button data-d="5">Z ▼ down</button></div>
<div id="st"></div>
<script>
const st=document.getElementById('st');
document.querySelectorAll('button[data-d]').forEach(b=>{
  b.addEventListener('pointerdown',async e=>{
    e.preventDefault();
    if(navigator.vibrate)navigator.vibrate(8);
    try{
      const r=await fetch('/api/game?dir='+b.dataset.d,{method:'POST'});
      const t=await r.text();
      st.textContent=t==='ok'?'':t;
    }catch(err){st.textContent='connection lost — retry'}
  });
});
</script></body></html>)HTML";
