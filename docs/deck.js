// SPDX-License-Identifier: GPL-3.0-only
// The installer's turntable: a head (glTF) that turns into a record while the FM-1 updates. The record is cut with
// a real scratch track rendered by the firmware itself (tests/pro_techniques.txt on the host build), and the same
// track moves the skin while it is still a head.
import * as THREE from "three";
import { GLTFLoader } from "three/addons/loaders/GLTFLoader.js";

const W = 512;                                  // waveform texture: row 0 = 23 ms of sound, row 1 = 2.5 s of loudness
const ENV_HZ = 200;

const VERT = /* glsl */ `
uniform float uTime, uMorph, uAmp, uKick, uSpin, uTilt;
uniform sampler2D uWave;
attribute float cang;
varying vec2 vUv;
varying vec3 vView;
varying float vM, vRad, vGroove, vH;

float hash(float n) { return fract(sin(n) * 43758.5453); }
float wave(float x) { return texture2D(uWave, vec2(x, 0.25)).r * 2.0 - 1.0; }
float env(float x) { return texture2D(uWave, vec2(x, 0.75)).r; }
vec3 rotY(vec3 p, float a) { float c = cos(a), s = sin(a); return vec3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z); }
vec3 rotX(vec3 p, float a) { float c = cos(a), s = sin(a); return vec3(p.x, c * p.y - s * p.z, s * p.y + c * p.z); }

void main() {
  vec3 p = position;
  float h = clamp((p.y + 1.1) / 2.2, 0.0, 1.0);                 // 0 neck .. 1 crown

  // HEAD: the skin ripples with the scratch, slices slip on the transients
  float w = wave(fract(h + uTime * 0.05));
  vec3 head = p + normal * w * (0.008 + uAmp * 0.06);
  float band = floor(p.y * 16.0) + floor(uTime * 10.0);
  float r = hash(band);
  head.x += step(0.94, r) * (r - 0.97) * uKick * 2.0;

  // RECORD: crown on the spindle, neck at the edge, the track cut into a spiral groove
  float raw = atan(p.x, p.z);
  float a = cang + atan(sin(raw - cang), cos(raw - cang));      // unwrapped next to its triangle: no seam
  float rad = 0.16 + (1.0 - h) * 1.05;
  float turns = 9.0;
  float spiral = (rad - 0.16) / 1.05 * turns - a / 6.2831853;
  float cut = wave(fract(spiral * 0.41)) * (0.25 + env(fract(spiral / turns)));
  vec3 disc = vec3(cos(a + uSpin) * rad, cut * (0.006 + uAmp * 0.03), sin(a + uSpin) * rad);
  disc = rotX(disc, uTilt);

  // the morph peels from the crown down, twisting on the way
  float m = smoothstep(0.0, 1.0, clamp(uMorph * 1.7 - (1.0 - h) * 0.7, 0.0, 1.0));
  vec3 q = mix(head, disc, m);
  q = rotY(q, m * (1.0 - m) * 6.0 * (h - 0.5));
  q += normal * m * (1.0 - m) * (0.1 + uKick * 0.5);

  vUv = uv; vM = m; vRad = rad; vGroove = spiral; vH = h;
  vec4 mv = modelViewMatrix * vec4(q, 1.0);
  vView = mv.xyz;
  gl_Position = projectionMatrix * mv;
}`;

