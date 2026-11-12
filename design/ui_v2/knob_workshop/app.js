// Knob Workshop: live preview of a knob recipe.
// A knob is two stacked lathed parts, Base (against the panel) and Top (carries the pointer), each with its own
// shape and grip. The geometry mirrors blender/knob_studio_geometry.py (lathe, pointer_mesh) and the lights mirror
// blender/knob_studio.py; recipe_to_studio.py turns a recipe into a Cycles config with the same profiles.
import * as THREE from 'three';
import { RectAreaLightUniformsLib } from 'three/addons/lights/RectAreaLightUniformsLib.js';

RectAreaLightUniformsLib.init();

const ZOOM = 4;                         // stage px per plugin px
const LIGHT_GAIN = 0.3;                 // Blender area-light watts -> three.js radiance per m^2, measured against the Cycles strip
const LOOK = 'contrast(1.1)';           // stands in for Blender's 'AgX - Medium High Contrast' look (same measurement)
const MASK = 118;                       // silhouette masks (outline match) are compared at this many px
const NS = 'http://www.w3.org/2000/svg';
const $ = (id) => document.getElementById(id);
const clamp = (v, a, b) => Math.min(b, Math.max(a, v));
const rad = THREE.MathUtils.degToRad;
const clone = (o) => JSON.parse(JSON.stringify(o));

let S = null;              // the recipe
let O = null;              // the recipe as opened (what "changed" is measured against)
let opened = null;         // JSON of O (Start over)
let history = [], hIndex = -1;
let turn = 30, cmpMode = 'mine', flick = false, tabIndex = 0;
let view = { name: 'plugin', pitch: 0, yaw: 0 };
let hoverPath = null;
const VIEWS = { plugin: { pitch: 0, yaw: 0 }, angle: { pitch: 58, yaw: -28 }, side: { pitch: 90, yaw: 0 } };
const images = {};

const getIn = (obj, path) => path.split('.').reduce((o, k) => (o == null ? o : o[k]), obj);
function setIn(obj, path, v) { const keys = path.split('.'), last = keys.pop(); keys.reduce((o, k) => o[k], obj)[last] = v; }
const get = (path) => getIn(S, path);
const set = (path, v) => setIn(S, path, v);

// ---------- geometry (mirrors knob_studio_geometry.py and recipe_to_studio.py) ----------
// Radius factor of the grip at one point. t = angle, tl = angle incl. flute phase, w = grip strength (0..1),
// zr = height up the wall in part radii (only the diamond knurl uses it).
function radial(g, lobes, t, tl, w, zr) {
  if (!lobes || g.style === 'smooth' || w <= 0) return 1;
  if (g.style === 'scallop') {                     // circular finger scallops cut into a cylinder
    const pitch = 2 * Math.PI / lobes, delta = ((tl + pitch / 2) % pitch) - pitch / 2;
    const dist = 1.4, hole = 0.4 + g.depth, disc = hole * hole - (dist * Math.sin(delta)) ** 2;
    return disc >= 0 ? 1 - w * (1 - Math.min(1, dist * Math.cos(delta) - Math.sqrt(disc))) : 1;
  }
  if (g.style === 'scoop') {                       // concave scoop with a flat land on each ridge
    const u = Math.min(1, Math.abs(((lobes * tl / (2 * Math.PI)) % 1) * 2 - 1) / (1 - (g.land ?? 0.1)));
    return 1 - g.depth * w * (1 - u * u);
  }
  if (g.style === 'diamond') {                     // two crossing 45-degree sets of V grooves
    const u = lobes * t / (2 * Math.PI), v = lobes * zr / (2 * Math.PI), fr = (x) => x - Math.floor(x);
    return 1 - g.depth * w * Math.max(Math.abs(2 * fr(u + v) - 1), Math.abs(2 * fr(u - v) - 1));
  }
  if (g.style === 'ridge') return 1 - g.depth * w * Math.abs(Math.sin(lobes * t / 2));   // straight knurl
  return 1 - g.depth * w * Math.pow((1 - Math.cos(lobes * tl)) / 2, g.sharpness ?? 1);    // soft wave
}

function lathe(prof, R, H, g) {
  const lobes = g.style === 'smooth' ? 0 : g.count | 0, n = Math.max(128, lobes * (lobes > 30 ? 12 : 48));
  const pos = [], idx = [], rings = [], col = [];
  if (lobes && g.style === 'diamond') {            // the diamond pattern needs rings close together up the wall
    const dense = [], gap = 2 * Math.PI * R / lobes / 4;
    for (let i = 0; i + 1 < prof.length; i++) {
      const [ra, za, wa, aa = 1] = prof[i], [rb, zb, wb, ab = 1] = prof[i + 1], k = Math.max(1, Math.floor(Math.abs(zb - za) * H / gap));
      for (let j = 0; j < k; j++) { const f = j / k; dense.push([ra + (rb - ra) * f, za + (zb - za) * f, wa + (wb - wa) * f, aa + (ab - aa) * f]); }
    }
    dense.push(prof[prof.length - 1]); prof = dense;
  }
  for (const [rf, zf, w, ao = 1] of prof) {    // ao = contact shading (1 = none), where one part meets another or the panel
    const start = pos.length / 3;
    if (rf === 0) { pos.push(0, 0, zf * H); col.push(ao, ao, ao); rings.push({ start, single: true }); continue; }
    for (let i = 0; i < n; i++) {
      const t = 2 * Math.PI * i / n, tl = t + (g.phase || 0) * 2 * Math.PI / Math.max(1, lobes);
      const r = R * rf * radial(g, lobes, t, tl, w, zf * H / R);
      pos.push(r * Math.cos(t), r * Math.sin(t), zf * H); col.push(ao, ao, ao);
    }
    rings.push({ start });
  }
  for (let k = 0; k + 1 < rings.length; k++) {
    const a = rings[k], b = rings[k + 1];
    if (a.single) continue;
    for (let i = 0; i < n; i++) {
      const j = (i + 1) % n;
      if (b.single) idx.push(a.start + i, a.start + j, b.start);
      else idx.push(a.start + i, a.start + j, b.start + j, a.start + i, b.start + j, b.start + i);
    }
  }
  if (!rings[0].single) {                          // closed underside (own vertices, so the wall's shading is not smeared)
    const s0 = pos.length / 3, a0 = prof[0][3] ?? 1;
    for (let i = 0; i < n; i++) { pos.push(pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2]); col.push(a0, a0, a0); }
    pos.push(0, 0, prof[0][1] * H); col.push(a0, a0, a0);
    for (let i = 0; i < n; i++) idx.push(s0 + (i + 1) % n, s0 + i, s0 + n);
  }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  geo.setAttribute('color', new THREE.Float32BufferAttribute(col, 3));
  geo.setIndex(idx); geo.computeVertexNormals();
  return geo;
}

// Part outlines as (radius fraction, height fraction, grip strength). Sizes in the recipe are in knob radii.
// taper = how far the wall leans in toward the top (0 = straight). grip.fade = how much the grip fades out up the wall.
const ARC = 16, WALL = 8, r4 = (v) => +v.toFixed(5);
// The wall runs straight into the shoulder through a blend whose curvature starts and ends at zero (a quintic Bezier
// with its first three and last three control points in line). A plain quarter-circle joins the wall with a sudden
// change of curvature, which shows up as a line across the flutes when light rakes over it. The grip strength follows
// the same idea: it eases (no sudden change of slope) up the wall and over the shoulder.
const ease = (f) => f * f * (3 - 2 * f);
const fall = (u) => 1 - u * u * u * (10 - 15 * u + 6 * u * u);
const B5 = [1, 5, 10, 10, 5, 1];
function blend(P0, C, P2, u) {
  const mix = (A, B, t) => [A[0] + (B[0] - A[0]) * t, A[1] + (B[1] - A[1]) * t];
  const Q = [P0, mix(P0, C, 0.5), mix(P0, C, 0.94), mix(P2, C, 0.94), mix(P2, C, 0.5), P2];
  let r = 0, z = 0;
  for (let i = 0; i < 6; i++) { const k = B5[i] * u ** i * (1 - u) ** (5 - i); r += k * Q[i][0]; z += k * Q[i][1]; }
  return [r, z];
}
// Shared by Base and Top: wall from (1, 0) toward the corner (rt, zs), blended into the flat/cap at radius rt - sr.
function wallAndShoulder(rt, zs, sr, sz, wt) {
  const zw = zs - sz, rw = 1 + (rt - 1) * zw / zs, pts = [[1, 0, 1, FOOT[0]]];
  for (let j = 1; j < WALL; j++) { const f = j / WALL; pts.push([r4(1 + (rw - 1) * f), r4(f * zw), r4(1 - (1 - wt) * ease(f)), j === 1 ? FOOT[1] : 1]); }
  for (let i = 0; i <= ARC; i++) { const u = i / ARC, [r, z] = blend([rw, zw], [rt, zs], [rt - sr, zs], u); pts.push([r4(r), r4(z), r4(wt * fall(u))]); }
  return pts;
}
// The 4th value is contact shading for the live view only (Cycles shades those creases itself): the foot of each wall,
// and a soft ring on the Base's flat top around the Top's footprint (foot = the Top's radius and deepest grip cut, in Base radii).
const FOOT = [0.7, 0.92];
function baseProfile(p, foot) {      // wall, rounded top edge, flat top (mostly hidden under the Top part)
  const rt = 1 - clamp(p.taper || 0, 0, 0.8), wt = 1 - clamp(p.grip.fade || 0, 0, 1);
  const br = clamp(p.bevel / p.width, 0.004, rt * 0.95), bz = clamp(p.bevel / p.height, 0.004, 0.95);
  const pts = wallAndShoulder(rt, 1, br, bz, wt);
  if (foot) for (const [r, ao] of [[foot.r * 1.4, 1], [foot.r * 1.2, 0.93], [foot.r * 1.08, 0.8], [foot.r, 0.66], [foot.r * (1 - foot.cut), 0.6]]) {
    if (r < pts[pts.length - 1][0] - 0.004 && r > 0.01) pts.push([r4(r), 1, 0, ao]);
  }
  pts.push([0, 1, 0, foot ? 0.6 : 1]);
  return pts;
}
function topProfile(p) {             // wall, rounded shoulder, domed (or dished) cap
  const c = clamp(p.dome / p.height, -0.5, 0.6), zs = 1 - Math.max(c, 0);
  const rt = 1 - clamp(p.taper || 0, 0, 0.8), wt = 1 - clamp(p.grip.fade || 0, 0, 1);
  const sr = clamp(p.round / p.width, 0.004, rt * 0.95), sz = clamp(p.round / p.height, 0.004, zs * 0.95);
  const pts = wallAndShoulder(rt, zs, sr, sz, wt), rf = rt - sr;
  for (let k = 1; k <= 8; k++) { const r = rf * (1 - k / 8); pts.push([r4(r), r4(zs + c * (1 - (r / rf) ** 2)), 0]); }
  return pts;
}

