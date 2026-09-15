// Post-processing for the "1984 cathode wordmark" look: phosphor persistence,
// halation/bloom (three's UnrealBloom), then one big CRT composite pass —
// barrel curvature, chromatic aberration, horizontal beam bleed, luminance-
// dependent scanlines, aperture-grille mask, vignette with rounded corners,
// grain, flicker and a slow rolling bar.
import * as THREE from 'three';
import { Pass, FullScreenQuad } from 'three/addons/postprocessing/Pass.js';

// ---- Phosphor persistence: max(current, previous * decay) ----------------
// Mimics the slow-fading phosphor of a tube: bright voxels leave a short
// trail as they move. Implemented as a feedback render target.
export class PhosphorPass extends Pass {
  constructor(width, height, decay = 0.72) {
    super();
    this.decay = decay;
    const opts = { type: THREE.HalfFloatType, minFilter: THREE.LinearFilter, magFilter: THREE.LinearFilter };
    this.prev = new THREE.WebGLRenderTarget(width, height, opts);
    this.tmp = new THREE.WebGLRenderTarget(width, height, opts);
    this.material = new THREE.ShaderMaterial({
      uniforms: { tCur: { value: null }, tPrev: { value: null }, decay: { value: decay } },
      vertexShader: `varying vec2 vUv; void main(){ vUv = uv; gl_Position = vec4(position.xy, 0.0, 1.0); }`,
      fragmentShader: `
        uniform sampler2D tCur, tPrev; uniform float decay; varying vec2 vUv;
        void main(){
          vec3 c = texture2D(tCur, vUv).rgb;
          vec3 p = texture2D(tPrev, vUv).rgb * decay;
          // Additive-ish but bounded: keeps the trail from blowing out.
          gl_FragColor = vec4(max(c, p), 1.0);
        }`,
    });
    this.copy = new THREE.ShaderMaterial({
      uniforms: { tDiffuse: { value: null } },
      vertexShader: this.material.vertexShader,
      fragmentShader: `uniform sampler2D tDiffuse; varying vec2 vUv; void main(){ gl_FragColor = texture2D(tDiffuse, vUv); }`,
    });
    this.fsQuad = new FullScreenQuad(this.material);
    this.needsSwap = true;
  }
  setSize(w, h) { this.prev.setSize(w, h); this.tmp.setSize(w, h); }
  render(renderer, writeBuffer, readBuffer) {
    this.material.uniforms.tCur.value = readBuffer.texture;
    this.material.uniforms.tPrev.value = this.prev.texture;
    this.material.uniforms.decay.value = this.decay;
    this.fsQuad.material = this.material;
    renderer.setRenderTarget(this.tmp);
    this.fsQuad.render(renderer);
    // tmp -> prev (feedback) and tmp -> writeBuffer (output)
    this.copy.uniforms.tDiffuse.value = this.tmp.texture;
    this.fsQuad.material = this.copy;
    renderer.setRenderTarget(this.prev);
    this.fsQuad.render(renderer);
    renderer.setRenderTarget(this.renderToScreen ? null : writeBuffer);
    this.fsQuad.render(renderer);
  }
  dispose() { this.prev.dispose(); this.tmp.dispose(); this.material.dispose(); this.copy.dispose(); this.fsQuad.dispose(); }
}

