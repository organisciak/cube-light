// cube-light browser simulator. The pattern engine is the firmware's own C++
// (firmware/lib/core) compiled to WebAssembly (engine.js/engine.wasm, built by
// ../build.sh); this file only draws the 1000 RGB values it produces.
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { ShaderPass } from 'three/addons/postprocessing/ShaderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
import createCubeEngine from './engine.js';
import { PhosphorPass, CRTShader } from './crt.js';
import { MicAnalyzer, fakeAudio, BANDS } from './audio.js';

const $ = id => document.getElementById(id);
const store = {
  get(k, d) { try { const v = localStorage.getItem(k); return v == null ? d : JSON.parse(v); } catch { return d; } },
  set(k, v) { try { localStorage.setItem(k, JSON.stringify(v)); } catch {} },
  del(k) { try { localStorage.removeItem(k); } catch {} },
};

// ---------------------------------------------------------------- engine ----
const M = await createCubeEngine();
const E = {
  n: M.ccall('cube_n', 'number'),
  count: M.ccall('cube_pattern_count', 'number'),
  id: M.cwrap('cube_pattern_id', 'string', ['number']),
  select: M.cwrap('cube_select', 'number', ['string']),
  clear: M.cwrap('cube_clear_params', null, []),
  setNum: M.cwrap('cube_set_num', null, ['string', 'number']),
  setBool: M.cwrap('cube_set_bool', null, ['string', 'number']),
  setStr: M.cwrap('cube_set_str', null, ['string', 'string']),
  setUp: M.cwrap('cube_set_up', null, ['string']),
  setAudio: M.cwrap('cube_set_audio', null, ['number', 'number', 'number', 'number']),
  beatFeed: M.cwrap('cube_beat_feed', 'number', ['number', 'number']),
  beatDecay: M.cwrap('cube_beat_decay', null, ['number']),
  beatEnv: M.cwrap('cube_beat_envelope', 'number', []),
  beatBpm: M.cwrap('cube_beat_bpm', 'number', ['number']),
  render: M.cwrap('cube_render', 'number', ['number']),
  index: M.cwrap('cube_index', 'number', ['number', 'number', 'number']),
  snake: M.cwrap('cube_snake_input', null, ['number']),
  pacman: M.cwrap('cube_pacman_input', null, ['number']),
  specs: JSON.parse(M.ccall('cube_specs_json', 'string')),
  palettes: JSON.parse(M.ccall('cube_palettes_json', 'string')),
};
const N = E.n, LEDS = N * N * N;
const bandsPtr = M._malloc(BANDS * 4);
const bandsHeap = new Float32Array(M.HEAPF32.buffer, bandsPtr, BANDS);
const patternIds = Array.from({ length: E.count }, (_, i) => E.id(i));

// ----------------------------------------------------------------- scene ----
const canvas = $('view');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: false, powerPreference: 'high-performance', preserveDrawingBuffer: true });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.toneMapping = THREE.NoToneMapping;
const scene = new THREE.Scene();
scene.background = new THREE.Color(0x000000);
const camera = new THREE.PerspectiveCamera(32, 1, 0.1, 400);
camera.position.set(22, 13, 28);
const controls = new OrbitControls(camera, canvas);
controls.enableDamping = true; controls.dampingFactor = 0.08;
controls.autoRotate = true; controls.autoRotateSpeed = 0.6;
controls.target.set(0, -0.3, 0);
controls.minDistance = 10; controls.maxDistance = 90;
const BASE_ORBIT = 0.6;