function surfaceZ(prof, R, H, r) {
  const pts = prof.filter((p) => p[1] > 0.3).map((p) => [p[0] * R, p[1] * H]).sort((a, b) => a[0] - b[0]);
  if (!pts.length) return H;
  for (let i = 0; i + 1 < pts.length; i++) {
    const [ra, za] = pts[i], [rb, zb] = pts[i + 1];
    if (ra <= r && r <= rb) return rb === ra ? za : za + (zb - za) * (r - ra) / (rb - ra);
  }
  return r > pts[pts.length - 1][0] ? pts[pts.length - 1][1] : pts[0][1];
}

function pointerGeo(prof, R, H, p) {
  const steps = 96, lift = 0.012, sink = 0.02, capRound = 0.2, hw = p.width * R / 2;
  const r0 = Math.min(p.start, p.end - 0.02), r1 = p.end, rows = [], tri = [];
  for (let s = 0; s <= steps; s++) {
    const x = R * (r0 + (r1 - r0) * s / steps), ed = Math.min(x - R * r0, R * r1 - x), cr = hw * capRound;
    const wh = ed >= cr ? hw : hw - cr + Math.sqrt(Math.max(1e-6 * R * R, cr * cr - (cr - ed) ** 2));
    const z = surfaceZ(prof, R, H, x);
    rows.push({ tl: [x, -wh, z + lift * R], tr: [x, wh, z + lift * R], bl: [x, -wh, z - sink * R], br: [x, wh, z - sink * R] });
  }
  const quad = (a, b, c, d) => tri.push(...a, ...b, ...c, ...a, ...c, ...d);
  for (let s = 0; s < steps; s++) {
    const a = rows[s], b = rows[s + 1];
    quad(a.tl, b.tl, b.tr, a.tr); quad(a.bl, b.bl, b.tl, a.tl); quad(a.tr, b.tr, b.br, a.br);
  }
  quad(rows[0].tl, rows[0].tr, rows[0].br, rows[0].bl);
  quad(rows[steps].bl, rows[steps].br, rows[steps].tr, rows[steps].tl);
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.Float32BufferAttribute(tri, 3));
  geo.computeVertexNormals();
  return geo;
}

// Handy absolute sizes of a recipe (metres), used to place the drag handles.
function dims(rec) {
  const R = rec.radius_m * rec.view.size, B = rec.parts.base, T = rec.parts.top;
  const z0 = B.on ? R * B.height : 0, Rt = R * T.width, Ht = R * T.height, Rb = R * B.width, Hb = R * B.height;
  return { R, B, T, P: rec.pointer, z0, Rt, Ht, Rb, Hb,
    wallT: Math.max(Ht - T.round * R - Math.max(T.dome, 0) * R, Ht * 0.2), wallB: Math.max(Hb - B.bevel * R, Hb * 0.2) };
}

// ---------- scene ----------
const renderer = new THREE.WebGLRenderer({ canvas: $('gl'), antialias: true, alpha: true, preserveDrawingBuffer: true });
const DPR = Math.min(window.devicePixelRatio || 1, 2);
renderer.setPixelRatio(DPR);
renderer.toneMapping = THREE.AgXToneMapping;
renderer.setClearColor(0x000000, 0);
const scene = new THREE.Scene();
const camera = new THREE.OrthographicCamera(-1, 1, 1, -1, 0.01, 3);

const pmrem = new THREE.PMREMGenerator(renderer);
const room = new THREE.Scene(); room.background = new THREE.Color().setRGB(0.7, 0.65, 0.6);
scene.environment = pmrem.fromScene(room).texture;

const root = new THREE.Group(), tiltG = new THREE.Group(), spinG = new THREE.Group();
scene.add(root); root.add(tiltG); tiltG.add(spinG);
const baseMat = new THREE.MeshPhysicalMaterial({ metalness: 0, vertexColors: true }), topMat = new THREE.MeshPhysicalMaterial({ metalness: 0, vertexColors: true });
const ptrMat = new THREE.MeshPhysicalMaterial({ metalness: 0, specularIntensity: 0.25, side: THREE.DoubleSide });
const baseMesh = new THREE.Mesh(new THREE.BufferGeometry(), baseMat);
const topMesh = new THREE.Mesh(new THREE.BufferGeometry(), topMat);
const ptr = new THREE.Mesh(new THREE.BufferGeometry(), ptrMat);
spinG.add(baseMesh, topMesh, ptr);
const ground = new THREE.Mesh(new THREE.CircleGeometry(0.3, 64), new THREE.MeshStandardMaterial({ color: new THREE.Color().setRGB(0.03, 0.028, 0.026), roughness: 0.9 }));
scene.add(ground);
const lightColor = new THREE.Color().setRGB(1, 0.96, 0.9);
const key = new THREE.RectAreaLight(lightColor, 1, 1, 1), rim = new THREE.RectAreaLight(lightColor, 1, 1, 1);
scene.add(key, rim);

const strip = (v) => JSON.stringify(v, (k, val) => (k === 'saved' ? undefined : val));
let geomKey = '', knobH = 0.03, queued = false;
function requestRender() { if (!queued) { queued = true; requestAnimationFrame(render); } }

function build(rec) {
  const R = rec.radius_m * rec.view.size, B = rec.parts.base, T = rec.parts.top;
  const gk = strip([rec.parts, rec.pointer.start, rec.pointer.end, rec.pointer.width, R]);
  if (gk === geomKey) return;
  geomKey = gk;
  for (const m of [baseMesh, topMesh, ptr]) m.geometry.dispose();
  const z0 = B.on ? R * B.height : 0, tp = topProfile(T);
  baseMesh.visible = !!B.on;
  const cut = T.grip.style === 'smooth' ? 0 : Math.min(T.grip.depth || 0, 0.3);
  baseMesh.geometry = B.on ? lathe(baseProfile(B, { r: T.width / B.width, cut }), R * B.width, R * B.height, B.grip) : new THREE.BufferGeometry();
  topMesh.geometry = lathe(tp, R * T.width, R * T.height, T.grip);
  ptr.geometry = pointerGeo(tp, R * T.width, R * T.height, rec.pointer);
  topMesh.position.z = ptr.position.z = z0;
  knobH = z0 + R * T.height;
}
function dress(rec, glow) {
  for (const [m, part] of [[baseMat, 'base'], [topMat, 'top']]) {
    const own = part === 'base' && rec.material.base_color;   // optional: the Base in its own colour (VOICE: cream dome on a black skirt)
    m.color.setRGB(...(own || rec.material.color)); m.roughness = own ? rec.material.base_roughness ?? rec.material.roughness : rec.material.roughness;
    m.clearcoat = own ? 0 : rec.material.coat; m.clearcoatRoughness = rec.material.coat_roughness;
    m.emissive.setRGB(...(glow === part ? [0.05, 0.16, 0.13] : [0, 0, 0]));
  }
  ptrMat.color.setRGB(...rec.pointer.color); ptrMat.roughness = rec.pointer.roughness;
  ptrMat.emissive.setRGB(...(glow === 'pointer' ? [0.1, 0.3, 0.25] : [0, 0, 0]));
  const L = rec.light, target = new THREE.Vector3(0, 0, rec.radius_m * 0.65);
  for (const [light, power, pos, sz] of [[key, L.key_power, L.key_pos, L.key_size], [rim, L.rim_power, L.rim_pos, L.rim_size]]) {
    light.position.set(...pos); light.width = sz[0]; light.height = sz[1];
    light.intensity = power * LIGHT_GAIN / (sz[0] * sz[1]);
    light.lookAt(target); light.visible = power > 0;
  }
  renderer.toneMappingExposure = Math.pow(2, L.exposure);
}
// Render a recipe exactly as the plugin shows it, into the WebGL canvas.
function shoot(rec, turnDeg, glow) {
  const px = rec.frame_px, half = px / 2 * 0.001, size = px * ZOOM;   // 1 plugin px = 1 mm in the Blender studio
  if (renderer.domElement.width !== size * DPR) renderer.setSize(size, size, false);
  build(rec); dress(rec, glow);
  spinG.rotation.z = rad(90 - turnDeg);
  root.position.set(0, rec.view.offset_y || 0, 0);
  // Never look exactly square-on at a flat face: three.js's area-light maths breaks down when the surface normal and the
  // view direction are parallel, and the face loses its direct light (it went dark at Tilt 0). 0.3 degrees is invisible.
  tiltG.rotation.x = -rad(Math.max(rec.view.tilt, 0.3));
  ground.visible = false; scene.environmentIntensity = rec.light.ambient;
  camera.left = -half; camera.right = half; camera.top = half; camera.bottom = -half;
  camera.position.set(0, 0, 0.65); camera.up.set(0, 1, 0); camera.lookAt(0, 0, 0); camera.updateProjectionMatrix();
  renderer.render(scene, camera);
}
// A picture of a recipe on the panel, for thumbnails (variations, shapes, versions).
function picture(rec, sizePx, turnDeg = 30) {
  shoot(rec, turnDeg);
  const c = document.createElement('canvas'); c.width = c.height = sizePx;
  const ctx = c.getContext('2d'); ctx.imageSmoothingQuality = 'high';
  ctx.drawImage($('bg'), 0, 0, sizePx, sizePx);
  ctx.filter = LOOK; ctx.drawImage(renderer.domElement, 0, 0, sizePx, sizePx); ctx.filter = 'none';
  requestRender();
  return c;
}

