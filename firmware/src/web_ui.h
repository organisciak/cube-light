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
  nav{display:flex;gap:14px;font-size:13px;margin-bottom:14px}
  nav a{color:#7ab0ff;text-decoration:none}
  nav b{color:#eee}
  details{margin-top:22px;border-top:1px solid #222;padding-top:8px}
  summary{font-size:14px;color:#bbb;cursor:pointer;padding:6px 0}
  .prow{display:flex;gap:10px;align-items:center;margin:10px 0}
  .prow label{flex:0 0 46%;margin:0}
  .prow input[type=range]{flex:1}
  .prow select,.prow input[type=text]{flex:1}
  .prow output{min-width:44px;text-align:right;font-size:12px;color:#aaa}
  .meter{height:10px;background:#1a1a20;border-radius:5px;overflow:hidden;flex:1}
  .meter div{height:100%;background:#2a5aa5;width:0%}
  button{padding:8px 12px;border-radius:6px;border:none;background:#26262e;color:#ccc;font-size:13px;cursor:pointer}
  button.pri{background:#2a5aa5;color:#fff}
</style></head><body>
<nav><b>Console</b><a href="/snake">Game pad</a><a href="/admin" id="adminLink">Admin 🔒</a></nav>
<h1>cube-light</h1>
<div id="guestBanner" style="display:none;background:#123a20;border:1px solid #2a8050;color:#8fe0a8;border-radius:6px;padding:10px;font-size:13px;margin-bottom:12px">
🎉 Feel free to tinker! Pick any pattern and play with the knobs. You can't
break anything permanent — just reset when you're done.</div>
<div id="liveBanner" style="display:none;background:#5a3a10;border:1px solid #a06820;color:#f0c070;border-radius:6px;padding:10px;font-size:13px;margin-bottom:12px">
⚠ An external stream (dev server?) is driving the cube right now — the
pattern below won't show until it stops.</div>
<label>Pattern</label>
<select id="pattern"></select>

<label>Mic reactivity</label>
<button id="micToggle" style="width:100%;padding:10px;font-size:15px"></button>

<details open id="paramsBox"><summary>Pattern parameters</summary>
<div id="params"></div>
<div class="prow">
  <button id="resetParams">Reset to defaults</button>
</div>
</details>

<details id="presetsBox"><summary>Presets</summary>
<div id="presetList"></div>
<div class="prow" id="presetSave">
  <input type="text" id="presetName" placeholder="Save current as…">
  <button id="presetSaveBtn" class="pri">Save</button>
</div>
</details>

<button class="pri" id="guestReset" style="display:none;width:100%;padding:12px;margin-top:18px">Alright, broken it in enough? Set it back →</button>

<div class="stat" id="stat"></div>
<script>
const $=id=>document.getElementById(id);
async function post(url){await fetch(url,{method:'POST'})}
async function refresh(){
  const s=await (await fetch('/api/status')).json();
  const sel=$('pattern');
  if(sel.options.length===0) for(const p of s.patterns){const o=document.createElement('option');o.value=o.textContent=p;sel.appendChild(o)}
  sel.value=s.pattern;
  micOn=s.micOn; drawMic();
  isGuest=!!s.guest;
  $('liveBanner').style.display=s.live?'block':'none';
  // Guests (on the cube's own hotspot) can tinker but not clobber saved
  // settings — hide the admin door, offer a friendly "set it back" button.
  if(s.guest){
    $('guestBanner').style.display='block';
    $('guestReset').style.display='block';
    $('adminLink').style.display='none';
  }
  $('stat').textContent=`ip ${s.ip} · rssi ${s.rssi}dBm · ${s.fps}fps target · v${s.version}`;
  loadParams();
  loadPresets();
}
let micOn=true;
let isGuest=false;
function drawMic(){
  const b=$('micToggle');
  b.textContent=micOn?'🎤 ON — sound drives the patterns':'🔇 OFF — patterns ignore sound';
  b.style.background=micOn?'#2a5aa5':'#26262e';
  b.style.color=micOn?'#fff':'#999';
}
$('micToggle').onclick=async()=>{micOn=!micOn;drawMic();await post('/api/mic?on='+(micOn?1:0))};
const T={NUM:0,BOOL:1,ENUM:2,PAL:3,STR:4};
// Bucket a param by key/type into a Pattern/Audio/Color subsection. Pure
// JS heuristic — the C++ ParamSpec struct is untouched.
function paramGroup(sp){
  const k=sp.key;
  if(sp.type===T.PAL||sp.type===5||k==='r'||k==='g'||k==='b'||k==='sat'||k==='pos'||k==='palette'||/[RGB]$/.test(k)||/[Cc]olor|Tint|hue/.test(k))return 'Color';
  if(/Gain|[Bb]eat|[Ll]evel|[Aa]udio|mic|attack|release|gamma/.test(k))return 'Audio';
  return 'Pattern';
}
function makeRow(sp,d,group){
  const row=document.createElement('div');row.className='prow';
  // Audio-group params drive another param — audio can push it past the max.
  if(group==='Audio')row.title='Audio can push this parameter beyond the slider maximum (up to ~2x by default).';
  const lab=document.createElement('label');lab.textContent=sp.label;row.appendChild(lab);
  let ctl,out=null;
  if(sp.type===T.BOOL){
    ctl=document.createElement('input');ctl.type='checkbox';ctl.checked=sp.value==='1';
    ctl.onchange=()=>post(`/api/param?type=bool&key=${sp.key}&v=${ctl.checked?1:0}`);
  }else if(sp.type===T.ENUM||sp.type===T.PAL){
    ctl=document.createElement('select');
    const opts=sp.type===T.PAL?d.palettes:sp.options.split(',');
    for(const o of opts){const e=document.createElement('option');e.value=e.textContent=o;ctl.appendChild(e)}
    ctl.value=sp.value;
    ctl.onchange=()=>post(`/api/param?type=str&key=${sp.key}&v=${encodeURIComponent(ctl.value)}`);
  }else if(sp.type===T.STR){
    ctl=document.createElement('input');ctl.type='text';ctl.value=sp.value;
    ctl.onchange=()=>post(`/api/param?type=str&key=${sp.key}&v=${encodeURIComponent(ctl.value)}`);
  }else{
    ctl=document.createElement('input');ctl.type='range';
    ctl.min=sp.min;ctl.max=sp.max;ctl.step=sp.step||0.01;ctl.value=parseFloat(sp.value);
    out=document.createElement('output');out.textContent=(+sp.value).toFixed(2).replace(/\.?0+$/,'');
    ctl.oninput=()=>{out.textContent=(+ctl.value).toFixed(2).replace(/\.?0+$/,'')};
    ctl.onchange=()=>post(`/api/param?type=num&key=${sp.key}&v=${ctl.value}`);
  }
  row.appendChild(ctl);if(out)row.appendChild(out);
  return row;
}
async function loadParams(){
  const d=await (await fetch('/api/params')).json();
  const box=$('params');box.innerHTML='';
  const groups={Pattern:[],Audio:[],Color:[]};
  for(const sp of d.specs)groups[paramGroup(sp)].push(sp);
  for(const g of ['Pattern','Audio','Color']){
    if(!groups[g].length)continue;
    const h=document.createElement('div');h.textContent=g;
    h.style.cssText='font-size:12px;color:#7ab0ff;margin:16px 0 2px;text-transform:uppercase;letter-spacing:.06em';
    box.appendChild(h);
    for(const sp of groups[g])box.appendChild(makeRow(sp,d,g));
  }
}
$('resetParams').onclick=async()=>{await post('/api/params/reset');loadParams()};
$('guestReset').onclick=async()=>{
  await post('/api/params/reset');loadParams();
  $('guestReset').textContent='Set back to how it was ✓ thanks!';
  setTimeout(()=>$('guestReset').textContent='Alright, broken it in enough? Set it back →',2200);
};
$('pattern').onchange=async e=>{await post('/api/pattern?id='+encodeURIComponent(e.target.value));loadParams()};
async function loadPresets(){
  const list=await (await fetch('/api/presets')).json();
  const box=$('presetList');box.innerHTML='';
  if(!list.length)box.innerHTML='<div style="font-size:12px;color:#777;margin:8px 0">No presets saved yet.</div>';
  for(const p of list){
    const row=document.createElement('div');row.className='prow';
    const lab=document.createElement('label');lab.textContent=p.name+' · '+p.pattern;lab.style.flex='1';row.appendChild(lab);
    const load=document.createElement('button');load.textContent='Load';
    load.onclick=async()=>{await post('/api/presets/load?name='+encodeURIComponent(p.name));refresh()};
    row.appendChild(load);
    if(!isGuest){
      const del=document.createElement('button');del.textContent='✕';del.title='Delete';
      del.onclick=async()=>{if(confirm('Delete "'+p.name+'"?')){await post('/api/presets/delete?name='+encodeURIComponent(p.name));loadPresets()}};
      row.appendChild(del);
    }
    box.appendChild(row);
  }
  $('presetSave').style.display=isGuest?'none':'flex';
}
$('presetSaveBtn').onclick=async()=>{
  const n=$('presetName').value.trim();if(!n)return;
  await post('/api/presets/save?name='+encodeURIComponent(n));
  $('presetName').value='';loadPresets();
};
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
  nav{display:flex;gap:14px;font-size:13px;margin-bottom:14px}
  nav a{color:#7ab0ff;text-decoration:none}
  nav b{color:#eee}
</style></head><body>
<nav><a href="/">Console</a><a href="/leds">LEDs</a><a href="/calibrate">Calibrate</a><a href="/snake">Game pad</a><b>WiFi</b></nav>
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

// Wiring-calibration wizard: lights one LED at a time (lit-pixel pattern),
// the user records where it appears, the on-chip solver narrows the 8 flip
// combos x offset to the unique wiring. Samples live in the textarea (and
// localStorage) as hand-editable "led,x,y,z" lines, mirroring the React
// app's CSV.
const char kCalibrateHtml[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>cube calibration</title>
<style>
  body{font-family:system-ui;background:#0d0d10;color:#ddd;margin:0;padding:24px;max-width:440px;margin:auto}
  h1{font-size:18px;margin:12px 0 8px}
  p{font-size:13px;color:#999;line-height:1.5;margin:6px 0}
  label{display:block;margin:12px 0 4px;font-size:13px;color:#999}
  input,textarea,select{box-sizing:border-box;background:#1a1a20;color:#eee;border:1px solid #333;border-radius:6px;padding:8px;font-size:15px}
  select{width:100%}
  textarea{width:100%;height:130px;font-family:ui-monospace,monospace;font-size:13px}
  .row{display:flex;gap:8px;align-items:end}
  .row div{flex:1}
  .row input{width:100%}
  button{padding:10px 14px;border-radius:6px;border:none;background:#2a5aa5;color:#fff;font-size:14px;cursor:pointer}
  button.sec{background:#26262e;color:#ccc}
  #result{font-size:13px;line-height:1.6;margin-top:12px;min-height:20px}
  .ok{color:#6fd66f}.warn{color:#e0b76a}.bad{color:#e07a6a}
  a{color:#7ab0ff}
  nav{display:flex;gap:14px;font-size:13px;margin-bottom:14px}
  nav a{color:#7ab0ff;text-decoration:none}
  nav b{color:#eee}
</style></head><body>
<nav><a href="/">Console</a><a href="/leds">LEDs</a><b>Calibrate</b><a href="/snake">Game pad</a><a href="/wifi">WiFi</a></nav>
<h1>Wiring calibration</h1>
<p>1&#41; <b>Start</b> switches the cube to single-pixel mode. 2&#41; Light an
LED, find it on the cube, and record its (x,y,z) — pick an origin corner
once and stick with it. 3&#41; <b>Solve</b> after 2&ndash;3 samples; apply
the layout when it&rsquo;s unique.</p>
<button id="start">Start (single-pixel mode)</button>
<div class="row" style="margin-top:14px">
  <div><label>LED index</label><input id="led" type="number" min="0" max="999" value="0"></div>
  <button id="light" class="sec">Light it</button>
</div>
<div class="row" style="margin-top:8px">
  <div><label>x</label><input id="sx" type="number" min="0" max="9"></div>
  <div><label>y</label><input id="sy" type="number" min="0" max="9"></div>
  <div><label>z</label><input id="sz" type="number" min="0" max="9"></div>
  <button id="add" class="sec">Add</button>
</div>
<label>Samples (led,x,y,z — hand-editable)</label>
<textarea id="samples" spellcheck="false"></textarea>
<div class="row" style="margin-top:10px">
  <button id="solve">Solve</button>
  <button id="apply" class="sec" disabled>Apply layout</button>
</div>
<div id="result"></div>

<hr style="border-color:#26262e;margin:24px 0">
<h1>Assembly map</h1>
<p>A <b>physical build</b> aid. <span style="color:#4de0e0">Cyan</span> marks the
two ends, <span style="color:#e0a24d">amber</span> the two middle.</p>
<p><b>Axis</b> uses the calibrated layout to light the far faces of an axis (the
actual cube faces). <b>Strand</b> ignores calibration and marks each strand&rsquo;s
ends by raw wire index (0, 9/10, 19/20&hellip;) &mdash; use it while threading.</p>
<div class="row">
  <div><label>Mode</label><select id="mode"><option value="axis">Axis (faces)</option><option value="strand">Strand (wire)</option></select></div>
  <div><label>Axis</label><select id="axis"><option>x</option><option>y</option><option>z</option></select></div>
  <div><label>Strand length</label><input id="period" type="number" min="2" max="100" value="10"></div>
</div>
<label style="display:flex;gap:8px;align-items:center;margin-top:12px">
  <input id="centers" type="checkbox" checked style="width:auto"> Mark the two middle</label>
<div style="margin-top:14px"><button id="buildStart">Show map</button></div>

<p><a href="/">&larr; back to console</a></p>
<script>
const $=id=>document.getElementById(id);
let solved=null;
$('samples').value=localStorage.getItem('cube-cal')||'';
const save=()=>localStorage.setItem('cube-cal',$('samples').value);
$('samples').addEventListener('input',save);
async function post(u,body){return fetch(u,{method:'POST',body})}
$('start').onclick=async()=>{await post('/api/pattern?id=lit-pixel');await light()};
async function light(){await post('/api/param?key=ledIdx&v='+(+$('led').value))}
$('light').onclick=light;
$('led').addEventListener('change',light);
$('add').onclick=()=>{
  const line=[+$('led').value,+$('sx').value,+$('sy').value,+$('sz').value];
  if(line.some(v=>Number.isNaN(v))){alert('fill x, y, z');return}
  $('samples').value=($('samples').value.trim()+'\n'+line.join(',')).trim();save();
  $('sx').value=$('sy').value=$('sz').value='';
};
$('solve').onclick=async()=>{
  const r=$('result');r.className='';r.textContent='Solving…';
  const res=await (await post('/api/calibrate/solve',$('samples').value)).json();
  solved=null;$('apply').disabled=true;
  if(res.candidates.length===1){
    solved=res.candidates[0];$('apply').disabled=false;
    r.className='ok';
    r.textContent=`Unique layout found: flips ${solved.fx?'X':''}${solved.fy?'Y':''}${solved.fz?'Z':''}${!(solved.fx||solved.fy||solved.fz)?'none':''}, offset ${solved.off}. Apply it!`;
  }else if(res.candidates.length>1){
    r.className='warn';
    r.textContent=`${res.candidates.length} layouts still match. Light LED ${res.suggest} next — it best tells them apart.`;
    $('led').value=res.suggest;light();
  }else if(res.bestEffort){
    r.className='bad';
    r.textContent=`No layout matches all ${res.samples} samples — likely a typo. Closest misses ${res.bestEffort.misses}; re-check your entries.`;
  }else{
    r.className='bad';r.textContent='No valid samples yet.';
  }
};
$('apply').onclick=async()=>{
  if(!solved)return;
  await post(`/api/layout?fx=${solved.fx}&fy=${solved.fy}&fz=${solved.fz}&off=${solved.off}`);
  $('result').className='ok';
  $('result').textContent='Layout applied and saved. Pick a pattern on the console to admire your correctly-mapped cube.';
};
const sp=(k,v,t)=>post('/api/param?key='+k+'&v='+v+(t?'&type='+t:''));
const setMode=()=>sp('mode',$('mode').value,'str');
const setAxis=()=>sp('axis',$('axis').value,'str');
const setPeriod=()=>sp('period',(+$('period').value||10));
const setCenters=()=>sp('showCenter',$('centers').checked?'1':'0','bool');
$('buildStart').onclick=async()=>{
  await post('/api/pattern?id=build-map');
  await setMode();await setAxis();await setPeriod();await setCenters();
};
['mode','axis','period','centers'].forEach(id=>$(id).addEventListener('change',()=>{
  ({mode:setMode,axis:setAxis,period:setPeriod,centers:setCenters})[id]();
}));
</script></body></html>)HTML";

// LED hardware page: output pins + chain split, applied live.
const char kLedsHtml[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>cube LEDs</title>
<style>
  body{font-family:system-ui;background:#0d0d10;color:#ddd;margin:0;padding:24px;max-width:420px;margin:auto}
  h1{font-size:18px;margin:12px 0 8px}
  p{font-size:13px;color:#999;line-height:1.5;margin:6px 0}
  label{display:block;margin:14px 0 4px;font-size:13px;color:#999}
  input{width:100%;box-sizing:border-box;background:#1a1a20;color:#eee;border:1px solid #333;border-radius:6px;padding:8px;font-size:15px}
  button{margin-top:18px;width:100%;padding:10px;border-radius:6px;border:none;background:#2a5aa5;color:#fff;font-size:15px;cursor:pointer}
  nav{display:flex;gap:14px;font-size:13px;margin-bottom:14px}
  nav a{color:#7ab0ff;text-decoration:none}
  nav b{color:#eee}
  #msg{font-size:13px;margin-top:10px;min-height:18px;color:#6fd66f}
</style></head><body>
<nav><a href="/">Console</a><b>LEDs</b><a href="/calibrate">Calibrate</a><a href="/snake">Game pad</a><a href="/wifi">WiFi</a></nav>
<h1>LED outputs</h1>
<p><b>Single chain:</b> set output 2 pin to -1 — output 1 drives all 1000
LEDs.<br><b>Split chain:</b> output 1 drives LEDs 0..split&minus;1, output 2
the rest. Feed the second half at its original start, same wire direction,
so calibration stays valid. This board's terminals: GPIO 16, 12, 4, 2, 13.</p>
<label>Output 1 GPIO</label><input type="number" id="lPin">
<label>Output 2 GPIO (-1 = single chain)</label><input type="number" id="lPin2">
<label>Split (LEDs on output 1)</label><input type="number" id="lSplit" min="1" max="999">
<button id="apply">Apply (live) &amp; save</button>
<div id="msg"></div>
<script>
const $=id=>document.getElementById(id);
(async()=>{
  const s=await (await fetch('/api/status')).json();
  $('lPin').value=s.ledPin;$('lPin2').value=s.ledPin2;$('lSplit').value=s.ledSplit;
})();
$('apply').onclick=async()=>{
  await fetch(`/api/ledcfg?pin=${$('lPin').value}&pin2=${$('lPin2').value}&split=${$('lSplit').value}`,{method:'POST'});
  $('msg').textContent='Applied — outputs rebuilt without a reboot.';
};
</script></body></html>)HTML";

// Admin page: everything that changes hardware, persistence, or network
// state. Split off the console (kIndexHtml) so the main page stays a clean
// guest-safe surface. Owner-only — the server 403s guests on the cube's AP.
const char kAdminHtml[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>cube-light admin</title>
<style>
  body{font-family:system-ui;background:#0d0d10;color:#ddd;margin:0;padding:24px;max-width:420px;margin:auto}
  h1{font-size:18px;margin:12px 0 8px}
  h2{font-size:14px;margin:26px 0 4px;color:#bbb;border-top:1px solid #222;padding-top:16px}
  p{font-size:13px;color:#999;line-height:1.5;margin:6px 0}
  label{display:block;margin:16px 0 6px;font-size:13px;color:#999}
  select,input[type=range],input[type=number]{width:100%;box-sizing:border-box;background:#1a1a20;color:#eee;border:1px solid #333;border-radius:6px;padding:8px;font-size:15px}
  .row{display:flex;gap:8px;align-items:center}
  .row output{min-width:48px;text-align:right;font-variant-numeric:tabular-nums}
  a{color:#7ab0ff}
  .prow{display:flex;gap:10px;align-items:center;margin:10px 0}
  .prow label{flex:0 0 46%;margin:0}
  .prow input[type=range]{flex:1}
  .prow output{min-width:44px;text-align:right;font-size:12px;color:#aaa}
  .meter{height:10px;background:#1a1a20;border-radius:5px;overflow:hidden;flex:1}
  .meter div{height:100%;background:#2a5aa5;width:0%}
  button{padding:8px 12px;border-radius:6px;border:none;background:#26262e;color:#ccc;font-size:13px;cursor:pointer}
  button.pri{background:#2a5aa5;color:#fff}
  nav{display:flex;gap:14px;font-size:13px;margin-bottom:14px}
  nav a{color:#7ab0ff;text-decoration:none}
  nav b{color:#eee}
</style></head><body>
<nav><a href="/">Console</a><a href="/leds">LEDs</a><a href="/calibrate">Calibrate</a><a href="/snake">Game pad</a><a href="/wifi">WiFi</a></nav>
<h1>Admin 🔒</h1>
<p>These change hardware, persistence, or network state. When a console
password is set (WiFi page), they ask for it (username <b>cube</b>).</p>

<label>Brightness</label>
<div class="row"><input type="range" id="bright" min="0" max="100" step="1"><output id="brightv"></output></div>
<label>Which way is up</label>
<select id="up"><option value="z+">Z+ (default)</option><option value="z-">Z&minus;</option><option value="x+">X+</option><option value="x-">X&minus;</option><option value="y+">Y+</option><option value="y-">Y&minus;</option></select>
<label>Power budget (mA, 0 = no limit)</label>
<input type="number" id="supply" min="0" step="100">
<label>Color order</label>
<select id="order"><option>RGB</option><option>GRB</option><option>BRG</option><option>RBG</option><option>GBR</option><option>BGR</option></select>

<h2>Power-on defaults</h2>
<p>Applies to the pattern currently running on the console.</p>
<div class="prow">
  <button class="pri" id="saveParams">Save current pattern as power-on defaults 🔒</button>
</div>
<div class="prow">
  <button id="factoryParams">Factory reset this pattern 🔒</button>
</div>

<h2>Microphone 🔒</h2>
<div class="prow"><label>Level</label><div class="meter"><div id="mLevel"></div></div><output id="mLevelV"></output></div>
<div class="prow"><label>Beat</label><div class="meter"><div id="mBeat"></div></div></div>
<div class="prow"><label>Raw RMS / frames</label><output id="mRaw" style="min-width:160px;text-align:left"></output></div>
<div class="prow"><label>Channel</label><select id="micCh"><option value="right">right</option><option value="left">left</option></select></div>
<div class="prow"><label>Squelch (raw RMS)</label><input type="range" id="micSqR" style="flex:1" min="0" max="500" step="5"><input type="number" id="micSq" style="flex:0 0 66px" min="0" step="5"></div>
<div class="prow"><label>Live raw RMS</label><output id="micSqLive" style="min-width:80px;text-align:left;color:#7ab0ff">–</output></div>
<p style="font-size:12px;color:#888;line-height:1.5;margin:4px 0 10px">Soft-knee gate: audio is fully silenced below squelch/2, ramps up in between, and is fully open at squelch and above. Watch the live RMS while it's quiet, then set squelch just above that idle level.</p>
<div class="prow"><button class="pri" id="micApply">Apply mic config</button></div>

<h2>Hardware &amp; network</h2>
<p><a href="/leds">LED outputs &rarr;</a> &nbsp;·&nbsp; <a href="/calibrate">Calibrate wiring &rarr;</a> &nbsp;·&nbsp; <a href="/wifi">WiFi &amp; security &rarr;</a></p>

<p style="margin-top:20px"><a href="/">&larr; back to console</a></p>
<script>
const $=id=>document.getElementById(id);
async function post(url){await fetch(url,{method:'POST'})}
async function refresh(){
  const s=await (await fetch('/api/status')).json();
  $('bright').value=Math.round(s.brightness*100);$('brightv').textContent=$('bright').value+'%';
  $('supply').value=s.supplyMA;$('order').value=s.colorOrder;$('up').value=s.up;
}
$('bright').oninput=e=>{$('brightv').textContent=e.target.value+'%'};
$('bright').onchange=e=>post('/api/brightness?v='+(e.target.value/100));
$('supply').onchange=e=>post('/api/supply?ma='+e.target.value);
$('order').onchange=e=>post('/api/order?v='+e.target.value);
$('up').onchange=e=>post('/api/up?v='+encodeURIComponent(e.target.value));
$('saveParams').onclick=async()=>{await post('/api/params/save');$('saveParams').textContent='Saved ✓';setTimeout(()=>$('saveParams').textContent='Save current pattern as power-on defaults 🔒',1500)};
$('factoryParams').onclick=async()=>{await post('/api/params/factory');$('factoryParams').textContent='Reset ✓';setTimeout(()=>$('factoryParams').textContent='Factory reset this pattern 🔒',1500)};
const applyMic=()=>post(`/api/miccfg?ch=${$('micCh').value}&squelch=${$('micSq').value}`);
$('micApply').onclick=applyMic;
$('micSqR').oninput=()=>{$('micSq').value=$('micSqR').value};
$('micSqR').onchange=applyMic;
$('micSq').oninput=()=>{$('micSqR').value=$('micSq').value};
$('micSq').onchange=applyMic;
let micInit=false;
setInterval(async()=>{
  try{
    const a=await (await fetch('/api/audio')).json();
    $('mLevel').style.width=Math.round(a.level*100)+'%';
    $('mLevelV').textContent=a.level.toFixed(2);
    $('mBeat').style.width=Math.round(a.beat*100)+'%';
    $('mRaw').textContent=`rms ${a.rms} · dc ${a.dc} · raw ${a.rawMin}..${a.rawMax} · ${a.frames} frames`;
    $('micSqLive').textContent=a.rms;
    if(!micInit){micInit=true;$('micCh').value=a.channel;$('micSq').value=$('micSqR').value=a.squelch}
  }catch(e){}
},700);
refresh();
</script></body></html>)HTML";