// Voxels: one instanced billboard quad per LED with a soft-disc shader.
// aColor = the LED's own light; aSpill = light borrowed from lit neighbours,
// so an off bead next to a bright one picks up a tint (the "refraction"
// of a real seed pixel's lens catching its neighbour).
const half = (N - 1) / 2;
const quad = new THREE.PlaneGeometry(1, 1);
const inst = new THREE.InstancedBufferGeometry().copy(quad);
inst.instanceCount = LEDS;
const offsets = new Float32Array(LEDS * 3);
const colors = new Float32Array(LEDS * 3);
const spill = new Float32Array(LEDS * 3);
const ledPos = new Int16Array(LEDS * 3);          // LED index -> (x,y,z)
const xyzToLed = new Int32Array(LEDS);            // (x,y,z) -> LED index
for (let z = 0; z < N; z++) for (let y = 0; y < N; y++) for (let x = 0; x < N; x++) {
  const i = E.index(x, y, z);
  ledPos[i * 3] = x; ledPos[i * 3 + 1] = y; ledPos[i * 3 + 2] = z;
  xyzToLed[(z * N + y) * N + x] = i;
}
// Six-neighbour lists for the spill pass (-1 = edge).
const nbr = new Int32Array(LEDS * 6).fill(-1);
for (let i = 0; i < LEDS; i++) {
  const x = ledPos[i * 3], y = ledPos[i * 3 + 1], z = ledPos[i * 3 + 2];
  const at = (xx, yy, zz) => (xx < 0 || yy < 0 || zz < 0 || xx >= N || yy >= N || zz >= N) ? -1 : xyzToLed[(zz * N + yy) * N + xx];
  nbr.set([at(x - 1, y, z), at(x + 1, y, z), at(x, y - 1, z), at(x, y + 1, z), at(x, y, z - 1), at(x, y, z + 1)], i * 6);
}
for (let i = 0; i < LEDS; i++) {
  // Design z is "up": map to world Y. World X = x, world Z = -y.
  offsets[i * 3] = ledPos[i * 3] - half;
  offsets[i * 3 + 1] = ledPos[i * 3 + 2] - half;
  offsets[i * 3 + 2] = -(ledPos[i * 3 + 1] - half);
}
inst.setAttribute('aOffset', new THREE.InstancedBufferAttribute(offsets, 3));
inst.setAttribute('aColor', new THREE.InstancedBufferAttribute(colors, 3));
inst.setAttribute('aSpill', new THREE.InstancedBufferAttribute(spill, 3));
const voxelMat = new THREE.ShaderMaterial({
  uniforms: { uSize: { value: 0.3 }, uGain: { value: 0.75 }, uFloor: { value: 0.035 }, uSpill: { value: 0.18 } },
  vertexShader: `
    attribute vec3 aOffset; attribute vec3 aColor; attribute vec3 aSpill;
    uniform float uSize; varying vec3 vColor; varying vec3 vSpill; varying vec2 vUv;
    void main(){
      vColor = aColor; vSpill = aSpill; vUv = uv;
      vec4 c = modelViewMatrix * vec4(aOffset, 1.0);
      float lum = max(max(aColor.r, aColor.g), aColor.b);
      float s = uSize * (0.75 + 0.9 * lum);   // bright LEDs bloom bigger
      c.xy += position.xy * s;
      gl_Position = projectionMatrix * c;
    }`,
  fragmentShader: `
    uniform float uGain, uFloor, uSpill; varying vec3 vColor; varying vec3 vSpill; varying vec2 vUv;
    void main(){
      vec2 d = vUv - 0.5; float r = length(d) * 2.0;
      float core = smoothstep(0.55, 0.0, r);
      float halo = smoothstep(1.0, 0.2, r) * 0.55;
      vec3 lit = vColor * uGain;
      float lum = max(max(lit.r, lit.g), lit.b);
      // The bead itself: a grey floor plus whatever its neighbours throw at it.
      vec3 bead = (vec3(uFloor) + vSpill * uSpill) * core * (1.0 - lum);
      vec3 col = lit * (core * 1.6 + halo) + bead;
      col += vec3(core * core) * lum * 0.35;   // overdriven core whitens
      gl_FragColor = vec4(col, 1.0);
    }`,
  transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, depthTest: true,
});
const voxels = new THREE.Mesh(inst, voxelMat);
voxels.frustumCulled = false;
scene.add(voxels);

// The strings: 100 faint verticals, the lattice the clips hold.
const stringPts = [];
for (let x = 0; x < N; x++) for (let y = 0; y < N; y++) {
  stringPts.push(x - half, -half - 0.6, -(y - half), x - half, half + 0.4, -(y - half));
}
const strings = new THREE.LineSegments(
  new THREE.BufferGeometry().setAttribute('position', new THREE.Float32BufferAttribute(stringPts, 3)),
  new THREE.LineBasicMaterial({ color: 0x2a2a30, transparent: true, opacity: 0.55 }));
scene.add(strings);