function render() {
  queued = false;
  if (!S) return;
  const px = S.frame_px, half = px / 2 * 0.001;
  const glow = hoverPath ? (hoverPath.startsWith('parts.top') ? 'top' : hoverPath.startsWith('parts.base') ? 'base' : hoverPath.startsWith('pointer') ? 'pointer' : null) : null;

  // pass 1: exactly as the plugin shows it (also feeds the actual-size thumbnail and the outline comparison)
  shoot(S, turn, glow);
  const a = $('actual'), ctx = a.getContext('2d');
  if (a.width !== px * DPR) { a.width = a.height = px * DPR; }
  ctx.clearRect(0, 0, a.width, a.height);
  ctx.imageSmoothingQuality = 'high';
  ctx.drawImage($('bg'), 0, 0, a.width, a.height);
  ctx.filter = LOOK; ctx.drawImage(renderer.domElement, 0, 0, a.width, a.height); ctx.filter = 'none';
  const orbit = view.name !== 'plugin';
  if (!orbit && (cmpMode === 'outline' || cmpMode === 'diff')) drawDiff();

  // pass 2: looking around the knob (flat on a dark table, with extra fill light so every side is readable)
  $('bg').style.display = orbit ? 'none' : 'block';
  if (orbit) {
    root.position.set(0, 0, 0); tiltG.rotation.x = -rad(0.3);
    ground.visible = true; scene.environmentIntensity = S.light.ambient + 0.5;
    const p = rad(view.pitch), y = rad(view.yaw), tz = knobH / 2, D = 0.65, h2 = half * 1.15;
    camera.left = -h2; camera.right = h2; camera.top = h2; camera.bottom = -h2;
    camera.position.set(D * Math.sin(p) * Math.sin(y), -D * Math.sin(p) * Math.cos(y), tz + D * Math.cos(p));
    camera.up.set(-Math.cos(p) * Math.sin(y), Math.cos(p) * Math.cos(y), Math.sin(p));
    camera.lookAt(0, 0, tz); camera.updateProjectionMatrix();
    renderer.render(scene, camera);
  }
  stageHandles.update();
}

// ---------- 2D layers: panel, target ----------
function loadImage(rel) {
  if (!images[rel]) images[rel] = new Promise((res) => { const im = new Image(); im.onload = () => res(im); im.onerror = () => res(null); im.src = '/files/' + rel; });
  return images[rel];
}
function sizeCanvas(c) {
  const size = S.frame_px * ZOOM * DPR;
  if (c.width !== size) c.width = c.height = size;
  return c.getContext('2d');
}
async function drawPanel(ctx, shadow) {
  const size = ctx.canvas.width, px = S.frame_px, P = S.panel;
  ctx.fillStyle = '#241a12'; ctx.fillRect(0, 0, size, size);
  const im = P && await loadImage(P.image);
  if (im) ctx.drawImage(im, (P.cx - px / 2) * P.scale, (P.cy - px / 2) * P.scale, px * P.scale, px * P.scale, 0, 0, size, size);
  if (shadow) {
    const wide = Math.max(S.parts.top.width, S.parts.base.on ? S.parts.base.width : 0);
    const k = size / px, r = S.radius_m * 1000 * S.view.size * wide * k, cx = size / 2, cy = size / 2 - (S.view.offset_y || 0) * 1000 * k + r * 0.07;
    const g = ctx.createRadialGradient(cx, cy, r * 0.8, cx, cy, r * 1.3);
    g.addColorStop(0, 'rgba(0,0,0,0.55)'); g.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = g; ctx.fillRect(0, 0, size, size);
  }
}
async function drawBackground() {       // drawn off-screen first, so a thumbnail taken meanwhile never sees a half-drawn panel
  const tmp = document.createElement('canvas'); tmp.width = tmp.height = S.frame_px * ZOOM * DPR;
  await drawPanel(tmp.getContext('2d'), true);
  sizeCanvas($('bg')).drawImage(tmp, 0, 0); requestRender();
}

const currentRef = () => S.references && S.references[$('refSel').value | 0];
const refAngle = () => { const r = currentRef(); return r && r.angle != null ? r.angle : 30; };
async function drawRefInto(ctx, ref, atTurn) {
  const size = ctx.canvas.width, px = S.frame_px;
  ctx.clearRect(0, 0, size, size);
  if (!ref) return;
  const im = await loadImage(ref.strip || ref.image);
  if (ref.strip) {
    await drawPanel(ctx, true);
    const k = Math.round((atTurn + 135) / 270 * (ref.frames - 1));
    if (im) ctx.drawImage(im, 0, k * ref.frame_px, ref.frame_px, ref.frame_px, 0, 0, size, size);
  } else if (im) {
    ctx.drawImage(im, (ref.cx - px / 2) * ref.scale, (ref.cy - px / 2) * ref.scale, px * ref.scale, px * ref.scale, 0, 0, size, size);
  }
}
async function drawReference() {
  await drawRefInto(sizeCanvas($('refOver')), currentRef(), turn);
  const t = sizeCanvas($('target')); t.clearRect(0, 0, t.canvas.width, t.canvas.height); t.drawImage($('refOver'), 0, 0);
}

// ---------- outline match: silhouettes of yours and the target ----------
const maskCanvas = document.createElement('canvas'); maskCanvas.width = maskCanvas.height = MASK;
const maskCtx = maskCanvas.getContext('2d', { willReadFrequently: true });
function glMask() {                      // silhouette of whatever shoot() rendered last
  maskCtx.clearRect(0, 0, MASK, MASK); maskCtx.drawImage(renderer.domElement, 0, 0, MASK, MASK);
  const d = maskCtx.getImageData(0, 0, MASK, MASK).data, m = new Uint8Array(MASK * MASK);
  for (let i = 0; i < m.length; i++) m[i] = d[i * 4 + 3] > 110 ? 1 : 0;
  return m;
}
const maskOf = (rec, ang) => { shoot(rec, ang); return glMask(); };
const targetMasks = {};
async function targetMask() {            // the target knob's silhouette, found in its picture
  const ref = currentRef(), id = $('refSel').value + '@' + (ref && ref.strip ? turn : 0);
  if (!ref) return null;
  if (targetMasks[id]) return targetMasks[id];
  const c = document.createElement('canvas'); c.width = c.height = S.frame_px * ZOOM * DPR;
  const big = c.getContext('2d');
  if (ref.strip) {                       // a rendered strip has its own transparency
    const im = await loadImage(ref.strip), k = Math.round((refAngle() + 135) / 270 * (ref.frames - 1));
    if (im) big.drawImage(im, 0, k * ref.frame_px, ref.frame_px, ref.frame_px, 0, 0, c.width, c.height);
  } else await drawRefInto(big, ref, 0);
  maskCtx.clearRect(0, 0, MASK, MASK); maskCtx.drawImage(c, 0, 0, MASK, MASK);
  const d = maskCtx.getImageData(0, 0, MASK, MASK).data, n = MASK * MASK, m = new Uint8Array(n);
  if (ref.strip) { for (let i = 0; i < n; i++) m[i] = d[i * 4 + 3] > 110 ? 1 : 0; return (targetMasks[id] = m); }
  // photo: knob = bright warm pixels joined to the centre; the panel is dark. Threshold by Otsu on the brightest channel.
  const v = new Uint8Array(n), hist = new Float64Array(256);
  for (let i = 0; i < n; i++) { v[i] = Math.max(d[i * 4], d[i * 4 + 1], d[i * 4 + 2]); hist[v[i]]++; }
  let sum = 0; for (let i = 0; i < 256; i++) sum += i * hist[i];
  let wB = 0, sB = 0, best = 0, thr = 90;
  for (let t = 0; t < 256; t++) {
    wB += hist[t]; if (!wB) continue; const wF = n - wB; if (!wF) break;
    sB += t * hist[t]; const mB = sB / wB, mF = (sum - sB) / wF, between = wB * wF * (mB - mF) ** 2;
    if (between > best) { best = between; thr = t; }
  }
  thr = Math.min(thr, ref.threshold ?? 96);
  const bright = (i) => v[i] > thr;
  const flood = (seeds, ok) => {         // 4-connected flood fill
    const seen = new Uint8Array(n), stack = seeds.filter((i) => ok(i));
    for (const i of stack) seen[i] = 1;
    while (stack.length) {
      const i = stack.pop(), x = i % MASK, y = (i / MASK) | 0;
      for (const j of [x > 0 ? i - 1 : -1, x < MASK - 1 ? i + 1 : -1, y > 0 ? i - MASK : -1, y < MASK - 1 ? i + MASK : -1]) {
        if (j >= 0 && !seen[j] && ok(j)) { seen[j] = 1; stack.push(j); }
      }
    }
    return seen;
  };
  const border = [];
  for (let i = 0; i < MASK; i++) border.push(i, n - 1 - i, i * MASK, i * MASK + MASK - 1);
  const outside = flood(border, (i) => !bright(i));                // dark panel reachable from the frame edge
  const centre = (MASK >> 1) * MASK + (MASK >> 1);
  const knob = flood([centre, centre + 6, centre - 6, centre + 6 * MASK, centre - 6 * MASK], (i) => !outside[i]);   // pointer hole stays inside
  for (let i = 0; i < n; i++) m[i] = knob[i];
  return (targetMasks[id] = m);
}
function iou(a, b) { let inter = 0, uni = 0; for (let i = 0; i < a.length; i++) { inter += a[i] & b[i]; uni += a[i] | b[i]; } return uni ? inter / uni : 0; }

