#pragma once

// Minimal on-chip control pages. Self-contained (no external assets) so they
// work on the cube's own AP with no internet. The full React app remains the
// rich dev UI; this is the "standing in the dust with a phone" surface.

const char kIndexHtml[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>cube-light</title>
<style>
  body{font-family:system-ui;background:#0d0d10;color:#ddd;margin:0;padding:24px 24px 72px;max-width:420px;margin:auto}
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
  .prow{display:flex;gap:10px;align-items:center;margin:10px 0;flex-wrap:wrap}
  .prow label{flex:0 0 46%;margin:0}
  .prow input[type=range]{flex:1}
  .prow select,.prow input[type=text]{flex:1}
  .prow output{min-width:44px;text-align:right;font-size:12px;color:#aaa}
  .prow.modded>label{color:#c99bff}
  #advTogWrap{display:none;align-items:center;gap:8px;font-size:12px;color:#c99bff;margin:6px 0 2px;cursor:pointer}
  #advTogWrap input{width:auto}
  .modwrap{flex-basis:100%;display:flex;gap:6px;align-items:center;margin:2px 0 4px;padding-left:6px;border-left:2px solid #43324f}
  .modwrap select{flex:0 0 92px;font-size:12px;padding:4px}
  .modfields{display:flex;gap:4px;flex:1}
  .modcol{display:flex;flex-direction:column;flex:1;min-width:0}
  .modcap{font-size:9px;color:#8a7f96;letter-spacing:.04em;margin:0 0 1px 2px;white-space:nowrap}
  .modf{width:100%;min-width:0;box-sizing:border-box;background:#1a1a20;color:#eee;border:1px solid #333;border-radius:6px;padding:4px;font-size:12px}
  .meter{height:10px;background:#1a1a20;border-radius:5px;overflow:hidden;flex:1}
  .meter div{height:100%;background:#2a5aa5;width:0%}
  button{padding:8px 12px;border-radius:6px;border:none;background:#26262e;color:#ccc;font-size:13px;cursor:pointer}
  button.pri{background:#2a5aa5;color:#fff}
  #plToggle{position:fixed;right:14px;bottom:14px;z-index:20;background:#2a5aa5;color:#fff;padding:10px 14px;border-radius:20px;box-shadow:0 2px 10px #0008}
  #toast{position:fixed;left:14px;bottom:14px;z-index:50;background:#3a1d1d;color:#f0b0a0;border:1px solid #8a4438;border-radius:8px;padding:8px 12px;font-size:13px;max-width:70vw;opacity:0;transition:opacity .3s;pointer-events:none}
  #sidebar{position:fixed;top:0;right:0;bottom:0;width:320px;max-width:88vw;background:#141419;border-left:1px solid #2a2a33;box-shadow:-4px 0 18px #0009;z-index:30;transform:translateX(105%);transition:transform .22s ease;display:flex;flex-direction:column;padding:16px;box-sizing:border-box}
  #sidebar.open{transform:none}
  .plhead{display:flex;justify-content:space-between;align-items:center;margin-bottom:8px}
  .plhead b{font-size:15px;color:#eee}
  #plStatus{font-size:12px;color:#8fb8ff;min-height:16px;margin:2px 0 10px}
  .plctrls{display:flex;gap:6px;margin-bottom:12px}
  .plctrls button{flex:1;font-size:16px;padding:8px 0}
  #plRows{overflow-y:auto;overflow-x:hidden;flex:1}
  .plcap{display:none;gap:6px;font-size:9px;color:#8a7f96;letter-spacing:.05em;padding:0 6px;margin-bottom:3px}
  .plrow{display:flex;gap:6px;align-items:center;padding:6px;border-radius:6px;margin-bottom:3px;background:#1a1a20}
  .plrow.now{background:#1e3352;outline:1px solid #3a6bb0}
  .plname{flex:1;min-width:0;cursor:pointer}
  .plname>div:first-child{font-size:13px;color:#ddd;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
  .plpat{font-size:10px;color:#778}
  .plmeta{font-size:11px;color:#888;white-space:nowrap}
  /* Scoped with #sidebar so these beat the page-wide input[type=number] rule
     (which otherwise blows each input to 100% width and hides the row). */
  #sidebar input[type=number]{width:48px;flex:0 0 auto;padding:5px 3px;font-size:13px;text-align:center}
  #sidebar input[type=text]{flex:1;min-width:0;background:#1a1a20;color:#eee;border:1px solid #333;border-radius:6px;padding:8px;font-size:14px}
  .plbtn{padding:6px 9px;font-size:12px;background:#222}
  #params input.modf{padding:4px;font-size:12px}
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
<a id="gameLink" href="/snake" style="display:none;background:#2a8050;color:#fff;text-align:center;text-decoration:none;border-radius:8px;padding:14px;font-size:16px;margin-top:10px">🎮 Grab the game pad →</a>

<label>Mic reactivity</label>
<button id="micToggle" style="width:100%;padding:10px;font-size:15px"></button>

<details open id="paramsBox"><summary>Pattern parameters</summary>
<label id="advTogWrap"><input type="checkbox" id="advTog"> ⚙ Advanced — auto-modulate params (∿)</label>
<div id="params"></div>
<div class="prow">
  <button id="resetParams">Reset to defaults</button>
</div>
</details>

<button class="pri" id="guestReset" style="display:none;width:100%;padding:12px;margin-top:18px">Alright, broken it in enough? Set it back →</button>

<button id="plToggle">☰ Presets</button>
<div id="sidebar">
  <div class="plhead"><b>Presets</b><button id="plClose">✕</button></div>
  <div id="plStatus">Cycle paused</div>
  <div class="plctrls">
    <button id="plPrev" title="Previous">⏮</button>
    <button id="plPlay" title="Play/pause cycle">▶</button>
    <button id="plNext" title="Next">⏭</button>
    <button id="plShuffle" title="Shuffle (priority-weighted)">🔀</button>
  </div>
  <div class="plcap" id="plCap"><span style="flex:1">preset — tap to show</span><span style="width:48px;text-align:center">dwell s</span><span style="width:48px;text-align:center">★ 0–5</span><span style="width:52px"></span></div>
  <div id="plRows"></div>
  <div class="prow" id="presetSave" style="margin:10px 0 0">
    <input type="text" id="presetName" placeholder="Save current look as…">
    <button id="presetSaveBtn" class="pri">Save</button>
  </div>
  <div class="prow" id="presetIO" style="display:none;margin:4px 0 0">
    <button id="presetExport" title="Download all presets as JSON">⬇ Export</button>
    <label class="pri" style="cursor:pointer;padding:8px 12px;border-radius:6px;font-size:13px" title="Import presets from a JSON file (overwrites matching names)">⬆ Import<input type="file" id="presetImport" accept="application/json,.json" style="display:none"></label>
  </div>
</div>

<div class="stat" id="stat"></div>
<div id="toast"></div>
<script>
const $=id=>document.getElementById(id);
let toastT=null;
function toast(m){const t=$('toast');t.textContent=m;t.style.opacity=1;clearTimeout(toastT);toastT=setTimeout(()=>t.style.opacity=0,2000)}
async function post(url){
  try{
    const r=await fetch(url,{method:'POST'});
    if(!r.ok){let t='';try{t=(await r.text()).trim()}catch(_){}toast('⚠ '+(t||"cube didn't accept that"))}
  }catch(e){toast("⚠ cube didn't accept that")}
}
let lastPat=null;
// One status fetch feeds everything: cheap UI sync every tick. Leaves the
// pattern select alone while focused; reloads params only when the pattern
// changed under us (playlist advance) and nothing in #params has focus.
function applyStatus(s){
  const sel=$('pattern');
  if(document.activeElement!==sel)sel.value=s.pattern;
  gameLink(s.pattern);
  micOn=s.micOn; drawMic();
  isGuest=!!s.guest;
  $('advTogWrap').style.display=isGuest?'none':'flex';  // modulation is owner-only
  $('liveBanner').style.display=s.live?'block':'none';
  // Guests (on the cube's own hotspot) can tinker but not clobber saved
  // settings — hide the admin door, offer a friendly "set it back" button.
  if(s.guest){
    $('guestBanner').style.display='block';
    $('guestReset').style.display='block';
    $('adminLink').style.display='none';
  }
  $('stat').textContent=`ip ${s.ip} · rssi ${s.rssi}dBm · ${s.fps}fps target · v${s.version}`;
  plState=s.playlist||{};
  drawPlaylist();
  if(s.pattern!==lastPat&&!$('params').contains(document.activeElement)){lastPat=s.pattern;loadParams()}
}
async function poll(){
  let s;try{s=await (await fetch('/api/status')).json()}catch(e){return}
  applyStatus(s);
}
async function refresh(){
  // Initial/full populate. Retry until the cube answers — one failed fetch
  // must not brick the page (empty select, blank mic button).
  let s;try{s=await (await fetch('/api/status')).json()}catch(e){setTimeout(refresh,2000);return}
  const sel=$('pattern');
  if(sel.options.length===0){
    // Split the dropdown: display patterns vs calibration/diagnostic tools.
    const UTIL=new Set(['snake-cal','index-walk','lit-pixel','build-map']);
    const gLight=document.createElement('optgroup');gLight.label='Light patterns';
    const gTool=document.createElement('optgroup');gTool.label='Calibration & tools';
    for(const p of s.patterns){const o=document.createElement('option');o.value=o.textContent=p;(UTIL.has(p)?gTool:gLight).appendChild(o)}
    sel.appendChild(gLight);sel.appendChild(gTool);
  }
  lastPat=s.pattern;
  applyStatus(s);
  loadParams();
  loadPresets();
}
let micOn=true;
let isGuest=false;
let advanced=localStorage.getItem('cube-adv')==='1';
$('advTog').checked=advanced;
$('advTog').onchange=()=>{advanced=$('advTog').checked;localStorage.setItem('cube-adv',advanced?'1':'0');loadParams()};
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
  if(k==='gray'||k==='grayFlicker')return 'Pattern';  // tv-static background style
  if(/Gain|[Bb]eat|[Ll]evel|[Aa]udio|mic|attack|release|gamma|throb|speedFrom|bpm/.test(k))return 'Audio';
  return 'Pattern';
}
function makeRow(sp,d,group){
  const row=document.createElement('div');row.className='prow';
  // Audio-group params drive another param — audio can push it past the max.
  if(group==='Audio')row.title='Audio can push this parameter beyond the slider maximum (up to ~2x by default).';
  const modActive=sp.mod&&sp.mod.mode&&sp.mod.mode!=='off';
  const lab=document.createElement('label');
  lab.textContent=(modActive?'∿ ':'')+sp.label;
  if(sp.desc){lab.title=sp.desc;lab.style.cursor='help';lab.style.textDecoration='underline dotted #444'}
  if(modActive)row.classList.add('modded');
  row.appendChild(lab);
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
    let last=0;  // live-stream drags, throttled to ~150ms; onchange is the final send
    ctl.oninput=()=>{out.textContent=(+ctl.value).toFixed(2).replace(/\.?0+$/,'');
      const now=Date.now();if(now-last>=150){last=now;post(`/api/param?type=num&key=${sp.key}&v=${ctl.value}`)}};
    ctl.onchange=()=>post(`/api/param?type=num&key=${sp.key}&v=${ctl.value}`);
  }
  row.appendChild(ctl);if(out)row.appendChild(out);
  // Advanced (owner-only): numeric params get an auto-modulation control.
  if(advanced&&!isGuest&&sp.type===T.NUM)row.appendChild(makeMod(sp));
  return row;
}
// Compact per-param modulation control: mode selector + min/max/rate/step.
// Empty numeric fields let the server fill spec-based defaults.
function makeMod(sp){
  const wrap=document.createElement('div');wrap.className='modwrap';
  const sel=document.createElement('select');
  [['off','off'],['pingpong','ping-pong'],['walk','walk']].forEach(([v,t])=>{
    const o=document.createElement('option');o.value=v;o.textContent=t;sel.appendChild(o)});
  sel.value=sp.mod?sp.mod.mode:'off';
  const fields=document.createElement('div');fields.className='modfields';
  // Each input gets a tiny caption so the numbers are identifiable at a
  // glance, plus a step attr matching the param spec so spinners move in
  // sensible increments (integer-only params like plane count step whole).
  const inp=(k,cap,val,st)=>{const c=document.createElement('div');c.className='modcol';
    const l=document.createElement('div');l.className='modcap';l.textContent=cap;
    const i=document.createElement('input');i.type='number';i.className='modf';i.dataset.k=k;i.title=k;
    i.step=st||'any';if(val!==undefined&&val!=='')i.value=val;
    c.append(l,i);c.inp=i;c.cap=l;return c};
  const m=sp.mod||{};
  const cMin=inp('min','min',m.min,sp.step),cMax=inp('max','max',m.max,sp.step),
        cRate=inp('rate','rate',m.rate),cStep=inp('step','± step',m.step,sp.step);
  fields.append(cMin,cMax,cRate,cStep);
  const cols=[cMin,cMax,cRate,cStep];
  // step is unused by ping-pong; rate means units/s (pingpong) vs steps/s (walk).
  const shape=()=>{const md=sel.value;
    cStep.style.display=md==='walk'?'flex':'none';
    cRate.cap.textContent=md==='walk'?'steps/s':'units/s'};
  const send=async(refresh)=>{
    const mode=sel.value;
    if(mode==='off'){await post('/api/param/mod?key='+sp.key+'&mode=off');fields.style.display='none';if(refresh)loadParams();return}
    fields.style.display='flex';shape();
    let q='/api/param/mod?key='+sp.key+'&mode='+mode;
    cols.forEach(c=>{if(c.inp.value!=='')q+='&'+c.inp.dataset.k+'='+c.inp.value});
    await post(q);if(refresh)loadParams();
  };
  sel.onchange=()=>send(true);           // refresh to reflect server defaults / badge
  cols.forEach(c=>c.inp.onchange=()=>send(false));
  fields.style.display=sel.value==='off'?'none':'flex';shape();
  wrap.append(sel,fields);
  return wrap;
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
// A game is on: surface a big link to the controller page.
function gameLink(pat){$('gameLink').style.display=(pat==='snake-3d'||pat==='pacman-3d')?'block':'none'}
$('resetParams').onclick=async()=>{await post('/api/params/reset');loadParams()};
$('guestReset').onclick=async()=>{
  await post('/api/params/reset');loadParams();
  $('guestReset').textContent='Set back to how it was ✓ thanks!';
  setTimeout(()=>$('guestReset').textContent='Alright, broken it in enough? Set it back →',2200);
};
$('pattern').onchange=async e=>{gameLink(e.target.value);await post('/api/pattern?id='+encodeURIComponent(e.target.value));loadParams()};
// One list, one place: the sidebar is the preset manager. Rows: tap name to
// show it; dwell + priority edit inline; ✎ rename, ✕ delete (owners only).
const esc=s=>String(s).replace(/[&<>"']/g,c=>'&#'+c.charCodeAt(0)+';');
let presetNames=[];
let anyAmbient=false;  // drives the "ambient only" playlist hint when mic is off
function presetRow(p){
  const row=document.createElement('div');row.className='plrow';row.dataset.name=p.name;
  const nm=document.createElement('div');nm.className='plname';nm.title='Show this preset';
  nm.innerHTML='<div>'+esc(p.name)+'</div><div class="plpat"><span class="plreact">'+(p.reactive?'🎵':'🌙')+'</span> '+esc(p.pattern)+'</div>';
  nm.onclick=async()=>{await post('/api/presets/load?name='+encodeURIComponent(p.name));refresh()};
  // Owners can reclassify: the 🎵/🌙 marker is a toggle.
  const rx=nm.querySelector('.plreact');
  rx.title=p.reactive?'Music-reactive (tap to mark ambient)':'Ambient (tap to mark music-reactive)';
  if(!isGuest){rx.style.cursor='pointer';
    rx.onclick=async e=>{e.stopPropagation();await post('/api/presets/meta?name='+encodeURIComponent(p.name)+'&reactive='+(p.reactive?0:1));loadPresets()}}
  row.appendChild(nm);
  if(isGuest){
    const m=document.createElement('span');m.className='plmeta';m.textContent=(+p.dwellSec)+'s · ★'+p.priority;row.appendChild(m);
  }else{
    const dw=document.createElement('input');dw.type='number';dw.min=1;dw.value=p.dwellSec;dw.title='Seconds this preset shows in the cycle';
    dw.onchange=()=>{const v=Math.max(1,Math.round(+dw.value)||1);dw.value=v;post('/api/presets/meta?name='+encodeURIComponent(p.name)+'&dwellSec='+v)};
    const pr=document.createElement('input');pr.type='number';pr.min=0;pr.max=5;pr.value=p.priority;pr.title='Shuffle priority 0-5 (5 = most often, 0 = never auto-plays)';
    pr.onchange=()=>{const v=Math.min(5,Math.max(0,Math.round(+pr.value)||0));pr.value=v;post('/api/presets/meta?name='+encodeURIComponent(p.name)+'&priority='+v)};
    const ren=document.createElement('button');ren.className='plbtn';ren.textContent='✎';ren.title='Rename';
    ren.onclick=async()=>{const nn=prompt('Rename "'+p.name+'" to:',p.name);if(!nn||!nn.trim()||nn.trim()===p.name)return;
      if(presetNames.includes(nn.trim())){alert('A preset with that name already exists');return}
      await post('/api/presets/rename?from='+encodeURIComponent(p.name)+'&to='+encodeURIComponent(nn.trim()));loadPresets()};
    const del=document.createElement('button');del.className='plbtn';del.textContent='✕';del.title='Delete';
    del.onclick=async()=>{if(confirm('Delete "'+p.name+'"?')){await post('/api/presets/delete?name='+encodeURIComponent(p.name));loadPresets()}};
    row.append(dw,pr,ren,del);
  }
  return row;
}
async function loadPresets(){
  const list=await (await fetch('/api/presets')).json();
  presetNames=list.map(p=>p.name);
  anyAmbient=list.some(p=>!p.reactive);
  const box=$('plRows');box.innerHTML='';
  $('plCap').style.display=list.length&&!isGuest?'flex':'none';
  if(!list.length)box.innerHTML='<div style="font-size:12px;color:#777;line-height:1.5">No presets yet — dial in a pattern you like, then save it below.</div>';
  // Two shelves: music-reactive vs ambient. Mic off = the auto-cycle plays
  // only the ambient shelf.
  for(const [title,items] of [['🎵 Music-reactive',list.filter(p=>p.reactive)],['🌙 Ambient',list.filter(p=>!p.reactive)]]){
    if(!items.length)continue;
    const h=document.createElement('div');h.textContent=title;
    h.style.cssText='font-size:11px;color:#8a8fa0;margin:8px 0 4px;letter-spacing:.05em;text-transform:uppercase';
    box.appendChild(h);
    for(const p of items)box.appendChild(presetRow(p));
  }
  $('presetSave').style.display=isGuest?'none':'flex';
  $('presetIO').style.display=isGuest?'none':'flex';
  markNow();
}
// Save flow: Enter saves; the button flips to "Overwrite" when the name
// already exists (import-style replace semantics).
$('presetName').oninput=()=>{$('presetSaveBtn').textContent=presetNames.includes($('presetName').value.trim())?'Overwrite':'Save'};
$('presetName').onkeydown=e=>{if(e.key==='Enter')$('presetSaveBtn').click()};
$('presetSaveBtn').onclick=async()=>{
  const n=$('presetName').value.trim();if(!n)return;
  await post('/api/presets/save?name='+encodeURIComponent(n));
  $('presetName').value='';$('presetSaveBtn').textContent='Saved ✓';
  setTimeout(()=>{$('presetSaveBtn').textContent='Save'},1200);
  loadPresets();
};
$('presetExport').onclick=()=>{location.href='/api/presets/export'};
$('presetImport').onchange=async(e)=>{
  const f=e.target.files[0];if(!f)return;
  const text=await f.text();e.target.value='';
  const r=await fetch('/api/presets/import',{method:'POST',body:text});
  let s=null;try{s=await r.json()}catch(_){}
  if(s)alert('Imported '+s.imported+', overwritten '+s.overwritten+', skipped '+s.skipped);
  else alert('Import failed');
  loadPresets();
};
// ---- playlist state ----
let plState={};
function markNow(){
  document.querySelectorAll('.plrow').forEach(r=>{
    r.classList.toggle('now',plState.enabled&&r.dataset.name===plState.current);
  });
}
function drawPlaylist(){
  // Guests may pause but not skip, shuffle, or start the cycle.
  ['plPrev','plNext','plShuffle'].forEach(id=>{$(id).disabled=isGuest;$(id).style.opacity=isGuest?.4:1});
  const noStart=isGuest&&!plState.enabled;  // ▶ would start the cycle — owner-only
  $('plPlay').disabled=noStart;$('plPlay').style.opacity=noStart?.4:1;
  $('plPlay').title=noStart?'Owners start the cycle; guests may pause':'Play/pause cycle';
  $('plPlay').textContent=plState.enabled?'⏸':'▶';
  $('plShuffle').style.background=plState.shuffle?'#2a5aa5':'#26262e';
  $('plStatus').textContent=plState.enabled
    ?('Now: '+(plState.current||'—')+' · '+Math.max(0,Math.round(plState.dwellRemainingSec))+'s left'
      +(!micOn&&anyAmbient?' · 🌙 ambient only':''))
    :'Cycle paused';
  markNow();
}
$('plToggle').onclick=()=>{$('sidebar').classList.toggle('open');loadPresets();poll()};
$('plClose').onclick=()=>$('sidebar').classList.remove('open');
addEventListener('keydown',e=>{if(e.key==='Escape')$('sidebar').classList.remove('open')});
$('plPlay').onclick=async()=>{
  if(!plState.enabled&&isGuest)return;  // guests may pause but not start
  if(!plState.enabled&&!presetNames.length){toast('No presets to cycle yet');return}
  await post('/api/playlist?enabled='+(plState.enabled?0:1));poll();
};
$('plShuffle').onclick=async()=>{if(isGuest)return;await post('/api/playlist?shuffle='+(plState.shuffle?0:1));poll()};
$('plNext').onclick=async()=>{if(isGuest)return;await post('/api/playlist/next');refresh()};
$('plPrev').onclick=async()=>{if(isGuest)return;await post('/api/playlist/prev');refresh()};
setInterval(poll,1500);
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
%APWARN%
<label>AP / OTA password (min 8 chars)</label>
<div class="pw"><input type="password" name="appass" id="appass" placeholder="(unchanged)"><button type="button" data-for="appass">show</button></div>

<h2>Settings console</h2>
<p>Optional password for these pages (username <b>cube</b>) — keeps others
on the same network from changing settings.</p>
<label>Console password</label>
<div class="pw"><input type="password" name="uipass" id="uipass" placeholder="%UIPASS%"><button type="button" data-for="uipass">show</button></div>
<label class="chk"><input type="checkbox" name="clearui" value="1">Remove console password</label>

<h2>Home Assistant (MQTT)</h2>
<p>Point the cube at your MQTT broker (the Mosquitto add-on, usually) and it
announces itself to Home Assistant automatically: a light with on/off +
brightness, pattern &amp; preset pickers, and a text box that drives the
text pattern. See <b>docs/home-assistant.md</b> in the repo for automations
(song titles, album art).</p>
<label class="chk"><input type="checkbox" name="mqen" value="1" %MQEN%>Enable MQTT</label>
<label>Broker host / IP</label><input type="text" name="mqhost" value="%MQHOST%" placeholder="homeassistant.local">
<label>Broker port</label><input type="text" name="mqport" value="%MQPORT%">
<label>MQTT username (optional)</label><input type="text" name="mquser" value="%MQUSER%">
<label>MQTT password</label>
<div class="pw"><input type="password" name="mqpass" id="mqpass" placeholder="(unchanged)"><button type="button" data-for="mqpass">show</button></div>

<button class="act">Save &amp; reboot</button>
</form>
<p><a href="/">&larr; back</a></p>
<script>
const $=id=>document.getElementById(id);
document.querySelectorAll('.pw button').forEach(b=>{
  b.onclick=()=>{const i=$(b.dataset.for);const show=i.type==='password';i.type=show?'text':'password';b.textContent=show?'hide':'show'};
});
const esc=s=>String(s).replace(/[&<>"']/g,c=>'&#'+c.charCodeAt(0)+';');
$('scan').onclick=async()=>{
  $('scan').textContent='Scanning…';$('scan').disabled=true;
  try{
    const nets=await (await fetch('/api/scan')).json();
    const ul=$('nets');ul.innerHTML='';
    for(const n of nets){
      const li=document.createElement('li');
      li.innerHTML=`${esc(n.ssid)}<span>${n.rssi}dBm${n.open?' · open':''}</span>`;
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
  #calBtn{margin-top:18px;background:#181820;border:1px solid #34343f;color:#9ab;
       border-radius:10px;padding:9px 14px;font-size:13px;cursor:pointer}
  /* calibration overlay */
  #cal{position:fixed;inset:0;background:#0b0b0ef2;display:none;flex-direction:column;
       align-items:center;justify-content:center;z-index:40;padding:20px;text-align:center}
  #cal.on{display:flex}
  #cal h2{font-size:16px;color:#ddd;margin:0 0 6px;font-weight:600}
  #cal p{font-size:13px;color:#9a9aa8;margin:0 0 20px;max-width:300px;line-height:1.5}
  #cal .prog{font-size:12px;color:#666;margin-bottom:14px}
  #cal .pad button:active{background:#3a7a3a}
  #cal .row{display:flex;gap:12px;margin-top:22px}
  #cal .row button{background:#1c1c24;border:1px solid #34343f;border-radius:10px;
       color:#bbb;font-size:13px;padding:9px 16px;cursor:pointer}
  nav{display:flex;gap:14px;font-size:13px;margin-bottom:14px;align-self:flex-start}
  nav a{color:#7ab0ff;text-decoration:none}
  .games{display:flex;gap:10px;width:284px;margin-bottom:16px}
  .games button{flex:1;height:52px;font-size:16px;background:#1c1c24;border:1px solid #34343f;
       border-radius:12px;color:#e8e8f0;cursor:pointer;touch-action:manipulation}
  .games button.on{background:#2a5aa5;border-color:#3a6bb0;color:#fff}
</style></head><body>
<nav><a href="/">&larr; Console</a></nav>
<h1>cube-light · game pad</h1>
<div class="games"><button data-g="snake-3d">🐍 Snake</button><button data-g="pacman-3d">👾 Pac-Man</button></div>
<div class="pad">
  <span class="blank"></span><button data-b="up">▲</button><span class="blank"></span>
  <button data-b="left">◀</button><button data-b="down">▼</button><button data-b="right">▶</button>
</div>
<div class="zrow"><button data-d="4">Z ▲ up</button><button data-d="5">Z ▼ down</button></div>
<div id="st"></div>
<button id="calBtn">⤢ Calibrate directions</button>

<div id="cal">
  <div class="prog" id="calProg"></div>
  <h2>Which way is the arrow pointing?</h2>
  <p>The cube shows a bright arrow aimed at a glowing edge. Tap the button that points that way from where you stand.</p>
  <div class="pad">
    <span class="blank"></span><button data-cb="up">▲</button><span class="blank"></span>
    <button data-cb="left">◀</button><span class="blank"></span><button data-cb="right">▶</button>
    <span class="blank"></span><button data-cb="down">▼</button><span class="blank"></span>
  </div>
  <div class="row"><button id="calCancel">Cancel</button></div>
</div>
<script>
const st=document.getElementById('st');
// Quick start: jump straight into a game (pattern switching is guest-open).
// The lit button tracks whatever the cube is actually running.
const gameBtns=document.querySelectorAll('.games button');
function markGame(id){gameBtns.forEach(b=>b.classList.toggle('on',b.dataset.g===id))}
gameBtns.forEach(b=>b.addEventListener('click',async()=>{
  if(navigator.vibrate)navigator.vibrate(8);
  try{await fetch('/api/pattern?id='+b.dataset.g,{method:'POST'});markGame(b.dataset.g);st.textContent=''}
  catch(e){st.textContent='connection lost — retry'}
}));
async function syncGame(){try{const s=await (await fetch('/api/status')).json();markGame(s.pattern)}catch(e){}}
syncGame();setInterval(syncGame,3000);
// Live play: horizontal buttons are player-relative (btn=), z buttons direct (dir=).
document.querySelectorAll('.pad button[data-b],.zrow button[data-d]').forEach(b=>{
  b.addEventListener('pointerdown',async e=>{
    e.preventDefault();
    if(navigator.vibrate)navigator.vibrate(8);
    const q=b.dataset.b?('btn='+b.dataset.b):('dir='+b.dataset.d);
    try{
      const r=await fetch('/api/game?'+q,{method:'POST'});
      const t=await r.text();
      st.textContent=t==='ok'?'':t;
    }catch(err){st.textContent='connection lost — retry'}
  });
});
// Direction calibration wizard.
const cal=document.getElementById('cal'),calProg=document.getElementById('calProg');
async function calRefresh(){
  try{const r=await fetch('/api/snakecal/state');const s=await r.json();
    if(!s.active){cal.classList.remove('on');return;}
    calProg.textContent='Step '+(s.index+1)+' of '+s.total;
  }catch(e){}
}
document.getElementById('calBtn').addEventListener('click',async()=>{
  await fetch('/api/snakecal/start',{method:'POST'});
  cal.classList.add('on');calRefresh();
});
document.getElementById('calCancel').addEventListener('click',async()=>{
  await fetch('/api/snakecal/cancel',{method:'POST'});
  cal.classList.remove('on');st.textContent='calibration canceled';
});
cal.querySelectorAll('button[data-cb]').forEach(b=>{
  b.addEventListener('click',async()=>{
    if(navigator.vibrate)navigator.vibrate(8);
    try{
      const r=await fetch('/api/snakecal/map?button='+b.dataset.cb,{method:'POST'});
      const j=await r.json();
      if(j.done){cal.classList.remove('on');st.textContent='directions saved ✓';}
      else calRefresh();
    }catch(e){st.textContent='connection lost — retry'}
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

<h2>Firmware updates (OTA) 🔒</h2>
<p>Wireless reflashing stays <b>locked</b> until you arm a 15-minute window
here — so nobody at the party can push their own firmware, even if they know
the hotspot password. It re-locks on its own, and any reboot locks it too.
(USB flashing can't be blocked in software — keep the enclosure locked.)</p>
<div class="prow"><button class="pri" id="otaBtn">…</button></div>
<div id="otaMsg" style="font-size:13px;min-height:18px;color:#e0b76a"></div>

<h2>Hardware &amp; network</h2>
<p><a href="/leds">LED outputs &rarr;</a> &nbsp;·&nbsp; <a href="/calibrate">Calibrate wiring &rarr;</a> &nbsp;·&nbsp; <a href="/wifi">WiFi &amp; security &rarr;</a></p>

<p style="margin-top:20px"><a href="/">&larr; back to console</a></p>
<script>
const $=id=>document.getElementById(id);
async function post(url){await fetch(url,{method:'POST'})}
let otaArmed=false;
async function refresh(){
  const s=await (await fetch('/api/status')).json();
  $('bright').value=Math.round(s.brightness*100);$('brightv').textContent=$('bright').value+'%';
  $('supply').value=s.supplyMA;$('order').value=s.colorOrder;$('up').value=s.up;
  otaArmed=!!s.otaArmed;
  $('otaBtn').textContent=otaArmed
    ?('🔓 Armed — lock now (auto-locks in '+Math.max(1,Math.round(s.otaRemainingSec/60))+' min)')
    :'Arm wireless reflashing for 15 min';
  $('otaBtn').style.background=otaArmed?'#8a4438':'#2a5aa5';
}
$('otaBtn').onclick=async()=>{
  const r=await fetch('/api/ota?on='+(otaArmed?0:1),{method:'POST'});
  $('otaMsg').textContent=r.ok?'':await r.text();
  refresh();
};
setInterval(refresh,5000);
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