const FRAG = /* glsl */ `
uniform sampler2D uTex;
uniform float uTime, uErr, uAmp, uDots;
varying vec2 vUv;
varying vec3 vView;
varying float vM, vRad, vGroove, vH;

void main() {
  vec3 n = normalize(cross(dFdx(vView), dFdy(vView)));
  vec3 v = normalize(-vView);
  vec3 L = normalize(vec3(-0.55, 0.75, 0.6));
  float diff = max(dot(n, L), 0.0);
  float rim = pow(1.0 - max(dot(n, v), 0.0), 3.0);
  vec3 bone = vec3(0.91, 0.88, 0.80), ink = vec3(0.06, 0.055, 0.05), rust = vec3(0.76, 0.19, 0.10);

  // HEAD: a photocopied flyer, halftone dots on a 45 degree screen
  float lum = dot(texture2D(uTex, vUv).rgb, vec3(0.3, 0.59, 0.11));
  float tone = clamp((diff * 0.9 + 0.1) * (0.5 + lum * 0.8) + rim * 0.35, 0.0, 1.0);
  vec2 g = gl_FragCoord.xy / uDots;
  g = vec2(g.x - g.y, g.x + g.y) * 0.7071;
  float d = length(fract(g) - 0.5);
  vec3 headc = mix(bone, ink, step(d, (1.0 - tone) * 0.66));
  headc = mix(headc, rust, smoothstep(0.55, 0.9, rim) * 0.85);

  // RECORD: black vinyl, groove sheen, a rust label
  vec3 rf = reflect(-v, n);
  float sheen = pow(max(dot(rf, L), 0.0), 10.0);
  float line = abs(fract(vGroove) - 0.5);
  vec3 vin = vec3(0.05) + sheen * 0.6 + (1.0 - smoothstep(0.0, 0.12, line)) * (0.1 + uAmp * 0.35) * mix(rust, bone, 0.35);

  vec3 c = mix(headc, vin, vM);
  c = mix(c, rust, uErr * 0.65);
  gl_FragColor = vec4(c, 1.0);
}`;

// split every triangle in four, `levels` times, so the displacement has vertices to move
function subdivide(P, N, U, levels) {
  for (let l = 0; l < levels; l++) {
    const tris = P.length / 9;
    const split = (src, dim) => {                 // corners a b c -> (a ab ca) (ab b bc) (ca bc c) (ab bc ca)
      const out = new Float32Array(tris * 12 * dim);
      const ORDER = [0, 3, 5, 3, 1, 4, 5, 4, 2, 3, 4, 5];
      const v = new Float32Array(6 * dim);
      for (let t = 0; t < tris; t++) {
        v.set(src.subarray(t * 3 * dim, (t + 1) * 3 * dim));
        for (let i = 0; i < dim; i++) {
          v[3 * dim + i] = (v[i] + v[dim + i]) * 0.5;
          v[4 * dim + i] = (v[dim + i] + v[2 * dim + i]) * 0.5;
          v[5 * dim + i] = (v[2 * dim + i] + v[i]) * 0.5;
        }
        ORDER.forEach((k, j) => out.set(v.subarray(k * dim, (k + 1) * dim), (t * 12 + j) * dim));
      }
      return out;
    };
    P = split(P, 3); N = split(N, 3); U = split(U, 2);
  }
  for (let i = 0; i < N.length; i += 3) {
    const l = Math.hypot(N[i], N[i + 1], N[i + 2]) || 1;
    N[i] /= l; N[i + 1] /= l; N[i + 2] /= l;
  }
  return [P, N, U];
}

async function loadHead(base) {
  const gltf = await new GLTFLoader().loadAsync(base + "head.gltf");
  let mesh = null;
  gltf.scene.updateMatrixWorld(true);
  gltf.scene.traverse((o) => { if (o.isMesh && !mesh) mesh = o; });
  const geo = mesh.geometry.clone().applyMatrix4(mesh.matrixWorld).toNonIndexed();
  geo.computeBoundingBox();
  const bb = geo.boundingBox, c = bb.getCenter(new THREE.Vector3()), s = 2.2 / (bb.max.y - bb.min.y);
  geo.translate(-c.x, -c.y, -c.z).scale(s, s, s);
  const [P, N, U] = subdivide(geo.attributes.position.array, geo.attributes.normal.array,
                              geo.attributes.uv.array, 2);
  const cang = new Float32Array(P.length / 3);
  for (let t = 0; t < cang.length; t += 3) {      // each triangle's own angle round the head
    const x = (P[t * 3] + P[t * 3 + 3] + P[t * 3 + 6]) / 3, z = (P[t * 3 + 2] + P[t * 3 + 5] + P[t * 3 + 8]) / 3;
    cang[t] = cang[t + 1] = cang[t + 2] = Math.atan2(x, z);
  }
  const out = new THREE.BufferGeometry();
  out.setAttribute("position", new THREE.BufferAttribute(P, 3));
  out.setAttribute("normal", new THREE.BufferAttribute(N, 3));
  out.setAttribute("uv", new THREE.BufferAttribute(U, 2));
  out.setAttribute("cang", new THREE.BufferAttribute(cang, 1));
  out.boundingSphere = new THREE.Sphere(new THREE.Vector3(), 2.5);
  return out;
}