async function drawDiff() {              // called right after pass 1, while the WebGL canvas holds the plugin view
  const mine = glMask(), tm = await targetMask(), c = $('diffOver');
  if (!tm) return;
  if (c.width !== MASK * 4) c.width = c.height = MASK * 4;
  const ctx = c.getContext('2d'), k = 4; ctx.clearRect(0, 0, c.width, c.height);
  if (cmpMode === 'diff') {
    for (let y = 0; y < MASK; y++) for (let x = 0; x < MASK; x++) {
      const i = y * MASK + x; if (mine[i] === tm[i]) continue;
      ctx.fillStyle = mine[i] ? 'rgba(224,115,107,0.75)' : 'rgba(127,183,164,0.8)';   // yours sticks out / target sticks out
      ctx.fillRect(x * k, y * k, k, k);
    }
  } else {
    ctx.fillStyle = '#9ad1b9';
    for (let y = 1; y < MASK - 1; y++) for (let x = 1; x < MASK - 1; x++) {
      const i = y * MASK + x;
      if (tm[i] && !(tm[i - 1] && tm[i + 1] && tm[i - MASK] && tm[i + MASK])) ctx.fillRect(x * k, y * k, k, k);
    }
  }
}
let scoreTimer = 0;
function scheduleScore() { clearTimeout(scoreTimer); scoreTimer = setTimeout(updateScore, 250); }
async function updateScore() {
  const tm = await targetMask(); if (!tm || !S) return;
  const s = iou(maskOf(S, refAngle()), tm); requestRender();
  $('score').textContent = `Outline match ${Math.round(s * 100)}%`;
}

// ---------- views ----------
function applyView() {
  const orbit = view.name !== 'plugin';
  for (const b of $('viewSeg').children) b.classList.toggle('on', b.dataset.v === view.name);
  for (const b of $('cmpSeg').children) b.classList.toggle('on', b.dataset.m === cmpMode);
  const mode = orbit ? 'mine' : flick ? 'reference' : cmpMode, o = $('refOver'), v = $('cmp').value | 0;
  const pic = mode === 'reference' || mode === 'fade' || mode === 'wipe';
  o.style.display = pic ? 'block' : 'none';
  o.style.opacity = mode === 'fade' ? v / 100 : 1;
  o.style.clipPath = mode === 'wipe' ? `inset(0 0 0 ${v}%)` : 'none';
  $('diffOver').style.display = mode === 'outline' || mode === 'diff' ? 'block' : 'none';
  $('handles').style.display = pic ? 'none' : '';
  $('cmpRow').style.visibility = !orbit && (cmpMode === 'fade' || cmpMode === 'wipe') ? 'visible' : 'hidden';
  $('cmpWord').textContent = cmpMode === 'fade' ? 'Fade amount' : 'Wipe position';
  $('stageTag').textContent = orbit ? { angle: 'Yours, angle view', side: 'Yours, side view', free: 'Yours, free view' }[view.name]
    : { mine: 'Yours', reference: 'Target', outline: 'Yours with the target outline', diff: 'Red: yours sticks out. Green: target sticks out', fade: 'Target faded over yours', wipe: 'Yours | Target' }[mode];
  requestRender();
}
let viewAnim = 0;
function setView(name) {                 // glide the camera instead of cutting
  const to = VIEWS[name], from = { pitch: view.pitch, yaw: ((view.yaw + 180) % 360 + 360) % 360 - 180 }, id = ++viewAnim, t0 = performance.now();
  if (name !== 'plugin') cmpMode = 'mine';
  if (name === 'plugin' && view.name === 'plugin') return applyView();
  view = { name: name === 'plugin' ? 'free' : name, ...from }; applyView();
  const step = (now) => {
    if (id !== viewAnim) return;
    const u = Math.min(1, (now - t0) / 380), e = u * u * (3 - 2 * u);
    view.pitch = from.pitch + (to.pitch - from.pitch) * e; view.yaw = from.yaw + (to.yaw - from.yaw) * e;
    if (u < 1) { requestRender(); requestAnimationFrame(step); } else { view = { name, ...to }; applyView(); }
  };
  requestAnimationFrame(step);
}

// ---------- drag handles (on the knob and on the side section) ----------
const depthRange = (prefix) => { const p = GRIPS[get(prefix).style].params.find((k) => k.key === 'depth'); return p ? [p.min, p.max] : [0, 0.2]; };
const HANDLES = [
  { path: 'parts.top.width', label: 'Top width', tab: 0, lo: 0.4, hi: 1.3, pt: (d) => [d.Rt * (1 - d.T.taper * 0.5), 0, d.z0 + d.wallT * 0.5] },
  { path: 'parts.top.taper', label: 'Taper', tab: 0, lo: 0, hi: 0.6, pt: (d) => [-d.Rt * (1 - d.T.taper), 0, d.z0 + d.wallT] },
  { path: 'parts.top.round', label: 'Rounding', tab: 0, lo: 0, hi: 0.5, pt: (d) => { const c = d.T.round * d.R * (1 - Math.SQRT1_2); return [d.Rt * (1 - d.T.taper) - c, 0, d.z0 + d.Ht - Math.max(d.T.dome, 0) * d.R - c]; } },
  { path: 'parts.top.height', label: 'Top height', tab: 0, lo: 0.1, hi: 1.2, pt: (d) => [-d.Rt * (1 - d.T.taper) * 0.4, 0, d.z0 + d.Ht] },
  { path: 'parts.top.grip.depth', label: 'Grip depth', tab: 0, range: () => depthRange('parts.top.grip'), when: (s) => s.parts.top.grip.style !== 'smooth', flat: true,
    pt: (d) => [0, -d.Rt * (1 - d.T.taper * 0.25) * (1 - d.T.grip.depth), d.z0 + d.wallT * 0.25] },
  { path: 'parts.base.width', label: 'Base width', tab: 1, lo: 0.5, hi: 1.5, when: (s) => s.parts.base.on, pt: (d) => { const r = d.Rb * (1 - (d.B.taper || 0) * 0.5); return [r * 0.77, -r * 0.64, d.wallB * 0.5]; } },
  { path: 'parts.base.height', label: 'Base height', tab: 1, lo: 0.05, hi: 0.8, when: (s) => s.parts.base.on, pt: (d) => [-(d.Rb + d.Rt) / 2, 0, d.Hb] },
  { path: 'parts.base.bevel', label: 'Bevel', tab: 1, lo: 0, hi: 0.3, when: (s) => s.parts.base.on, pt: (d) => { const c = d.B.bevel * d.R * (1 - Math.SQRT1_2); return [d.Rb * (1 - (d.B.taper || 0)) - c, 0, d.Hb - c]; } },
  { path: 'pointer.start', label: 'Pointer start', tab: 2, lo: 0, hi: 0.9, spin: true, flat: true, pt: (d) => [d.Rt * d.P.start, 0, d.z0 + d.Ht + 0.02 * d.R] },
  { path: 'pointer.end', label: 'Pointer end', tab: 2, lo: 0.1, hi: 1, spin: true, flat: true, pt: (d) => [d.Rt * d.P.end, 0, d.z0 + d.Ht + 0.02 * d.R] },
];
// proj(point, handle) -> [x, y] in the svg's own units. minReach = hide a handle that could barely move in this view.
function handleLayer(svg, proj, { minReach, radius, font, filter = () => true }) {
  let drag = null;
  const groups = HANDLES.map((h) => {
    const g = document.createElementNS(NS, 'g'); g.setAttribute('class', 'handle');
    g.innerHTML = `<circle class="hit" r="${radius * 2.4}"/><circle class="dot" r="${radius}"/><text font-size="${font}" x="${radius * 1.8}" y="${-radius * 1.4}"></text>`;
    g.querySelector('text').textContent = h.label;
    const local = (e) => { const p = new DOMPoint(e.clientX, e.clientY).matrixTransform(svg.getScreenCTM().inverse()); return [p.x, p.y]; };
    g.addEventListener('pointerdown', (e) => {
      if (e.button !== 0) return; e.stopPropagation(); e.preventDefault();
      drag = { h, g, from: local(e), start: get(h.path), dv: g._dv }; g.setPointerCapture(e.pointerId);
      if (tabIndex !== h.tab) goTab(h.tab);
      hoverPath = h.path; update();
    });
    g.addEventListener('pointermove', (e) => {
      if (!drag || drag.h !== h) return;
      const m = local(e), d = [m[0] - drag.from[0], m[1] - drag.from[1]], dv = drag.dv, [lo, hi] = h.range ? h.range() : [h.lo, h.hi];
      const delta = (d[0] * dv[0] + d[1] * dv[1]) / (dv[0] * dv[0] + dv[1] * dv[1]) * (e.shiftKey ? 0.2 : 1);
      set(h.path, +clamp(drag.start + delta, lo, hi).toFixed(3));
      edited(false); refreshAll();
    });
    const end = () => { if (drag && drag.h === h) { drag = null; hoverPath = null; edited(true); refreshAll(); } };
    g.addEventListener('pointerup', end); g.addEventListener('pointercancel', end);
    g.addEventListener('pointerenter', () => { if (!drag) { hoverPath = h.path; markRows(); } });
    g.addEventListener('pointerleave', () => { if (!drag) { hoverPath = null; markRows(); } });
    svg.append(g);
    return g;
  });
  function update() {
    if (!S || !svg.isConnected) return;
    HANDLES.forEach((h, i) => {
      const g = groups[i];
      if (!filter(h) || (h.when && !h.when(S))) { g.style.display = 'none'; return; }
      const [lo, hi] = h.range ? h.range() : [h.lo, h.hi], v0 = get(h.path) ?? lo;
      const eps = (hi - lo) * 0.02 * (v0 + (hi - lo) * 0.02 > hi ? -1 : 1);
      const p0 = proj(h.pt(dims(S)), h); set(h.path, v0 + eps);
      const p1 = proj(h.pt(dims(S)), h); set(h.path, v0);
      const dv = [(p1[0] - p0[0]) / eps, (p1[1] - p0[1]) / eps];
      if (!(drag && drag.h === h) && Math.hypot(dv[0], dv[1]) * (hi - lo) < minReach) { g.style.display = 'none'; return; }
      g.style.display = ''; if (!(drag && drag.h === h)) g._dv = dv;
      g.setAttribute('transform', `translate(${p0[0].toFixed(1)},${p0[1].toFixed(1)})`);
      const hot = hoverPath === h.path; g.classList.toggle('hot', hot);
      g.querySelector('text').textContent = hot ? `${h.label} ${(+v0).toFixed(3)}` : h.label;
    });
  }
  return { update };
}
const v3 = new THREE.Vector3();
const stageHandles = handleLayer($('handles'), (p, h) => {
  v3.set(p[0], p[1], p[2]).applyMatrix4((h.spin ? spinG : tiltG).matrixWorld).project(camera);
  return [(v3.x + 1) * 500, (1 - v3.y) * 500];
}, { minReach: 60, radius: 13, font: 30 });