// The room: an inside-out sphere and a floor that take on the cube's average
// colour, scaled by how much of the cube is lit. One pixel barely registers;
// every LED at full white washes the whole tube.
const roomMat = new THREE.ShaderMaterial({
  uniforms: { uAmb: { value: new THREE.Color(0, 0, 0) }, uRoom: { value: 0.16 } },
  vertexShader: `varying vec3 vPos; void main(){ vPos = position; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`,
  fragmentShader: `
    uniform vec3 uAmb; uniform float uRoom; varying vec3 vPos;
    void main(){
      // Brightest where the cube is (the origin), fading toward the far walls.
      float dist = length(vPos) / 120.0;
      float fall = 1.0 / (1.0 + dist * dist * 2.0);
      // The floor (below the cube) catches more than the ceiling.
      float floorBias = 0.7 + 0.6 * smoothstep(20.0, -30.0, vPos.y);
      gl_FragColor = vec4(uAmb * uRoom * fall * floorBias, 1.0);
    }`,
  side: THREE.BackSide, depthWrite: false,
});
const room = new THREE.Mesh(new THREE.SphereGeometry(120, 24, 16), roomMat);
scene.add(room);
const floorMat = new THREE.ShaderMaterial({
  uniforms: { uAmb: roomMat.uniforms.uAmb, uRoom: roomMat.uniforms.uRoom },
  vertexShader: `varying vec2 vUv; void main(){ vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`,
  fragmentShader: `
    uniform vec3 uAmb; uniform float uRoom; varying vec2 vUv;
    void main(){
      float r = length(vUv - 0.5) * 2.0;
      float pool = smoothstep(1.0, 0.0, r);
      gl_FragColor = vec4(uAmb * uRoom * 1.6 * pool * pool, 1.0);
    }`,
  transparent: true, blending: THREE.AdditiveBlending, depthWrite: false,
});
const floor = new THREE.Mesh(new THREE.PlaneGeometry(60, 60), floorMat);
floor.rotation.x = -Math.PI / 2; floor.position.y = -half - 4.2;
scene.add(floor);

// Wordmark under the cube: a canvas-drawn 80s block-italic with sunset
// stripes, emissive so the bloom + CRT passes treat it like the logo cards.
const capCanvas = document.createElement('canvas');
capCanvas.width = 1024; capCanvas.height = 256;
const capTex = new THREE.CanvasTexture(capCanvas);
capTex.colorSpace = THREE.SRGBColorSpace;
const capMesh = new THREE.Mesh(new THREE.PlaneGeometry(12, 3),
  new THREE.MeshBasicMaterial({ map: capTex, transparent: true, depthWrite: false }));
capMesh.position.set(0, -half - 2.6, 0);
scene.add(capMesh);
let captionShown = '';
function drawCaption(text, sub = '') {
  text = (text || '').trim();
  const key = text + '\n' + sub;
  if (key === captionShown) return;
  captionShown = key;
  const c = capCanvas.getContext('2d');
  c.clearRect(0, 0, capCanvas.width, capCanvas.height);
  if (!text) { capTex.needsUpdate = true; capMesh.visible = false; return; }
  capMesh.visible = true;
  c.font = '900 italic 150px "Helvetica Neue", "Arial Black", Arial, sans-serif';
  c.textAlign = 'center'; c.textBaseline = 'middle';
  const g = c.createLinearGradient(0, 40, 0, 200);
  g.addColorStop(0, '#fff6b0'); g.addColorStop(0.5, '#ffb347'); g.addColorStop(0.52, '#ff7a2a'); g.addColorStop(1, '#ff2d6b');
  c.fillStyle = g;
  const T = text.toUpperCase();
  let w = c.measureText(T).width;
  if (w > 960) { c.font = `900 italic ${Math.floor(150 * 960 / w)}px "Helvetica Neue", "Arial Black", Arial, sans-serif`; w = c.measureText(T).width; }
  c.fillText(T, 512, 118);
  if (sub) {
    // Legend line instead of the stripes: same glow, smaller face.
    c.font = '700 44px "Helvetica Neue", Arial, sans-serif';
    c.fillStyle = '#ffd98a';
    c.fillText(sub, 512, 218);
  } else {
    for (let i = 0; i < 3; i++) {   // speed stripes, shrinking
      c.fillStyle = ['#ff2d6b', '#ff7a2a', '#ffb347'][i];
      const sw = w * (1 - i * 0.22);
      c.fillRect(512 - sw / 2 + i * 18, 200 + i * 14, sw, 8);
    }
  }
  capTex.needsUpdate = true;
}

// ------------------------------------------------------------ post chain ----
const composer = new EffectComposer(renderer, new THREE.WebGLRenderTarget(1, 1, { type: THREE.HalfFloatType }));
composer.addPass(new RenderPass(scene, camera));
const phosphor = new PhosphorPass(1, 1, 0.72);
composer.addPass(phosphor);
const bloom = new UnrealBloomPass(new THREE.Vector2(1, 1), 0.65, 0.55, 0.0);
composer.addPass(bloom);
const crt = new ShaderPass(CRTShader);
composer.addPass(crt);
composer.addPass(new OutputPass());

