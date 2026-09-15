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
const camera = new THREE.PerspectiveCamera(32, 1, 0.1, 200);
camera.position.set(22, 13, 28);
const controls = new OrbitControls(camera, canvas);
controls.enableDamping = true; controls.dampingFactor = 0.08;
controls.autoRotate = true; controls.autoRotateSpeed = 0.6;
controls.target.set(0, -0.3, 0);
controls.minDistance = 10; controls.maxDistance = 90;

// Voxels: one instanced billboard quad per LED with a soft-disc shader.
// Instance attribute `aColor` is refilled from the engine buffer every frame.
const half = (N - 1) / 2;
const quad = new THREE.PlaneGeometry(1, 1);
const inst = new THREE.InstancedBufferGeometry().copy(quad);
inst.instanceCount = LEDS;
const offsets = new Float32Array(LEDS * 3);
const colors = new Float32Array(LEDS * 3);
// ledToXYZ: the engine writes buffer[idx(x,y,z)]; we need the inverse.
const ledPos = new Int16Array(LEDS * 3);
for (let z = 0; z < N; z++) for (let y = 0; y < N; y++) for (let x = 0; x < N; x++) {
  const i = E.index(x, y, z);
  ledPos[i * 3] = x; ledPos[i * 3 + 1] = y; ledPos[i * 3 + 2] = z;
}
function rebuildOffsets() {
  for (let i = 0; i < LEDS; i++) {
    // Design z is "up": map to world Y. World X = x, world Z = -y so the
    // default camera sees the x/z face the console's preview showed.
    offsets[i * 3] = ledPos[i * 3] - half;
    offsets[i * 3 + 1] = ledPos[i * 3 + 2] - half;
    offsets[i * 3 + 2] = -(ledPos[i * 3 + 1] - half);
  }
  inst.attributes.aOffset.needsUpdate = true;
}
inst.setAttribute('aOffset', new THREE.InstancedBufferAttribute(offsets, 3));
inst.setAttribute('aColor', new THREE.InstancedBufferAttribute(colors, 3));
rebuildOffsets();
const voxelMat = new THREE.ShaderMaterial({
  uniforms: { uSize: { value: 0.3 }, uGain: { value: 0.75 }, uFloor: { value: 0.035 } },
  vertexShader: `
    attribute vec3 aOffset; attribute vec3 aColor;
    uniform float uSize; varying vec3 vColor; varying vec2 vUv;
    void main(){
      vColor = aColor; vUv = uv;
      // Billboard: expand the quad in view space so every LED faces the camera.
      vec4 c = modelViewMatrix * vec4(aOffset, 1.0);
      float lum = max(max(aColor.r, aColor.g), aColor.b);
      float s = uSize * (0.75 + 0.9 * lum);   // bright LEDs bloom bigger
      c.xy += position.xy * s;
      gl_Position = projectionMatrix * c;
    }`,
  fragmentShader: `
    uniform float uGain, uFloor; varying vec3 vColor; varying vec2 vUv;
    void main(){
      vec2 d = vUv - 0.5; float r = length(d) * 2.0;
      // Hot core + soft phosphor halo; a faint grey bead for unlit LEDs so
      // the physical lattice reads in the dark.
      float core = smoothstep(0.55, 0.0, r);
      float halo = smoothstep(1.0, 0.2, r) * 0.55;
      vec3 lit = vColor * uGain;
      float lum = max(max(lit.r, lit.g), lit.b);
      vec3 col = lit * (core * 1.6 + halo) + vec3(uFloor) * core * (1.0 - lum);
      // Push saturated colors toward white in the core, like an overdriven LED.
      col += vec3(core * core) * lum * 0.35;
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
function drawCaption(text) {
  const c = capCanvas.getContext('2d');
  c.clearRect(0, 0, capCanvas.width, capCanvas.height);
  if (!text) { capTex.needsUpdate = true; capMesh.visible = false; return; }
  capMesh.visible = true;
  c.font = '900 italic 150px "Helvetica Neue", "Arial Black", Arial, sans-serif';
  c.textAlign = 'center'; c.textBaseline = 'middle';
  const g = c.createLinearGradient(0, 40, 0, 200);
  g.addColorStop(0, '#fff6b0'); g.addColorStop(0.5, '#ffb347'); g.addColorStop(0.52, '#ff7a2a'); g.addColorStop(1, '#ff2d6b');
  c.fillStyle = g;
  c.fillText(text.toUpperCase(), 512, 118);
  // Speed stripes under the word, shrinking like the Montreal / GTA marks.
  const w = Math.min(900, c.measureText(text.toUpperCase()).width);
  for (let i = 0; i < 3; i++) {
    c.fillStyle = ['#ff2d6b', '#ff7a2a', '#ffb347'][i];
    const sw = w * (1 - i * 0.22);
    c.fillRect(512 - sw / 2 + i * 18, 200 + i * 14, sw, 8);
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

// ------------------------------------------------------------------- UI ----
const GAMES = new Set(['snake-3d', 'pacman-3d']);
const TOOLS = new Set(['snake-cal', 'index-walk', 'lit-pixel', 'build-map']);
const patSel = $('pattern');
for (const [label, filter] of [['Light patterns', id => !GAMES.has(id) && !TOOLS.has(id)], ['Games', id => GAMES.has(id)], ['Calibration & tools', id => TOOLS.has(id)]]) {
  const og = document.createElement('optgroup'); og.label = label;
  for (const id of patternIds.filter(filter)) { const o = document.createElement('option'); o.value = o.textContent = id; og.appendChild(o); }
  patSel.appendChild(og);
}

const state = { pattern: 'wavy-sheet', params: {}, t0: performance.now(), audio: 'off' };
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
    if (s.type === 0) {                      // number
      input = document.createElement('input'); input.type = 'range';
      input.min = s.min; input.max = s.max; input.step = s.step || 0.01;
      input.value = cur ?? s.def; val.textContent = fmt(input.value, +input.step);
      input.oninput = () => { val.textContent = fmt(input.value, +input.step); setParam(s.key, +input.value, 'num'); };
    } else if (s.type === 1) {               // bool
      input = document.createElement('input'); input.type = 'checkbox';
      input.checked = cur ?? !!s.def;
      input.onchange = () => setParam(s.key, input.checked, 'bool');
      hdr.style.cursor = 'pointer'; hdr.onclick = e => { if (e.target !== input) { input.checked = !input.checked; input.onchange(); } };
      hdr.insertBefore(input, hdr.firstChild);
    } else if (s.type === 2 || s.type === 3) { // enum / palette
      input = document.createElement('select');
      const opts = s.type === 3 ? E.palettes : s.options.split(',');
      for (const o of opts) { const e = document.createElement('option'); e.value = e.textContent = o; input.appendChild(e); }
      input.value = cur ?? s.defStr;
      input.onchange = () => setParam(s.key, input.value, 'str');
    } else if (s.type === 5) {               // color (rgb string?) treat as text
      input = document.createElement('input'); input.type = 'text'; input.value = cur ?? s.defStr;
      input.onchange = () => setParam(s.key, input.value, 'str');
    } else {                                 // string
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
function selectPattern(id, keepParams = false) {
  state.pattern = id;
  if (!keepParams) { state.params = {}; E.clear(); }
  E.select(id);
  state.t0 = performance.now();
  buildParams();
  patSel.value = id;
  location.hash = id;
}
patSel.onchange = () => selectPattern(patSel.value);
$('resetParams').onclick = () => selectPattern(state.pattern);
$('up').onchange = () => { E.setUp($('up').value); };

// Look controls.
const look = [
  ['LED gain', voxelMat.uniforms.uGain, 0.2, 3, 0.05],
  ['LED size', voxelMat.uniforms.uSize, 0.1, 0.8, 0.01],
  ['Bloom', bloom, 0, 3, 0.05, 'strength'],
  ['Bloom radius', bloom, 0, 1.5, 0.05, 'radius'],
  ['Phosphor persistence', phosphor, 0, 0.95, 0.01, 'decay'],
  ['Scanlines', crt.uniforms.scanline, 0, 1, 0.01],
  ['Scanline count', crt.uniforms.scanlines, 100, 800, 10],
  ['Grille mask', crt.uniforms.mask, 0, 1, 0.01],
  ['Curvature', crt.uniforms.curvature, 0, 0.35, 0.01],
  ['Aberration', crt.uniforms.aberration, 0, 5, 0.1],
  ['Beam bleed', crt.uniforms.bleed, 0, 4, 0.1],
  ['Vignette', crt.uniforms.vignette, 0, 1, 0.01],
  ['Grain', crt.uniforms.grain, 0, 0.3, 0.01],
  ['Flicker', crt.uniforms.flicker, 0, 0.2, 0.005],
  ['Rolling bar', crt.uniforms.rollbar, 0, 0.3, 0.01],
  ['Brightness', crt.uniforms.brightness, 0.5, 2, 0.05],
  ['Saturation', crt.uniforms.saturation, 0, 2, 0.05],
];
for (const [label, obj, min, max, step, prop = 'value'] of look) {
  const wrap = document.createElement('div'); wrap.className = 'param';
  wrap.innerHTML = `<div class="hdr"><span>${label}</span><span class="val">${fmt(obj[prop], step)}</span></div>`;
  const input = document.createElement('input'); input.type = 'range'; input.min = min; input.max = max; input.step = step; input.value = obj[prop];
  input.oninput = () => { obj[prop] = +input.value; wrap.querySelector('.val').textContent = fmt(input.value, step); };
  wrap.appendChild(input); $('look').appendChild(wrap);
}
$('crtOn').onchange = () => { crt.enabled = $('crtOn').checked; };
$('autoRotate').onchange = () => { controls.autoRotate = $('autoRotate').checked; };
$('showStrings').onchange = () => { strings.visible = $('showStrings').checked; };
$('caption').oninput = () => drawCaption($('caption').value);
$('panelToggle').onclick = () => $('panel').classList.toggle('hidden');
addEventListener('keydown', e => {
  if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;
  if (e.key === 'Tab') { e.preventDefault(); $('panel').classList.toggle('hidden'); return; }
  // Game pad: XP=0 XN=1 YP=2 YN=3 ZP=4 ZN=5 (SnakeDir).
  const map = { ArrowRight: 0, ArrowLeft: 1, ArrowUp: 2, ArrowDown: 3, w: 4, s: 5, W: 4, S: 5 };
  if (e.key in map) {
    e.preventDefault();
    if (state.pattern === 'snake-3d') E.snake(map[e.key]);
    else if (state.pattern === 'pacman-3d') E.pacman(map[e.key]);
  }
});

// ----------------------------------------------------------------- audio ----
const mic = new MicAnalyzer();
const audioFrame = { level: 0, bands: new Float32Array(BANDS), beat: 0, bpm: 0 };
const meterBands = $('audioMeter').querySelector('.bands');
for (let i = 0; i < BANDS; i++) meterBands.appendChild(document.createElement('i'));
const meterBeat = $('audioMeter').querySelector('.beat'), meterBpm = $('audioMeter').querySelector('.bpm');
$('audioSrc').onchange = async () => {
  const v = $('audioSrc').value;
  if (state.audio === 'mic') mic.stop();
  state.audio = v;
  if (v === 'mic') {
    try { await mic.start(); } catch (err) { alert('Microphone unavailable: ' + err.message); $('audioSrc').value = state.audio = 'off'; }
  }
};

// ---------------------------------------------------------------- capture ----
let recorder = null;
function record(seconds) {
  if (recorder) return;
  const stream = canvas.captureStream(30);
  const mime = ['video/webm;codecs=vp9', 'video/webm;codecs=vp8', 'video/webm', 'video/mp4'].find(m => MediaRecorder.isTypeSupported(m));
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
  const tick = () => { $('recStatus').textContent = `recording… ${left}s`; if (left-- > 0) setTimeout(tick, 1000); else recorder.stop(); };
  tick();
}
$('rec5').onclick = () => record(5);
$('rec10').onclick = () => record(10);
$('snap').onclick = () => {
  const a = document.createElement('a'); a.href = canvas.toDataURL('image/png'); a.download = `cube-light-${state.pattern}.png`; a.click();
};

// ------------------------------------------------------------------ loop ----
let lastFrame = performance.now();
function frame(now) {
  requestAnimationFrame(frame);
  const dt = Math.min(0.1, (now - lastFrame) / 1000); lastFrame = now;
  const t = (now - state.t0) / 1000;

  // Audio -> engine.
  if (state.audio === 'mic') {
    const a = mic.update();
    if (a) {
      E.beatFeed(a.bass, now | 0);
      audioFrame.level = a.level; audioFrame.bands.set(a.bands);
    }
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

  // Engine -> voxel colors.
  const ptr = E.render(t);
  const buf = M.HEAPU8.subarray(ptr, ptr + LEDS * 3);
  for (let i = 0; i < LEDS * 3; i++) colors[i] = buf[i] / 255;
  inst.attributes.aColor.needsUpdate = true;

  crt.uniforms.time.value = now / 1000;
  controls.update();
  capMesh.quaternion.copy(camera.quaternion);  // wordmark always faces the viewer
  composer.render();
}

// --------------------------------------------------------------- startup ----
const fromHash = patternIds.includes(location.hash.slice(1));
selectPattern(fromHash ? location.hash.slice(1) : 'wavy-sheet');
if (!fromHash) { setParam('palette', 'sunset', 'str'); buildParams(); }  // landing look
drawCaption('');
resize();
$('loading').hidden = true;
requestAnimationFrame(frame);