// side section: both parts cut through the middle, with the same handles
let sectionLayer = null, sectionScale = null;
function sectionBlock() {
  const svg = document.createElementNS(NS, 'svg'); svg.id = 'section'; svg.setAttribute('class', 'handles'); svg.setAttribute('viewBox', '0 0 340 150');
  const shape = document.createElementNS(NS, 'g'); svg.append(shape); svg._shape = shape;
  sectionLayer = handleLayer(svg, (p) => [170 + Math.sign(p[0] || 1) * Math.hypot(p[0], p[1]) * sectionScale, 132 - p[2] * sectionScale], { minReach: 8, radius: 5.5, font: 11, filter: (h) => !h.flat });
  return svg;
}
function drawSection() {
  const svg = $('section'); if (!svg || !S) return;
  const d = dims(S), tall = d.z0 + d.Ht, wide = Math.max(d.Rt, S.parts.base.on ? d.Rb : 0);
  sectionScale = Math.min(118 / tall, 150 / wide);
  const X = (r) => 170 + r * sectionScale, Y = (z) => 132 - z * sectionScale;
  const poly = (prof, R, H, z, on) => {
    const right = prof.map((p) => `${X(p[0] * R).toFixed(1)},${Y(z + p[1] * H).toFixed(1)}`), left = prof.slice().reverse().map((p) => `${X(-p[0] * R).toFixed(1)},${Y(z + p[1] * H).toFixed(1)}`);
    return `<polygon points="${right.concat(left).join(' ')}" fill="${hex(S.material.color)}" fill-opacity="${on ? 0.8 : 0.28}" stroke="${on ? '#f3ead8' : '#8a8172'}" stroke-width="${on ? 1.4 : 1}"/>`;
  };
  const act = TABS[tabIndex].title;
  svg._shape.innerHTML = `<line x1="0" x2="340" y1="132" y2="132" stroke="#4a4136"/>` +
    (S.parts.base.on ? poly(baseProfile(S.parts.base), d.Rb, d.Hb, 0, act === 'Base') : '') + poly(topProfile(S.parts.top), d.Rt, d.Ht, d.z0, act !== 'Base');
  sectionLayer.update();
}

// ---------- parameters ----------
const hex = (c) => '#' + new THREE.Color().setRGB(...c).getHexString();
const fromHex = (h) => { const c = new THREE.Color(h); return [c.r, c.g, c.b].map((v) => +v.toFixed(4)); };
const same = (a, b) => strip(a) === strip(b);
const el = (tag, cls, text) => { const e = document.createElement(tag); if (cls) e.className = cls; if (text !== undefined) e.textContent = text; return e; };
const C = (path, label, min, max, step) => ({ path, label, min, max, step });
const K = (key, label, min, max, step) => ({ key, label, min, max, step });

// Each grip type has its own parameters and remembers its own settings when you swap away and back.
const GRIPS = {
  smooth: { name: 'Smooth', start: {}, icon: { count: 0, depth: 0 }, params: [] },
  wave: { name: 'Fluted', start: { count: 9, depth: 0.045, sharpness: 1, phase: 0 }, icon: { count: 9, depth: 0.13 },
    params: [K('count', 'Flutes', 3, 24, 1), K('depth', 'Depth', 0, 0.2, 0.001), K('sharpness', 'Sharpness', 0.3, 4, 0.05), K('phase', 'Rotate', 0, 1, 0.01)] },
  scoop: { name: 'Creased', start: { count: 9, depth: 0.09, land: 0.1, phase: 0 }, icon: { count: 9, depth: 0.16 },
    params: [K('count', 'Flutes', 3, 24, 1), K('depth', 'Depth', 0, 0.2, 0.001), K('land', 'Ridge flat', 0, 0.5, 0.01), K('phase', 'Rotate', 0, 1, 0.01)] },
  scallop: { name: 'Finger', start: { count: 6, depth: 0.1, phase: 0 }, icon: { count: 6, depth: 0.1 },
    params: [K('count', 'Fingers', 3, 12, 1), K('depth', 'Scoop size', 0, 0.3, 0.005), K('phase', 'Rotate', 0, 1, 0.01)] },
  ridge: { name: 'Straight knurl', start: { count: 60, depth: 0.03 }, icon: { count: 26, depth: 0.09 },
    params: [K('count', 'Teeth', 20, 160, 1), K('depth', 'Depth', 0, 0.08, 0.001)] },
  diamond: { name: 'Diamond knurl', start: { count: 70, depth: 0.03 }, icon: { count: 26, depth: 0.09, cross: true },
    params: [K('count', 'Teeth', 20, 160, 1), K('depth', 'Depth', 0, 0.08, 0.001)] },
};
const FADE = K('fade', 'Fade to top', 0, 1, 0.01);
for (const G of Object.values(GRIPS)) if (G.params.length) G.params.push(FADE);
const KEPT = ['count', 'depth', 'sharpness', 'land', 'phase', 'fade'];

const TABS = [
  { title: 'Top', watch: ['parts.top', 'view.size', 'view.tilt'], hint: 'The upper part of the knob. Drag the dots on the knob or on this side view, or use the numbers.', section: true, items: () => [
    { sub: 'Shape' },
    C('parts.top.width', 'Width', 0.4, 1.3, 0.005), C('parts.top.height', 'Height', 0.1, 1.2, 0.005), C('parts.top.taper', 'Taper', 0, 0.6, 0.005),
    C('parts.top.round', 'Rounding', 0, 0.5, 0.005), C('parts.top.dome', 'Dome', -0.1, 0.4, 0.005),
    { sub: 'Grip' }, { grip: 'parts.top.grip' },
    { sub: 'Whole knob' }, C('view.size', 'Size', 0.8, 1.2, 0.005), C('view.tilt', 'Tilt', 0, 25, 0.5)] },
  { title: 'Base', watch: ['parts.base'], hint: 'The lower part, against the panel. Turn it off for a one-piece knob.', section: true, items: () => [
    { path: 'parts.base.on', label: 'Base', seg: [[true, 'On'], [false, 'Off']], rerender: true },
    ...(S.parts.base.on ? [{ sub: 'Shape' },
      C('parts.base.width', 'Width', 0.5, 1.5, 0.005), C('parts.base.height', 'Height', 0.05, 0.8, 0.005), C('parts.base.taper', 'Taper', 0, 0.6, 0.005), C('parts.base.bevel', 'Bevel', 0, 0.3, 0.005),
      { sub: 'Grip' }, { grip: 'parts.base.grip' }] : [])] },
  { title: 'Pointer', watch: ['pointer'], hint: 'The line that shows where the knob is set. Drag its two dots on the knob.', items: () => [
    C('pointer.start', 'Start', 0, 0.9, 0.01), C('pointer.end', 'End', 0.1, 1, 0.01), C('pointer.width', 'Width', 0.02, 0.3, 0.005),
    { path: 'pointer.color', label: 'Colour', chips: [['Black', '#0a0908'], ['White', '#f0ece2'], ['Cream', '#e6cfa4'], ['Red', '#b3261e']] }] },
  { title: 'Material', watch: ['material'], hint: 'Pick the nearest colour and finish, then fine-tune.', items: () => [
    { path: 'material.color', label: 'Colour', chips: [['Ivory', '#e9dcc0'], ['White', '#f2f0ea'], ['Grey', '#8a8a88'], ['Black', '#151515'], ['Oxblood', '#5a1a16']] },
    { label: 'Finish', preset: [
      ['Matte', { 'material.roughness': 0.6, 'material.coat': 0, 'material.coat_roughness': 0.3 }],
      ['Satin', { 'material.roughness': 0.35, 'material.coat': 0.15, 'material.coat_roughness': 0.25 }],
      ['Gloss', { 'material.roughness': 0.2, 'material.coat': 0.6, 'material.coat_roughness': 0.08 }]] },
    C('material.roughness', 'Roughness', 0.02, 1, 0.01), C('material.coat', 'Gloss coat', 0, 1, 0.01), C('material.coat_roughness', 'Coat rough', 0.02, 1, 0.01),
    ...(S.material.base_color ? [{ sub: 'Base' },
      { path: 'material.base_color', label: 'Colour', chips: [['Black', '#151515'], ['Grey', '#8a8a88'], ['Ivory', '#e9dcc0'], ['Oxblood', '#5a1a16']] },
      C('material.base_roughness', 'Roughness', 0.02, 1, 0.01)] : [])] },
  { title: 'Light', watch: ['light'], hint: 'The light stays still while the knob turns.', items: () => [
    C('light.key_power', 'Top light', 0, 20, 0.1), C('light.ambient', 'Room light', 0, 0.6, 0.01),
    C('light.key_pos.2', 'Height', 0.03, 0.8, 0.01), C('light.key_pos.1', 'Distance', 0.05, 0.6, 0.01), C('light.key_size.1', 'Softness', 0.01, 0.3, 0.005),
    C('light.rim_power', 'Edge light', 0, 8, 0.1), C('light.exposure', 'Exposure', -2, 1, 0.05)] },
];
// every numeric parameter of a tab, as {path, label, min, max, step} (used by Variations)
function numericOf(i, rec = S) {
  const keep = S; S = rec; const out = [];
  for (const it of TABS[i].items()) {
    if (it.grip) for (const p of GRIPS[getIn(rec, it.grip).style].params) out.push({ ...p, path: it.grip + '.' + p.key });
    else if (it.min !== undefined) out.push(it);
  }
  S = keep; return out;
}