function resize() {
  const w = canvas.clientWidth, h = canvas.clientHeight;
  renderer.setSize(w, h, false);
  composer.setSize(w, h);
  camera.aspect = w / h; camera.updateProjectionMatrix();
  crt.uniforms.resolution.value.set(w * renderer.getPixelRatio(), h * renderer.getPixelRatio());
}
addEventListener('resize', resize);

// ------------------------------------------------------------ patterns ----
const GAMES = new Set(['snake-3d', 'pacman-3d']);
const TOOLS = new Set(['snake-cal', 'index-walk', 'lit-pixel', 'build-map']);
const patSel = $('pattern');
for (const [label, filter] of [['Light patterns', id => !GAMES.has(id) && !TOOLS.has(id)], ['Games', id => GAMES.has(id)], ['Calibration & tools', id => TOOLS.has(id)]]) {
  const og = document.createElement('optgroup'); og.label = label;
  for (const id of patternIds.filter(filter)) { const o = document.createElement('option'); o.value = o.textContent = id; og.appendChild(o); }
  patSel.appendChild(og);
}

const state = { pattern: 'wavy-sheet', params: {}, t0: performance.now(), audio: 'off', lastGameKeyMs: -1e9 };
const paramsDiv = $('params');
function fmt(v, step) { const d = step >= 1 ? 0 : step >= 0.1 ? 1 : step >= 0.01 ? 2 : 3; return (+v).toFixed(d); }

function buildParams() {
  paramsDiv.innerHTML = '';
  const specs = E.specs[state.pattern] || [];
  for (const s of specs) {
    const cur = state.params[s.key];
    const wrap = document.createElement('div'); wrap.className = 'param'; if (s.desc) wrap.title = s.desc;
    const hdr = document.createElement('div'); hdr.className = 'hdr';
    hdr.innerHTML = `<span>${s.label}</span><span class="val"></span>`;
    wrap.appendChild(hdr);
    const val = hdr.querySelector('.val');
    let input;
    if (s.type === 0) {
      input = document.createElement('input'); input.type = 'range';
      input.min = s.min; input.max = s.max; input.step = s.step || 0.01;
      input.value = cur ?? s.def; val.textContent = fmt(input.value, +input.step);
      input.oninput = () => { val.textContent = fmt(input.value, +input.step); setParam(s.key, +input.value, 'num'); };
    } else if (s.type === 1) {
      input = document.createElement('input'); input.type = 'checkbox';
      input.checked = cur ?? !!s.def;
      input.onchange = () => setParam(s.key, input.checked, 'bool');
      hdr.style.cursor = 'pointer'; hdr.onclick = e => { if (e.target !== input) { input.checked = !input.checked; input.onchange(); } };
      hdr.insertBefore(input, hdr.firstChild);
    } else if (s.type === 2 || s.type === 3) {
      input = document.createElement('select');
      const opts = s.type === 3 ? E.palettes : s.options.split(',');
      for (const o of opts) { const e = document.createElement('option'); e.value = e.textContent = o; input.appendChild(e); }
      input.value = cur ?? s.defStr;
      input.onchange = () => setParam(s.key, input.value, 'str');
    } else {
      input = document.createElement('input'); input.type = 'text'; input.value = cur ?? s.defStr; input.maxLength = 19;
      input.onchange = () => setParam(s.key, input.value, 'str');
    }
    if (s.type !== 1) wrap.appendChild(input);
    paramsDiv.appendChild(wrap);
  }
}
function setParam(key, v, kind) {
  state.params[key] = v;
  if (kind === 'num') E.setNum(key, v); else if (kind === 'bool') E.setBool(key, v ? 1 : 0); else E.setStr(key, v);
}
function applyParams(params) {
  for (const [k, v] of Object.entries(params || {})) {
    if (typeof v === 'number') setParam(k, v, 'num');
    else if (typeof v === 'boolean') setParam(k, v, 'bool');
    else setParam(k, String(v), 'str');
  }
}
function selectPattern(id, params = {}) {
  state.pattern = id;
  state.params = {}; E.clear();
  E.select(id);
  applyParams(params);
  state.t0 = performance.now();
  buildParams();
  patSel.value = id;
  history.replaceState(null, '', '#' + id);
  state.lastGameKeyMs = -1e9;
}
patSel.onchange = () => { playlist.pause('manual pick'); selectPattern(patSel.value); };
$('resetParams').onclick = () => selectPattern(state.pattern);
$('up').onchange = () => { E.setUp($('up').value); };