const TILT = 0.92;                              // the record plane, tipped toward the camera (the shader uses it too)

function canvasTex(w, h, draw) {
  const c = document.createElement("canvas");
  c.width = w; c.height = h;
  draw(c.getContext("2d"), w, h);
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  t.anisotropy = 4;
  return t;
}

// a silver direct-drive deck in the record's plane (y = up out of the record): plinth, strobe-dotted platter,
// slipmat, pitch fader, start/stop, and a tonearm that plays the progress inward
async function turntable() {
  try { await Promise.race([Promise.all(["Anton", "Permanent Marker", "Special Elite"].map((f) => document.fonts.load(`40px "${f}"`))),
                            new Promise((r) => setTimeout(r, 2500))]); } catch (_) {}
  const group = new THREE.Group();
  const deck = new THREE.Group();
  deck.rotation.x = TILT;
  group.add(deck);
  const silver = new THREE.MeshStandardMaterial({ color: 0xa29f97, metalness: 0.8, roughness: 0.32 });
  const dark = new THREE.MeshStandardMaterial({ color: 0x151413, metalness: 0.3, roughness: 0.55 });
  const felt = new THREE.MeshStandardMaterial({ color: 0x0c0b0a, roughness: 1 });
  const add = (geo, mat, x, y, z, parent = deck) => { const m = new THREE.Mesh(geo, mat); m.position.set(x, y, z); parent.add(m); return m; };

  add(new THREE.BoxGeometry(3.7, 0.24, 2.9), silver, 0.33, -0.27, 0.05);
  add(new THREE.BoxGeometry(3.66, 0.012, 2.86), new THREE.MeshStandardMaterial({ color: 0x5c5954, metalness: 0.7, roughness: 0.45 }),
      0.33, -0.146, 0.05);                                                             // the top plate
  const strobe = canvasTex(2048, 64, (g, w, h) => {
    g.fillStyle = "#b9b6ae"; g.fillRect(0, 0, w, h);
    g.fillStyle = "#2a2826";
    [180, 183, 186, 190].forEach((n, row) => {
      for (let i = 0; i < n; i++) g.fillRect((i / n) * w, 6 + row * 14, w / n * 0.5, 8);
    });
  });
  add(new THREE.CylinderGeometry(1.3, 1.3, 0.1, 128, 1, false),
      [new THREE.MeshStandardMaterial({ map: strobe, metalness: 0.75, roughness: 0.3 }), dark, dark], 0, -0.09, 0);
  add(new THREE.CylinderGeometry(1.26, 1.26, 0.012, 96), felt, 0, -0.034, 0);
  add(new THREE.CylinderGeometry(0.025, 0.025, 0.12, 16), silver, 0, 0.03, 0);         // spindle
  add(new THREE.BoxGeometry(0.34, 0.05, 0.24), silver, -1.22, -0.13, 1.22);            // start / stop
  add(new THREE.BoxGeometry(0.1, 0.01, 1.0), dark, 1.88, -0.138, 0.78);                // pitch slot
  add(new THREE.BoxGeometry(0.2, 0.07, 0.12), silver, 1.88, -0.11, 0.62);              // pitch knob
  add(new THREE.CylinderGeometry(0.22, 0.24, 0.12, 48), silver, 1.6, -0.08, -0.95);    // arm base

  // the record label, a separate disc so it stays sharp; it turns with the record
  const labelSpin = new THREE.Group();
  labelSpin.position.y = 0.04;
  deck.add(labelSpin);
  const labelMat = new THREE.MeshStandardMaterial({ roughness: 0.8, transparent: true, opacity: 0,
    map: canvasTex(512, 512, (g, w) => {
      const c = w / 2;
      g.fillStyle = "#c0321d"; g.beginPath(); g.arc(c, c, c, 0, 7); g.fill();
      g.strokeStyle = "#ebe4d2"; g.lineWidth = 6; g.beginPath(); g.arc(c, c, c - 16, 0, 7); g.stroke();
      g.textAlign = "center"; g.fillStyle = "#ebe4d2";
      g.font = '96px "Anton", Impact, sans-serif'; g.fillText("NU//METAL", c, 168);
      g.fillStyle = "#0f0e0c"; g.font = '50px "Permanent Marker", cursive'; g.fillText("Skratchmaster", c, 222);
      g.fillStyle = "#ebe4d2"; g.font = '26px "Special Elite", monospace';
      g.fillText("PRO · SIDE A · 33⅓", c, 352);
      g.fillText("FM-1_926", c, 390);
      g.fillStyle = "#0f0e0c"; g.beginPath(); g.arc(c, c, 16, 0, 7); g.fill();
    }) });
  const label = add(new THREE.CircleGeometry(0.42, 96), labelMat, 0, 0, 0, labelSpin);
  label.rotation.x = -Math.PI / 2;

  // tonearm: yaw on the base, lift at the pivot; built along +x from the pivot
  const PIV = new THREE.Vector2(1.6, -0.95), L = 1.75;
  const yaw = new THREE.Group();
  yaw.position.set(PIV.x, 0.17, PIV.y);
  deck.add(yaw);
  const lift = new THREE.Group();
  yaw.add(lift);
  const tube = add(new THREE.CylinderGeometry(0.028, 0.028, L, 16), silver, L / 2, 0, 0, lift);
  tube.rotation.z = Math.PI / 2;
  const cw = add(new THREE.CylinderGeometry(0.1, 0.1, 0.26, 32), dark, -0.36, 0, 0, lift);
  cw.rotation.z = Math.PI / 2;
  add(new THREE.SphereGeometry(0.07, 24, 12), silver, 0, 0, 0, lift);
  add(new THREE.BoxGeometry(0.26, 0.035, 0.13), dark, L + 0.06, -0.04, 0, lift);       // headshell
  add(new THREE.CylinderGeometry(0.006, 0.002, 0.11, 8), silver, L + 0.08, -0.1, 0, lift);   // stylus

  const D = PIV.length(), toCenter = Math.atan2(-PIV.y, -PIV.x);
  const setArm = (r, up) => {                      // the stylus over radius r of the record, lifted by `up` (0..1)
    const c = Math.max(-1, Math.min(1, (D * D + L * L - r * r) / (2 * D * L)));
    yaw.rotation.y = -(toCenter - Math.acos(c));
    lift.rotation.z = up * 0.16;
  };
  return { group, labelSpin, labelMat, setArm };
}