const refreshers = [];
const paths = (c) => (c.preset ? Object.keys(c.preset[0][1]) : [c.path]);
const isChanged = (c) => paths(c).some((p) => !same(get(p), getIn(O, p)));
const tabChanged = (t) => t.watch.some((p) => !same(get(p), getIn(O, p)));
const fill = (r) => r.style.setProperty('--p', ((r.value - r.min) / (r.max - r.min) * 100).toFixed(1) + '%');
const places = (c) => (c.step >= 1 ? 0 : c.step < 0.01 ? 3 : 2);
const snap = (c, v) => +(Math.round(clamp(v, c.min, c.max) / c.step) * c.step).toFixed(4);

function swapGrip(prefix, style) {
  const g = get(prefix); if (style === g.style) return;
  g.saved = g.saved || {}; g.saved[g.style] = Object.fromEntries(KEPT.filter((k) => k in g).map((k) => [k, g[k]]));
  Object.assign(g, GRIPS[style].start, g.saved[style] || {}); g.style = style;
  edited(true); renderPanel();
}
function gripIcon(style) {
  const ic = GRIPS[style].icon, g = { style, phase: 0, sharpness: 1, land: 0.1, ...ic }, pts = [];
  for (let i = 0; i < 360; i++) { const t = 2 * Math.PI * i / 360, r = 13 * radial(g, ic.count, t, t, 1, 0); pts.push(`${(16 + r * Math.cos(t)).toFixed(1)},${(16 + r * Math.sin(t)).toFixed(1)}`); }
  return `<svg viewBox="0 0 32 32" width="32" height="32" aria-hidden="true"><polygon points="${pts.join(' ')}" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"/>` +
    (ic.cross ? '<path d="M10 10L22 22M22 10L10 22M16 8L24 16M8 16L16 24M16 8L8 16M24 16L16 24" stroke="currentColor" stroke-width="0.8" fill="none"/>' : '') + '</svg>';
}
function gripBlock(prefix) {
  const wrap = el('div'), box = el('div', 'types'), now = get(prefix).style;
  for (const [style, G] of Object.entries(GRIPS)) {
    const b = el('button', 'type' + (now === style ? ' on' : ''));
    b.innerHTML = gripIcon(style); b.append(el('span', '', G.name));
    b.setAttribute('aria-pressed', now === style);
    b.onclick = () => swapGrip(prefix, style);
    box.append(b);
  }
  wrap.append(box);
  for (const p of GRIPS[now].params) wrap.append(controlRow({ ...p, path: prefix + '.' + p.key }));
  return wrap;
}

function controlRow(c) {
  const row = el('div', 'ctl'), label = el('label', '', c.label);
  if (c.path) row.dataset.path = c.path;
  const reset = () => { for (const p of paths(c)) { const v = getIn(O, p); if (v !== undefined) set(p, clone(v)); } edited(true); c.rerender ? renderPanel() : refreshAll(); };
  const back = el('button', 'undo1', '↺'); back.title = 'Back to how it started (or double-click the name)'; back.setAttribute('aria-label', 'Reset ' + c.label);
  back.onclick = reset; label.ondblclick = reset;
  row.onpointerenter = () => { if (c.path) { hoverPath = c.path; requestRender(); if (sectionLayer) sectionLayer.update(); } };
  row.onpointerleave = () => { if (hoverPath === c.path) { hoverPath = null; requestRender(); if (sectionLayer) sectionLayer.update(); } };
  let body, sync;
  if (c.seg || c.preset) {
    body = el('span', 'seg');
    for (const [v, t] of (c.seg || c.preset)) {
      const b = el('button', '', c.seg ? t : v);
      b.onclick = () => { if (c.seg) set(c.path, v); else for (const [p, val] of Object.entries(t)) set(p, val); edited(true); c.rerender ? renderPanel() : refreshAll(); };
      b._on = () => (c.seg ? get(c.path) === v : Object.entries(t).every(([p, val]) => get(p) === val));
      body.append(b);
    }
    sync = () => { for (const b of body.children) b.classList.toggle('on', b._on()); };
  } else if (c.chips) {
    body = el('div', 'chips'); const pick = el('input'); pick.type = 'color'; pick.title = 'Any colour';
    for (const [name, h] of [['As it started', hex(getIn(O, c.path))], ...c.chips]) {
      const b = el('button', 'chip'); b.style.background = h; b.title = name; b.setAttribute('aria-label', name);
      b.onclick = () => { set(c.path, name === 'As it started' ? [...getIn(O, c.path)] : fromHex(h)); edited(true); refreshAll(); };
      b._hex = h; body.append(b);
    }
    pick.oninput = () => { set(c.path, fromHex(pick.value)); edited(false); refresh(); };
    pick.onchange = () => edited(true);
    body.append(pick);
    sync = () => { const h = hex(get(c.path)); pick.value = h; for (const b of body.querySelectorAll('.chip')) b.classList.toggle('on', b._hex === h); };
  } else {
    body = el('div', 'num');
    const input = el('input'), num = el('input');
    input.type = 'range'; num.type = 'number';
    for (const i of [input, num]) { i.min = c.min; i.max = c.max; i.step = c.step; i.setAttribute('aria-label', c.label); }
    num.title = 'Drag left or right to change. Shift = fine, Ctrl = coarse. Click to type.';
    input.oninput = () => { set(c.path, +input.value); edited(false); refresh(); drawSection(); };
    input.onchange = () => edited(true);
    num.onchange = () => { set(c.path, snap(c, +num.value || 0)); edited(true); refresh(); drawSection(); };
    // scrub: drag the number itself
    let scrub = null;
    num.addEventListener('pointerdown', (e) => { if (e.button === 0 && document.activeElement !== num) { scrub = { x: e.clientX, start: get(c.path) ?? c.min, moved: false }; num.setPointerCapture(e.pointerId); e.preventDefault(); } });
    num.addEventListener('pointermove', (e) => {
      if (!scrub) return;
      const dx = e.clientX - scrub.x; if (!scrub.moved && Math.abs(dx) < 3) return;
      scrub.moved = true;
      const perPx = (c.max - c.min) / 260 * (e.shiftKey ? 0.1 : e.ctrlKey ? 4 : 1);
      set(c.path, snap(c, scrub.start + dx * perPx)); edited(false); refresh(); drawSection();
    });
    const endScrub = () => { if (!scrub) return; const moved = scrub.moved; scrub = null; if (moved) edited(true); else { num.focus(); num.select(); } };
    num.addEventListener('pointerup', endScrub); num.addEventListener('pointercancel', endScrub);
    body.append(input, num);
    sync = () => { const v = get(c.path) ?? Math.max(c.min, 0); input.value = v; fill(input); if (document.activeElement !== num || +num.value !== v) num.value = (+v).toFixed(places(c)); };
  }
  row.append(label, body, back);
  const refresh = () => { sync(); row.classList.toggle('changed', isChanged(c)); };
  refreshers.push(refresh);
  return row;
}
function markRows() { requestRender(); if (sectionLayer) sectionLayer.update(); for (const r of document.querySelectorAll('.ctl')) r.style.background = r.dataset.path && r.dataset.path === hoverPath ? 'rgba(234,217,184,.12)' : ''; }