// ------------------------------------------------------------- playlist ----
// Mirrors the cube: presets are {name, pattern, params, dwellSec, priority,
// reactive}; the cycle plays them in order, skipping music-reactive presets
// while audio is off, and a manual pattern pick pauses it.
const playlist = {
  presets: [], i: -1, enabled: true, dwellLeft: 0, source: 'built-in',
  shuffle: store.get('cube-shuffle', true),
  async load() {
    const imported = store.get('cube-presets', null);
    if (imported) { this.presets = imported.presets; this.source = 'imported'; }
    else {
      try { this.presets = (await (await fetch('presets.json')).json()).presets; }
      catch { this.presets = []; }
      this.source = 'built-in';
    }
    this.presets = this.presets.filter(p => patternIds.includes(p.pattern));
    this.i = -1; this.render();
  },
  eligible(p) { return state.audio !== 'off' || !p.reactive; },
  play(idx) {
    if (!this.presets.length) return;
    this.i = ((idx % this.presets.length) + this.presets.length) % this.presets.length;
    const p = this.presets[this.i];
    selectPattern(p.pattern, p.params);
    this.dwellLeft = Math.max(4, +p.dwellSec || 20);
    this.render();
  },
  // Shuffle = the cube's weighted pick: each eligible preset's chance is its
  // priority (0 never auto-plays), never the one just shown.
  pickWeighted() {
    const pool = this.presets.map((p, k) => ({ k, w: +p.priority || 0 })).filter(x => x.w > 0 && x.k !== this.i && this.eligible(this.presets[x.k]));
    if (!pool.length) return -1;
    let r = Math.random() * pool.reduce((a, x) => a + x.w, 0);
    for (const x of pool) { r -= x.w; if (r <= 0) return x.k; }
    return pool[pool.length - 1].k;
  },
  advance(dir) {
    if (!this.presets.length) return;
    if (this.shuffle && dir > 0) { const k = this.pickWeighted(); if (k >= 0) { this.play(k); return; } }
    let j = this.i;
    for (let k = 0; k < this.presets.length; k++) {
      j += dir;
      const p = this.presets[((j % this.presets.length) + this.presets.length) % this.presets.length];
      if (this.eligible(p)) { this.play(j); return; }
    }
    this.play(j);  // nothing eligible: play anyway rather than stall
  },
  pause(why) { if (this.enabled) { this.enabled = false; this.render(); } },
  resume() { this.enabled = true; if (this.i < 0) this.advance(1); this.render(); },
  tick(dt) {
    if (!this.enabled || !this.presets.length) return;
    this.dwellLeft -= dt;
    if (this.dwellLeft <= 0) this.advance(1);
  },
  current() { return this.enabled && this.i >= 0 ? this.presets[this.i] : null; },
  render() {
    $('plPlay').textContent = this.enabled ? '⏸' : '▶';
    $('plShuffle').classList.toggle('on', this.shuffle);
    $('plNow').textContent = this.i >= 0 ? this.presets[this.i].name : (this.presets.length ? 'paused' : 'no presets');
    $('plHint').textContent = `${this.presets.length} presets · ${this.source}`;
    const list = $('plList'); list.innerHTML = '';
    this.presets.forEach((p, k) => {
      const d = document.createElement('div');
      if (k === this.i) d.className = 'now';
      d.innerHTML = `<span>${p.name}</span><span class="dim">${p.pattern}${p.reactive ? ' · 🎵' : ''} · ${p.dwellSec || 20}s</span>`;
      d.onclick = () => { this.enabled = true; this.play(k); };
      list.appendChild(d);
    });
  },
};
$('plPlay').onclick = () => playlist.enabled ? playlist.pause() : playlist.resume();
$('plNext').onclick = () => { playlist.enabled = true; playlist.advance(1); };
$('plShuffle').onclick = () => { playlist.shuffle = !playlist.shuffle; store.set('cube-shuffle', playlist.shuffle); playlist.render(); };
$('plPrev').onclick = () => { playlist.enabled = true; playlist.advance(-1); };
$('plImport').onchange = async e => {
  const f = e.target.files[0]; if (!f) return;
  try {
    const doc = JSON.parse(await f.text());
    const presets = Array.isArray(doc) ? doc : doc.presets;
    if (!Array.isArray(presets)) throw new Error('expected {"presets":[...]} (the cube\'s /api/presets/export)');
    store.set('cube-presets', { presets });
    await playlist.load(); playlist.enabled = true; playlist.advance(1);
  } catch (err) { alert('Could not import: ' + err.message); }
  e.target.value = '';
};
$('plReset').onclick = async () => { store.del('cube-presets'); await playlist.load(); playlist.enabled = true; playlist.advance(1); };