// ---- The CRT composite -----------------------------------------------------
export const CRTShader = {
  uniforms: {
    tDiffuse: { value: null },
    time: { value: 0 },
    resolution: { value: new THREE.Vector2(1, 1) },
    curvature: { value: 0.12 },   // barrel amount
    scanline: { value: 0.55 },    // scanline depth 0..1
    scanlines: { value: 300.0 },  // count across the height
    mask: { value: 0.35 },        // aperture-grille strength
    maskScale: { value: 1.0 },    // grille pitch in device px (1 = every px)
    aberration: { value: 1.6 },   // px of RGB split at the edge
    bleed: { value: 1.2 },        // horizontal beam smear in px
    vignette: { value: 0.35 },
    grain: { value: 0.08 },
    flicker: { value: 0.03 },
    rollbar: { value: 0.06 },
    brightness: { value: 1.15 },
    saturation: { value: 1.15 },
    cornerRadius: { value: 0.06 },
  },
  vertexShader: `varying vec2 vUv; void main(){ vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`,
  fragmentShader: `
    uniform sampler2D tDiffuse;
    uniform float time; uniform vec2 resolution;
    uniform float curvature, scanline, scanlines, mask, maskScale, aberration, bleed;
    uniform float vignette, grain, flicker, rollbar, brightness, saturation, cornerRadius;
    varying vec2 vUv;

    float hash(vec2 p){ return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }

    // Barrel distortion: push uv outward by r^2 so the middle stays put and
    // the edges bow like glass.
    vec2 curve(vec2 uv){
      vec2 c = uv * 2.0 - 1.0;
      float r2 = dot(c, c);
      c *= 1.0 + curvature * r2;
      return c * 0.5 + 0.5;
    }

    // Sample with horizontal smear: 5 taps weighted like a beam spot.
    vec3 beam(vec2 uv, float off){
      vec2 px = vec2(bleed / resolution.x, 0.0);
      vec3 s = vec3(0.0);
      s += texture2D(tDiffuse, uv + vec2(off, 0.0) - 2.0 * px).rgb * 0.10;
      s += texture2D(tDiffuse, uv + vec2(off, 0.0) - 1.0 * px).rgb * 0.22;
      s += texture2D(tDiffuse, uv + vec2(off, 0.0)).rgb            * 0.36;
      s += texture2D(tDiffuse, uv + vec2(off, 0.0) + 1.0 * px).rgb * 0.22;
      s += texture2D(tDiffuse, uv + vec2(off, 0.0) + 2.0 * px).rgb * 0.10;
      return s;
    }

    void main(){
      vec2 uv = curve(vUv);
      // Rounded-corner tube edge: outside the glass is black.
      vec2 q = abs(uv - 0.5) - (0.5 - cornerRadius);
      float d = length(max(q, 0.0)) - cornerRadius;
      float glass = 1.0 - smoothstep(-0.004, 0.002, d);
      if (glass <= 0.0) { gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }

      // Chromatic aberration grows toward the edges (lens + misconverged guns).
      vec2 c = uv - 0.5;
      float edge = dot(c, c) * 2.0;
      float ab = aberration * edge / resolution.x;
      vec3 col;
      col.r = beam(uv, +ab).r;
      col.g = beam(uv, 0.0).g;
      col.b = beam(uv, -ab).b;

      // Scanlines: a raised-cosine beam profile whose width grows with
      // luminance, so bright areas fill in and dark areas show the gaps —
      // the way a real beam blooms.
      float lum = dot(col, vec3(0.299, 0.587, 0.114));
      float ph = uv.y * scanlines * 6.28318;
      float prof = 0.5 + 0.5 * cos(ph);
      float width = mix(1.6, 0.35, clamp(lum * 1.4, 0.0, 1.0));
      float sl = pow(prof, width);
      col *= mix(1.0, sl * 1.35, scanline);

      // Aperture grille: RGB stripes across device pixels.
      float gx = floor(gl_FragCoord.x / maskScale);
      float m = mod(gx, 3.0);
      vec3 grille = vec3(m == 0.0 ? 1.0 : 0.55, m == 1.0 ? 1.0 : 0.55, m == 2.0 ? 1.0 : 0.55);
      col *= mix(vec3(1.0), grille * 1.25, mask);

      // Rolling brightness bar (the bar that drifts through a filmed CRT).
      float bar = smoothstep(0.0, 0.25, fract(uv.y - time * 0.11)) * smoothstep(0.55, 0.25, fract(uv.y - time * 0.11));
      col *= 1.0 + rollbar * bar;
      // Flicker at ~mains-ish beat plus a slower sway.
      col *= 1.0 - flicker * (0.5 + 0.5 * sin(time * 120.0)) * (0.5 + 0.5 * sin(time * 3.1));
      // Grain.
      col += (hash(vUv * resolution + fract(time) * 100.0) - 0.5) * grain * (0.3 + lum);

      // Vignette: darker toward the corners, with the glass mask.
      float vig = 1.0 - vignette * smoothstep(0.35, 1.3, dot(c, c) * 3.2);
      col *= vig * glass;

      // Saturation + brightness, then a gentle shoulder so hot spots roll off.
      float l = dot(col, vec3(0.299, 0.587, 0.114));
      col = mix(vec3(l), col, saturation) * brightness;
      col = col / (1.0 + col * 0.15);
      gl_FragColor = vec4(col, 1.0);
    }`,
};