function renderTabs() {
  const nav = $('tabs'); nav.innerHTML = '';
  TABS.forEach((t, i) => {
    const b = el('button', (i === tabIndex ? 'current ' : '') + (tabChanged(t) ? 'changed' : ''), t.title);
    b.setAttribute('role', 'tab'); b.setAttribute('aria-selected', i === tabIndex);
    b.title = `${t.title} (key ${i + 1})` + (tabChanged(t) ? ', changed' : '');
    b.onclick = () => goTab(i);
    nav.append(b);
  });
}
function renderPanel() {
  const t = TABS[tabIndex], host = $('panel'), keep = host.scrollTop; host.innerHTML = ''; refreshers.length = 0; sectionLayer = null;
  host.append(el('p', 'tip', t.hint));
  if (t.section) host.append(sectionBlock());
  for (const it of t.items()) host.append(it.sub ? el('div', 'sub', it.sub) : it.grip ? gripBlock(it.grip) : controlRow(it));
  refreshAll(); host.scrollTop = keep;
}
function refreshAll() { for (const r of refreshers) r(); renderTabs(); drawSection(); }
function goTab(i) {
  tabIndex = clamp(i, 0, TABS.length - 1);
  try { localStorage.setItem('kw-tab', tabIndex); } catch {}
  $('panel').scrollTop = 0; renderPanel();
}

// ---------- history, save, versions ----------
let saveTimer = 0, retryTimer = 0, dirty = false;
function edited(commit) {
  if (S.status === 'finished') { S.status = 'draft'; $('doneMsg').textContent = ''; }
  requestRender(); scheduleSave();
  if (commit) {
    const j = JSON.stringify(S);
    if (j !== history[hIndex]) { history.length = hIndex + 1; history.push(j); hIndex++; }
    updateButtons(); drawBackground(); renderTabs(); scheduleScore();
  }
}
function updateButtons() {
  $('undo').disabled = hIndex <= 0; $('redo').disabled = hIndex >= history.length - 1;
  $('reset').disabled = JSON.stringify(S) === opened;
}
function restore(json, commit) {
  S = JSON.parse(json); drawBackground(); requestRender(); scheduleSave(); scheduleScore();
  if (commit && json !== history[hIndex]) { history.length = hIndex + 1; history.push(json); hIndex++; }
  updateButtons(); renderPanel(); $('notes').value = S.notes || '';
}
function saveState(text, bad) { const s = $('save'); s.textContent = text; s.classList.toggle('bad', !!bad); }
function scheduleSave() { dirty = true; saveState('Saving…'); clearTimeout(saveTimer); saveTimer = setTimeout(save, 300); }
async function save() {
  clearTimeout(saveTimer); clearTimeout(retryTimer);
  let ok = false;
  try { ok = (await fetch('/api/recipe/' + S.name, { method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(S) })).ok; } catch {}
  if (ok) { dirty = false; saveState('Saved ' + new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })); }
  else { saveState('NOT SAVED, retrying', true); retryTimer = setTimeout(save, 3000); }   // never fail quietly
  return ok;
}
addEventListener('beforeunload', (e) => { if (dirty) { e.preventDefault(); e.returnValue = ''; } });

let versions = [];
async function loadVersions() {
  try { const r = await fetch('/api/versions/' + S.name); versions = r.ok ? await r.json() : []; } catch { versions = []; }
  drawVersions();
}
function drawVersions() {
  const host = $('versions'); for (const b of [...host.querySelectorAll('.ver')]) b.remove();
  versions.slice().reverse().forEach((v) => {
    const b = el('button', 'ver'), im = el('img'); im.src = v.thumb; im.alt = '';
    const when = new Date(v.t).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    b.append(im, el('span', '', v.label || when)); b.title = `${v.label || 'Kept'} at ${when}. Click to go back to it.`;
    b.onclick = () => { const r = clone(v.recipe); r.name = S.name; restore(JSON.stringify(r), true); };
    host.append(b);
  });
}
async function keepVersion(label) {
  versions.push({ t: Date.now(), label, thumb: picture(S, 132).toDataURL('image/jpeg', 0.82), recipe: JSON.parse(strip(S)) });
  if (versions.length > 14) versions = versions.slice(-14);
  drawVersions();
  try { await fetch('/api/versions/' + S.name, { method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(versions) }); } catch {}
}

// ---------- sheet: starting shapes and variations ----------
const SHAPES = [
  ['Fluted cap', { base: { on: false, width: 1.07, height: 0.3, taper: 0, bevel: 0.05, grip: { style: 'smooth' } },
    top: { width: 1.03, height: 0.5, taper: 0.17, round: 0.16, dome: 0.012, grip: { style: 'wave', count: 9, depth: 0.055, sharpness: 1, phase: 0.75, fade: 0.6 } } }, { start: 0.36, end: 0.84, width: 0.12 }],
  ['Knurled dome', { base: { on: false, width: 1.07, height: 0.3, taper: 0, bevel: 0.05, grip: { style: 'smooth' } },
    top: { width: 0.96, height: 0.8, taper: 0.06, round: 0.3, dome: 0.1, grip: { style: 'diamond', count: 70, depth: 0.03, fade: 0 } } }, { start: 0.1, end: 0.5, width: 0.08 }],
  ['Skirted', { base: { on: true, width: 1.28, height: 0.2, taper: 0.22, bevel: 0.03, grip: { style: 'smooth' } },
    top: { width: 0.86, height: 0.62, taper: 0.05, round: 0.1, dome: 0.03, grip: { style: 'ridge', count: 60, depth: 0.025, fade: 0 } } }, { start: 0.2, end: 0.8, width: 0.1 }],
  ['Pointer knob', { base: { on: true, width: 1.16, height: 0.16, taper: 0.08, bevel: 0.05, grip: { style: 'smooth' } },
    top: { width: 0.82, height: 0.72, taper: 0.12, round: 0.1, dome: 0.02, grip: { style: 'smooth' } } }, { start: 0.05, end: 0.95, width: 0.1 }],
  ['Finger grip', { base: { on: false, width: 1.07, height: 0.3, taper: 0, bevel: 0.05, grip: { style: 'smooth' } },
    top: { width: 1, height: 0.58, taper: 0.04, round: 0.12, dome: 0.02, grip: { style: 'scallop', count: 6, depth: 0.1, phase: 0, fade: 0 } } }, { start: 0.3, end: 0.8, width: 0.1 }],
  ['Smooth dome', { base: { on: false, width: 1.07, height: 0.3, taper: 0, bevel: 0.05, grip: { style: 'smooth' } },
    top: { width: 1, height: 0.62, taper: 0.1, round: 0.36, dome: 0.1, grip: { style: 'smooth' } } }, { start: 0.25, end: 0.75, width: 0.1 }],
];
function openSheet(title, hint) {
  $('sheetTitle').textContent = title; $('sheetHint').textContent = hint;
  $('sheetBar').innerHTML = ''; $('sheetGrid').innerHTML = ''; $('sheet').hidden = false; $('sheetClose').focus();
}
function closeSheet() { $('sheet').hidden = true; requestRender(); }
function cell(rec, caption, onPick, current) {
  const b = el('button', 'cell' + (current ? ' current' : '')); b.append(picture(rec, 236), el('span', '', caption));
  if (!current) b.onclick = onPick; else b.disabled = false;
  return b;
}
function openShapes() {
  openSheet('Starting shapes', 'Pick the closest shape. It keeps your colour, finish and lighting. Your current knob is kept as a version first.');
  for (const [name, parts, pointer] of SHAPES) {
    const rec = clone(S); rec.parts = clone(parts); Object.assign(rec.pointer, pointer);
    $('sheetGrid').append(cell(rec, name, async () => {
      await keepVersion('Before shape');
      S = rec; closeSheet(); edited(true); renderPanel();
    }));
  }
}