// ----------------------------------------------------------- look prefs ----
const look = [
  ['LED gain', 'gain', voxelMat.uniforms.uGain, 0.2, 3, 0.05],
  ['LED size', 'size', voxelMat.uniforms.uSize, 0.1, 0.8, 0.01],
  ['Neighbour spill', 'spill', voxelMat.uniforms.uSpill, 0, 0.6, 0.01],
  ['Room light', 'room', roomMat.uniforms.uRoom, 0, 1.5, 0.01],
  ['Bloom', 'bloom', bloom, 0, 3, 0.05, 'strength'],
  ['Bloom radius', 'bloomR', bloom, 0, 1.5, 0.05, 'radius'],
  ['Phosphor persistence', 'persist', phosphor, 0, 0.95, 0.01, 'decay'],
  ['Scanlines', 'scan', crt.uniforms.scanline, 0, 1, 0.01],
  ['Scanline count', 'scanN', crt.uniforms.scanlines, 100, 800, 10],
  ['Grille mask', 'mask', crt.uniforms.mask, 0, 1, 0.01],
  ['Curvature', 'curve', crt.uniforms.curvature, 0, 0.35, 0.01],
  ['Aberration', 'aber', crt.uniforms.aberration, 0, 5, 0.1],
  ['Beam bleed', 'bleed', crt.uniforms.bleed, 0, 4, 0.1],
  ['Vignette', 'vig', crt.uniforms.vignette, 0, 1, 0.01],
  ['Grain', 'grain', crt.uniforms.grain, 0, 0.3, 0.01],
  ['Flicker', 'flicker', crt.uniforms.flicker, 0, 0.2, 0.005],
  ['Rolling bar', 'roll', crt.uniforms.rollbar, 0, 0.3, 0.01],
  ['Brightness', 'bright', crt.uniforms.brightness, 0.5, 2, 0.05],
  ['Saturation', 'sat', crt.uniforms.saturation, 0, 2, 0.05],
];
const lookDefaults = Object.fromEntries(look.map(([, k, obj, , , , prop = 'value']) => [k, obj[prop]]));
Object.assign(lookDefaults, { crt: true, orbit: true, strings: true });
const lookInputs = {};
function saveLook() {
  const v = {};
  for (const [, k, obj, , , , prop = 'value'] of look) v[k] = obj[prop];
  v.crt = $('crtOn').checked; v.orbit = $('autoRotate').checked; v.strings = $('showStrings').checked;
  store.set('cube-look', v);
}
function applyLook(v) {
  for (const [, k, obj, , , step, prop = 'value'] of look) {
    if (k in v) { obj[prop] = v[k]; lookInputs[k].input.value = v[k]; lookInputs[k].val.textContent = fmt(v[k], step); }
  }
  if ('crt' in v) { $('crtOn').checked = v.crt; crt.enabled = v.crt; }
  if ('orbit' in v) { $('autoRotate').checked = v.orbit; controls.autoRotate = v.orbit; }
  if ('strings' in v) { $('showStrings').checked = v.strings; strings.visible = v.strings; }
}
for (const [label, key, obj, min, max, step, prop = 'value'] of look) {
  const wrap = document.createElement('div'); wrap.className = 'param';
  wrap.innerHTML = `<div class="hdr"><span>${label}</span><span class="val">${fmt(obj[prop], step)}</span></div>`;
  const input = document.createElement('input'); input.type = 'range'; input.min = min; input.max = max; input.step = step; input.value = obj[prop];
  const val = wrap.querySelector('.val');
  input.oninput = () => { obj[prop] = +input.value; val.textContent = fmt(input.value, step); saveLook(); };
  wrap.appendChild(input); $('look').appendChild(wrap);
  lookInputs[key] = { input, val };
}
$('crtOn').onchange = () => { crt.enabled = $('crtOn').checked; saveLook(); };
$('autoRotate').onchange = () => { controls.autoRotate = $('autoRotate').checked; saveLook(); };
$('showStrings').onchange = () => { strings.visible = $('showStrings').checked; saveLook(); };
$('resetLook').onclick = e => { e.preventDefault(); e.stopPropagation(); store.del('cube-look'); applyLook(lookDefaults); };
applyLook(store.get('cube-look', {}));
$('caption').oninput = () => store.set('cube-caption', $('caption').value);
$('caption').value = store.get('cube-caption', '');