async function loadTrack(ctx, url) {
  const buf = await ctx.decodeAudioData(await (await fetch(url)).arrayBuffer());
  const a = buf.getChannelData(0), b = buf.numberOfChannels > 1 ? buf.getChannelData(1) : a;
  const mono = new Float32Array(a.length);
  for (let i = 0; i < a.length; i++) mono[i] = (a[i] + b[i]) * 0.5;
  const hop = Math.round(buf.sampleRate / ENV_HZ), env = new Float32Array(Math.floor(mono.length / hop));
  let peak = 1e-6;
  for (let k = 0; k < env.length; k++) {
    let s = 0;
    for (let i = 0; i < hop; i++) s += mono[k * hop + i] ** 2;
    env[k] = Math.sqrt(s / hop);
    peak = Math.max(peak, env[k]);
  }
  for (let k = 0; k < env.length; k++) env[k] /= peak;
  return { buf, mono, env, rate: buf.sampleRate, dur: buf.duration };
}

export async function startDeck(canvas, { base = "", track }) {
  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true, powerPreference: "high-performance" });
  renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
  const scene = new THREE.Scene();
  const camera = new THREE.PerspectiveCamera(32, 1, 0.1, 50);
  camera.position.set(0, 0.35, 5.6);
  camera.lookAt(0, 0, 0);

  const ctx = new (window.AudioContext || window.webkitAudioContext)();
  const [geo, tex, tr] = await Promise.all([
    loadHead(base),
    new THREE.TextureLoader().loadAsync(base + "head.jpg"),
    loadTrack(ctx, track),
  ]);
  tex.colorSpace = THREE.NoColorSpace;

  const waveData = new Uint8Array(W * 2);
  const waveTex = new THREE.DataTexture(waveData, W, 2, THREE.RedFormat, THREE.UnsignedByteType);
  waveTex.magFilter = waveTex.minFilter = THREE.LinearFilter;
  waveTex.wrapS = THREE.RepeatWrapping;
  const U = {
    uTime: { value: 0 }, uMorph: { value: 0 }, uAmp: { value: 0 }, uKick: { value: 0 }, uSpin: { value: 0 },
    uErr: { value: 0 }, uDots: { value: 4 * renderer.getPixelRatio() }, uWave: { value: waveTex }, uTex: { value: tex },
  };
  U.uTilt = { value: TILT };
  const mat = new THREE.ShaderMaterial({ vertexShader: VERT, fragmentShader: FRAG, uniforms: U, side: THREE.DoubleSide });
  const mesh = new THREE.Mesh(geo, mat);
  const root = new THREE.Group();
  root.add(mesh);
  scene.add(root);
  scene.add(new THREE.HemisphereLight(0xf2ead8, 0x1a1410, 1.1));
  const key = new THREE.DirectionalLight(0xffffff, 2.4);
  key.position.set(-3, 5, 4);
  scene.add(key);
  const back = new THREE.DirectionalLight(0xc0321d, 1.6);
  back.position.set(3, 2, -4);
  scene.add(back);
  const tt = await turntable();
  root.add(tt.group);

  const fit = () => {
    const w = canvas.clientWidth, h = canvas.clientHeight;
    renderer.setSize(w, h, false);
    camera.aspect = w / h;
    camera.position.z = w / h < 1 ? 5.6 / Math.max(0.62, w / h) : 5.6;
    const wide = w / h > 1.15;
    root.position.set(wide ? 0.82 : 0.05, wide ? 0.3 : 0.5, 0);   // wide: beside the title
    root.scale.setScalar(wide ? 0.62 : 0.58);
    camera.updateProjectionMatrix();
  };
  new ResizeObserver(fit).observe(canvas);
  fit();

  let target = 0, morph = 0, err = 0, amp = 0, slow = 0, spin = 0, dir = 1, fast = 0, up = 1, lifted = false, armR = 1.62;
  let src = null, gain = null, t0 = 0, wallStart = performance.now() / 1000, offset = 0;
  const calm = matchMedia("(prefers-reduced-motion: reduce)").matches;

  const playhead = () => {
    if (src) return (ctx.currentTime - t0) % tr.dur;
    return (offset + performance.now() / 1000 - wallStart) % tr.dur;
  };

  const deck = {
    get soundOn() { return !!src; },
    async sound(on) {
      if (on && !src) {
        await ctx.resume();
        const at = playhead();
        gain = ctx.createGain();
        gain.gain.value = 0.9;
        gain.connect(ctx.destination);
        src = ctx.createBufferSource();
        src.buffer = tr.buf;
        src.loop = true;
        src.connect(gain);
        src.start(0, at);
        t0 = ctx.currentTime - at;
      } else if (!on && src) {
        offset = playhead(); wallStart = performance.now() / 1000;
        src.stop(); src.disconnect(); src = null;
      }
    },
    progress(p) { target = Math.max(0, Math.min(1, p)); err = 0; lifted = false; },
    done() { target = 1; fast = 1; lifted = true; },
    error() { target = 0; err = 1; lifted = false; },
    reset() { target = 0; lifted = false; },
    snap() { morph = target; up = 0; armR = 1.14 - 0.7 * Math.max(0, Math.min(1, (target - 0.3) / 0.67)); },
  };

  let last = performance.now();
  renderer.setAnimationLoop((now) => {
    const dt = Math.min(0.05, (now - last) / 1000);
    last = now;
    const t = playhead();
    const s0 = Math.floor(t * tr.rate);
    for (let i = 0; i < W; i++) {
      const x = tr.mono[(s0 + i * 2) % tr.mono.length];
      waveData[i] = Math.max(0, Math.min(255, 128 + x * 300));
    }
    const e0 = Math.floor(t * ENV_HZ);
    for (let i = 0; i < W; i++) {
      const k = (e0 - W + i + tr.env.length) % tr.env.length;
      waveData[W + i] = tr.env[k] * 255;
    }
    waveTex.needsUpdate = true;

    const e = tr.env[e0 % tr.env.length];
    amp += (e - amp) * (e > amp ? 0.5 : 0.08);
    slow += (e - slow) * 0.03;
    const kick = Math.max(0, amp - slow * 1.15) * (calm ? 0.3 : 1);
    if (kick > 0.25 && Math.random() < 0.2) dir = -dir;     // the hand on the record: back and forth on the cuts
    morph += (target - morph) * Math.min(1, dt * 2.2);
    err *= Math.pow(0.4, dt);
    // the first 30 % turns the head into a record on a rising turntable; the rest, the needle plays it inward
    const sm = (a, b, x) => { const t = Math.max(0, Math.min(1, (x - a) / (b - a))); return t * t * (3 - 2 * t); };
    const rec = sm(0.02, 0.3, morph), rise = sm(0, 0.24, morph), play = sm(0.3, 0.97, morph);
    spin += dt * (rec * 3.49 + (1 - rec) * 0.6 + fast * 8 + kick * 9 * dir * rec);   // 33 1/3 rpm, and the cuts
    fast *= Math.pow(0.6, dt);
    tt.group.position.y = -4.5 * (1 - rise) * (1 - rise);
    tt.group.visible = rise > 0.001;
    tt.labelSpin.rotation.y = -spin;
    tt.labelMat.opacity = sm(0.85, 1, rec);
    up += ((rec > 0.97 && !lifted ? 0 : 1) - up) * Math.min(1, dt * 3);                // drop the needle
    armR += ((rec > 0.97 ? 1.14 - 0.7 * play : 1.62) - armR) * Math.min(1, dt * 2.5);   // swing over, then inward
    tt.setArm(armR, up);
    U.uTime.value = now / 1000;
    U.uMorph.value = rec;
    U.uAmp.value = amp;
    U.uKick.value = kick;
    U.uSpin.value = spin;
    U.uErr.value = err;
    mesh.rotation.y = Math.sin(now / 2600) * 0.5 * (1 - rec);
    mesh.scale.setScalar(1 + 0.3 * (1 - rec));                                      // a big head, a record-sized record
    mesh.rotation.z = (calm ? 0 : kick * 0.04) * dir * (1 - rec);
    renderer.render(scene, camera);
  });
  return deck;
}