function rng(seed) { return () => { seed = (seed + 0x6D2B79F5) | 0; let t = Math.imul(seed ^ (seed >>> 15), 1 | seed); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
const vary = { mode: 'around', tab: 0, spread: 0.12, seed: 1, x: 0, y: 1 };
function openVariations() {
  vary.tab = tabIndex; vary.x = 0; vary.y = 1;
  openSheet('Variations', 'Nine versions of this knob. Click the one you like best and nine more appear around it. The middle one is where you are now.');
  drawVariations();
}
function drawVariations() {
  const bar = $('sheetBar'), grid = $('sheetGrid'); bar.innerHTML = ''; grid.innerHTML = '';
  const ctl = numericOf(vary.tab); vary.x = Math.min(vary.x, ctl.length - 1); vary.y = Math.min(vary.y, ctl.length - 1);
  const seg = (items, cur, on) => { const s = el('span', 'seg'); for (const [v, t] of items) { const b = el('button', v === cur ? 'on' : '', t); b.onclick = () => { on(v); drawVariations(); }; s.append(b); } return s; };
  const pick = (cur, on) => { const s = el('select'); ctl.forEach((c, i) => s.add(new Option(c.label, i, false, i === cur))); s.onchange = () => { on(+s.value); drawVariations(); }; return s; };
  bar.append(el('span', 'lead', 'Vary'), seg(TABS.map((t, i) => [i, t.title]), vary.tab, (v) => { vary.tab = v; }),
    seg([['around', 'Mixed'], ['sweep', 'Two settings']], vary.mode, (v) => { vary.mode = v; }),
    el('span', 'lead', 'By'), seg([[0.06, 'A little'], [0.12, 'Some'], [0.25, 'A lot']], vary.spread, (v) => { vary.spread = v; }));
  if (vary.mode === 'sweep' && ctl.length > 1) bar.append(el('span', 'lead', 'Across'), pick(vary.x, (v) => { vary.x = v; }), el('span', 'lead', 'Down'), pick(vary.y, (v) => { vary.y = v; }));
  else if (vary.mode === 'around') { const sh = el('button', '', 'Shuffle'); sh.onclick = () => { vary.seed++; drawVariations(); }; bar.append(sh); }
  if (!ctl.length) { grid.append(el('div', 'small', 'Nothing to vary here. Pick another group.')); return; }
  const rand = rng(vary.seed * 7919 + vary.tab), arrow = (d) => (d > 0 ? '+' : '−');
  for (let n = 0; n < 9; n++) {
    const rec = clone(S), notes = [];
    const nudge = (c, amount) => {
      const before = getIn(rec, c.path) ?? c.min, after = snap(c, before + amount * (c.max - c.min));
      if (after !== before) { setIn(rec, c.path, after); notes.push(`${c.label} ${arrow(after - before)}`); }
    };
    if (n !== 4) {
      if (vary.mode === 'sweep' && ctl.length > 1) { nudge(ctl[vary.x], (n % 3 - 1) * vary.spread); if (vary.y !== vary.x) nudge(ctl[vary.y], (1 - ((n / 3) | 0)) * vary.spread); }
      else { const count = 1 + ((rand() * Math.min(3, ctl.length)) | 0), pool = ctl.slice(); for (let k = 0; k < count && pool.length; k++) nudge(pool.splice((rand() * pool.length) | 0, 1)[0], (rand() < 0.5 ? -1 : 1) * vary.spread * (0.6 + rand() * 0.8)); }
    }
    grid.append(cell(rec, n === 4 ? 'Now' : notes.join(', ') || 'Same', () => { S = rec; vary.seed++; edited(true); renderPanel(); drawVariations(); }, n === 4));
  }
}

// ---------- match outline: fit the silhouette to the target automatically ----------
async function matchOutline() {
  const btn = $('match'), tm = await targetMask(); if (!tm) return;
  const ang = refAngle(), label = btn.innerHTML; btn.disabled = true;
  await keepVersion('Before match');
  const score = (rec) => iou(maskOf(rec, ang), tm);
  let best = clone(S), bs = score(best), tries = 0; const before = bs;
  const breathe = async () => { if (++tries % 12 === 0) { btn.textContent = `Matching… ${Math.round(bs * 100)}%`; await new Promise((r) => setTimeout(r, 0)); } };
  const tryRec = async (rec) => { const s = score(rec); await breathe(); if (s > bs + 1e-4) { best = rec; bs = s; return true; } return false; };
  const grip = best.parts.top.grip;
  // The outline cannot see the flutes on the face, so the flute count is left alone: only size, position, tilt and proportions move.
  const P = [['view.size', 0.8, 1.2, 0.04], ['view.offset_y', -0.006, 0.006, 0.002], ['view.tilt', 0, 25, 3], ['parts.top.width', 0.4, 1.3, 0.04],
    ['parts.top.height', 0.1, 1.2, 0.06], ['parts.top.taper', 0, 0.6, 0.04]];
  if (grip.style !== 'smooth') P.push(['parts.top.grip.depth', ...depthRange('parts.top.grip'), 0.02], ['parts.top.grip.phase', 0, 1, 0.05]);
  if (best.parts.base.on) P.push(['parts.base.width', 0.5, 1.5, 0.04], ['parts.base.height', 0.05, 0.8, 0.05]);
  for (let round = 0; round < 4; round++) {
    for (const [path, lo, hi, step0] of P) {
      const step = step0 / 2 ** round;
      for (const dir of [1, -1]) for (let k = 0; k < 5; k++) {
        const r = clone(best), v = clamp(getIn(r, path) + dir * step, lo, hi);
        if (v === getIn(best, path)) break;
        setIn(r, path, +v.toFixed(4)); if (!(await tryRec(r))) break;
      }
    }
  }
  S = best; btn.innerHTML = label; btn.disabled = false;
  edited(true); renderPanel(); clearTimeout(scoreTimer);
  $('score').textContent = `Outline match ${Math.round(before * 100)}% → ${Math.round(bs * 100)}%`;
  if (cmpMode === 'mine' && view.name === 'plugin') { cmpMode = 'outline'; applyView(); }
}

// ---------- wiring ----------
function setTurn(v) { turn = v; $('turn').value = v; fill($('turn')); $('turnOut').textContent = v + '°'; requestRender(); drawReference(); }
$('undo').onclick = () => { if (hIndex > 0) restore(history[--hIndex], false); };
$('redo').onclick = () => { if (hIndex < history.length - 1) restore(history[++hIndex], false); };
$('reset').onclick = () => restore(opened, true);
$('done').onclick = async () => {
  S.status = 'finished'; S.notes = $('notes').value;
  const ok = await save(); if (ok) keepVersion('Finished');
  $('doneMsg').textContent = ok ? 'Handed over. Tell Claude it is done and it will render and install it.' : 'Could not save. Is the workshop server still running?';
};
$('notes').oninput = () => { S.notes = $('notes').value; if (S.status === 'finished') { S.status = 'draft'; $('doneMsg').textContent = ''; } scheduleSave(); };
$('sweep').onclick = () => {
  const t0 = performance.now(), dur = 3600;
  const tick = (now) => {
    const u = Math.min(1, (now - t0) / dur), tri = u < 0.5 ? u * 2 : 2 - u * 2;
    setTurn(Math.round(-135 + 270 * tri));
    if (u < 1) requestAnimationFrame(tick); else setTurn(30);
  };
  requestAnimationFrame(tick);
};
$('keep').onclick = () => keepVersion('');
$('openShapes').onclick = openShapes;
$('openVars').onclick = openVariations;
$('match').onclick = matchOutline;
$('sheetClose').onclick = closeSheet;
$('sheet').addEventListener('pointerdown', (e) => { if (e.target === $('sheet')) closeSheet(); });
const typing = (e) => ['TEXTAREA', 'SELECT'].includes(e.target.tagName) || (e.target.tagName === 'INPUT' && e.target.type !== 'range');
addEventListener('keydown', (e) => {
  if (e.key === 'Escape' && !$('sheet').hidden) return closeSheet();
  if (!$('sheet').hidden) return;
  if (e.code === 'Space' && !typing(e) && e.target.tagName !== 'BUTTON') { e.preventDefault(); if (!flick) { flick = true; if (view.name !== 'plugin') view = { name: 'plugin', ...VIEWS.plugin }; applyView(); } return; }
  if (!e.ctrlKey && !e.metaKey && !e.altKey && !typing(e) && e.key >= '1' && e.key <= String(TABS.length)) return goTab(+e.key - 1);
  if (!(e.ctrlKey || e.metaKey) || typing(e)) return;
  if (e.key === 'z' && !e.shiftKey) { e.preventDefault(); $('undo').click(); }
  else if (e.key === 'y' || (e.key === 'z' && e.shiftKey)) { e.preventDefault(); $('redo').click(); }
});
addEventListener('keyup', (e) => { if (e.code === 'Space' && flick) { flick = false; applyView(); } });
$('turn').oninput = () => setTurn(+$('turn').value);
$('cmp').oninput = () => { fill($('cmp')); applyView(); };
fill($('cmp'));
for (const b of $('viewSeg').children) b.onclick = () => setView(b.dataset.v);
for (const b of $('cmpSeg').children) b.onclick = () => {
  cmpMode = b.dataset.m; viewAnim++;
  if (cmpMode !== 'mine') view = { name: 'plugin', ...VIEWS.plugin };
  if ((cmpMode === 'outline' || cmpMode === 'diff') && turn !== refAngle() && !(currentRef() || {}).strip) setTurn(refAngle());   // a photo target only has one pointer angle
  applyView();
};
$('refSel').onchange = () => { drawReference(); scheduleScore(); requestRender(); };
$('recipeSel').onchange = () => { location.search = '?r=' + $('recipeSel').value; };
$('gl').style.filter = LOOK;

// drag the knob (not a dot) to look around it
let orbitDrag = null;
const stage = $('stage');
stage.addEventListener('pointerdown', (e) => { if (e.button === 0 && !e.target.closest('.handle')) { orbitDrag = { x: e.clientX, y: e.clientY, moved: false }; stage.setPointerCapture(e.pointerId); } });
stage.addEventListener('pointermove', (e) => {
  if (!orbitDrag) return;
  const dx = e.clientX - orbitDrag.x, dy = e.clientY - orbitDrag.y;
  if (!orbitDrag.moved && Math.hypot(dx, dy) < 4) return;
  orbitDrag = { x: e.clientX, y: e.clientY, moved: true };
  cmpMode = 'mine'; viewAnim++;
  view = { name: 'free', pitch: clamp(view.pitch - dy * 0.4, 0, 100), yaw: view.yaw - dx * 0.4 };
  applyView();
});
const endOrbit = () => { orbitDrag = null; };
stage.addEventListener('pointerup', endOrbit); stage.addEventListener('pointercancel', endOrbit);

const DEFAULT_PARTS = SHAPES[0][1];

async function start() {
  const names = await (await fetch('/api/recipes')).json();
  const want = new URLSearchParams(location.search).get('r'), name = names.includes(want) ? want : names[0];
  S = await (await fetch('/api/recipe/' + name)).json();
  for (const n of names) $('recipeSel').add(new Option(n === name ? (S.label || n) : n, n, false, n === name));
  if (!S.parts) S.parts = clone(DEFAULT_PARTS);      // recipe from before Top/Base parts
  delete S.shape; delete S.grip;
  for (const p of [S.parts.top, S.parts.base]) { p.taper ??= 0; }
  opened = JSON.stringify(S); O = JSON.parse(opened); history = [opened]; hIndex = 0;
  (S.references || []).forEach((r, i) => $('refSel').add(new Option('Target: ' + r.label, i)));
  $('refSel').hidden = (S.references || []).length < 2;
  try { tabIndex = clamp(+localStorage.getItem('kw-tab') || 0, 0, TABS.length - 1); } catch {}
  $('notes').value = S.notes || ''; $('noteBox').open = !!S.notes;
  if (S.status === 'finished') $('doneMsg').textContent = 'Handed over. Tell Claude it is done.';
  setTurn(turn); renderPanel(); updateButtons(); applyView(); saveState('Saved');
  await drawBackground(); await drawReference(); requestRender();
  loadVersions(); scheduleScore();
}
start();