// --------------------------------------------------------------- panel ----
function setPanel(hidden) { $('panel').classList.toggle('hidden', hidden); $('panelShow').hidden = !hidden; }
$('panelToggle').onclick = () => setPanel(true);
$('panelShow').onclick = () => setPanel(false);
addEventListener('keydown', e => {
  if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT' || e.target.tagName === 'TEXTAREA') return;
  if (e.key === 'Tab') { e.preventDefault(); setPanel(!$('panel').classList.contains('hidden')); return; }
  // Game pad: XP=0 XN=1 YP=2 YN=3 ZP=4 ZN=5 (SnakeDir).
  const map = { ArrowRight: 0, ArrowLeft: 1, ArrowUp: 2, ArrowDown: 3, w: 4, s: 5, W: 4, S: 5 };
  if (e.key in map && GAMES.has(state.pattern)) {
    e.preventDefault();
    (state.pattern === 'snake-3d' ? E.snake : E.pacman)(map[e.key]);
    state.lastGameKeyMs = performance.now();
    playlist.pause('playing');
  }
});

// ----------------------------------------------------------------- audio ----
const mic = new MicAnalyzer();
const audioFrame = { level: 0, bands: new Float32Array(BANDS), beat: 0, bpm: 0 };
const meterBands = $('audioMeter').querySelector('.bands');
for (let i = 0; i < BANDS; i++) meterBands.appendChild(document.createElement('i'));
const meterBeat = $('audioMeter').querySelector('.beat'), meterBpm = $('audioMeter').querySelector('.bpm');
async function setAudio(v) {
  if (state.audio === 'mic') mic.stop();
  state.audio = v;
  if (v === 'mic') {
    try { await mic.start(); }
    catch (err) { alert('Microphone unavailable: ' + err.message); state.audio = 'off'; }
  }
  $('audioSrc').value = state.audio;
  store.set('cube-audio', state.audio);
}
$('audioSrc').onchange = () => setAudio($('audioSrc').value);

// ---------------------------------------------------------------- capture ----
let recorder = null;
function record(seconds) {
  if (recorder) return;
  const stream = canvas.captureStream(30);
  // Mix the mic into the recording so exported clips carry the music.
  if (state.audio === 'mic' && mic.stream) for (const t of mic.stream.getAudioTracks()) stream.addTrack(t);
  const mime = ['video/webm;codecs=vp9,opus', 'video/webm;codecs=vp9', 'video/webm', 'video/mp4'].find(m => MediaRecorder.isTypeSupported(m));
  recorder = new MediaRecorder(stream, { mimeType: mime, videoBitsPerSecond: 8_000_000 });
  const chunks = [];
  recorder.ondataavailable = e => chunks.push(e.data);
  recorder.onstop = () => {
    const blob = new Blob(chunks, { type: mime });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = `cube-light-${state.pattern}-${seconds}s.${mime.includes('mp4') ? 'mp4' : 'webm'}`;
    a.click();
    $('recStatus').textContent = `saved ${a.download} (${(blob.size / 1e6).toFixed(1)} MB)`;
    recorder = null;
    for (const b of [$('rec5'), $('rec10')]) b.classList.remove('rec');
  };
  recorder.start();
  for (const b of [$('rec5'), $('rec10')]) b.classList.add('rec');
  let left = seconds;
  const tick = () => { $('recStatus').textContent = `recording… ${left}s${state.audio === 'mic' ? ' (with mic)' : ''}`; if (left-- > 0) setTimeout(tick, 1000); else recorder.stop(); };
  tick();
}
$('rec5').onclick = () => record(5);
$('rec10').onclick = () => record(10);
$('snap').onclick = () => {
  const a = document.createElement('a'); a.href = canvas.toDataURL('image/png'); a.download = `cube-light-${state.pattern}.png`; a.click();
};

// ------------------------------------------------------------------ loop ----
let lastFrame = performance.now();
const amb = new THREE.Color();
const manualNow = () => GAMES.has(state.pattern) && performance.now() - state.lastGameKeyMs < 12000;
function frame(now) {
  requestAnimationFrame(frame);
  const dt = Math.min(0.1, (now - lastFrame) / 1000); lastFrame = now;
  const t = (now - state.t0) / 1000;

  // Audio -> engine.
  if (state.audio === 'mic') {
    const a = mic.update();
    if (a) { E.beatFeed(a.bass, now | 0); audioFrame.level = a.level; audioFrame.bands.set(a.bands); }
    E.beatDecay(dt);
    audioFrame.beat = E.beatEnv(); audioFrame.bpm = E.beatBpm(now | 0);
  } else if (state.audio === 'fake') {
    fakeAudio(t, audioFrame);
  } else {
    audioFrame.level = audioFrame.beat = audioFrame.bpm = 0; audioFrame.bands.fill(0);
  }
  bandsHeap.set(audioFrame.bands);
  E.setAudio(audioFrame.level, bandsPtr, audioFrame.beat, audioFrame.bpm);
  for (let i = 0; i < BANDS; i++) meterBands.children[i].style.height = `${2 + audioFrame.bands[i] * 24}px`;
  meterBeat.style.opacity = 0.15 + audioFrame.beat * 0.85;
  meterBpm.textContent = audioFrame.bpm > 0 ? `${audioFrame.bpm.toFixed(0)} bpm` : '';

  // Playlist dwell + wordmark.
  playlist.tick(dt);
  const typed = $('caption').value;
  if (GAMES.has(state.pattern)) {
    const name = typed || playlist.current()?.name || (state.pattern === 'snake-3d' ? 'SNAKE' : 'PAC-MAN');
    drawCaption(name, manualNow() ? 'YOU DRIVE  ·  ← → ↑ ↓  ·  W / S UP · DOWN' : 'PRESS ← → ↑ ↓ OR W / S TO TAKE OVER');
  } else {
    drawCaption(typed || playlist.current()?.name || '');
  }

  // Engine -> voxel colours, neighbour spill, room ambience.
  const ptr = E.render(t);
  const buf = M.HEAPU8.subarray(ptr, ptr + LEDS * 3);
  let sr = 0, sg = 0, sb = 0;
  for (let i = 0; i < LEDS * 3; i += 3) {
    colors[i] = buf[i] / 255; colors[i + 1] = buf[i + 1] / 255; colors[i + 2] = buf[i + 2] / 255;
    sr += colors[i]; sg += colors[i + 1]; sb += colors[i + 2];
  }
  for (let i = 0; i < LEDS; i++) {
    let r = 0, g = 0, b = 0;
    for (let k = 0; k < 6; k++) { const j = nbr[i * 6 + k]; if (j >= 0) { r += colors[j * 3]; g += colors[j * 3 + 1]; b += colors[j * 3 + 2]; } }
    spill[i * 3] = r; spill[i * 3 + 1] = g; spill[i * 3 + 2] = b;
  }
  inst.attributes.aColor.needsUpdate = true;
  inst.attributes.aSpill.needsUpdate = true;
  // Mean light per LED, lifted a little so a handful of pixels still register.
  amb.setRGB(Math.pow(sr / LEDS, 0.8), Math.pow(sg / LEDS, 0.8), Math.pow(sb / LEDS, 0.8));
  roomMat.uniforms.uAmb.value.copy(amb);

  // Games in manual mode: hold the camera still, brighten the unlit beads so
  // the board reads, keep the hint up; hand back after 12 s without a key.
  const manual = manualNow();
  voxelMat.uniforms.uFloor.value = manual ? 0.11 : 0.035;
  controls.autoRotate = $('autoRotate').checked && !manual;
  // Orbit speed follows the tempo: 120 BPM = the base rate, with a nudge on
  // each beat.
  const tempo = audioFrame.bpm > 0 ? audioFrame.bpm / 120 : 1;
  controls.autoRotateSpeed = BASE_ORBIT * tempo * (1 + 1.5 * audioFrame.beat);

  crt.uniforms.time.value = now / 1000;
  controls.update();
  capMesh.quaternion.copy(camera.quaternion);
  composer.render();
}

// --------------------------------------------------------------- startup ----
await playlist.load();
const fromHash = patternIds.includes(location.hash.slice(1));
if (fromHash) { playlist.enabled = false; selectPattern(location.hash.slice(1)); }
else if (playlist.presets.length) playlist.advance(1);
else selectPattern('wavy-sheet', { palette: 'sunset' });
playlist.render();
resize();
$('loading').hidden = true;
requestAnimationFrame(frame);

// Onboarding: offer the mic before anything else (the click doubles as the
// user gesture WebAudio needs).
const remembered = store.get('cube-audio', null);
if (remembered && remembered !== 'mic') { $('modal').hidden = true; setAudio(remembered); }
$('modalMic').onclick = async () => { $('modal').hidden = true; await setAudio('mic'); };
$('modalFake').onclick = () => { $('modal').hidden = true; setAudio('fake'); };
$('modalOff').onclick = () => { $('modal').hidden = true; setAudio('off'); };
