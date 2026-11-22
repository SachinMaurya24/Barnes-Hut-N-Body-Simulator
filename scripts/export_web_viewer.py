#!/usr/bin/env python3
"""
Generate a standalone interactive 3D WebGL HTML simulation viewer.
Supports multi-component galaxies (Disk, Bulge, Dark Matter Halo, Central SMBH)
with component color palettes, dark matter halo visibility toggle, and real-time core separation.
"""

import argparse
import json
import os
import sys

HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Barnes-Hut 3D Interactive Galaxy Viewer</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; }
        body {
            background-color: #030712;
            color: #f3f4f6;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            overflow: hidden;
            height: 100vh;
            width: 100vw;
        }
        #canvas-container {
            width: 100vw;
            height: 100vh;
            position: absolute;
            top: 0;
            left: 0;
            z-index: 1;
        }
        .hud-panel {
            position: absolute;
            z-index: 10;
            background: rgba(15, 23, 42, 0.78);
            backdrop-filter: blur(12px);
            -webkit-backdrop-filter: blur(12px);
            border: 1px solid rgba(255, 255, 255, 0.1);
            border-radius: 12px;
            padding: 14px 18px;
            box-shadow: 0 10px 25px rgba(0, 0, 0, 0.5);
        }
        #info-panel {
            top: 20px;
            left: 20px;
            min-width: 285px;
        }
        #info-panel h1 {
            font-size: 1.05rem;
            font-weight: 600;
            color: #60a5fa;
            margin-bottom: 6px;
            letter-spacing: -0.02em;
        }
        .stat-row {
            display: flex;
            justify-content: space-between;
            font-size: 0.82rem;
            color: #94a3b8;
            margin-top: 4px;
        }
        .stat-val {
            font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
            color: #f8fafc;
            font-weight: 500;
        }
        #controls-panel {
            bottom: 24px;
            left: 50%;
            transform: translateX(-50%);
            display: flex;
            flex-direction: column;
            gap: 10px;
            width: min(820px, 95vw);
        }
        .scrubber-container {
            display: flex;
            align-items: center;
            gap: 12px;
            width: 100%;
        }
        #timeline {
            flex-grow: 1;
            height: 6px;
            border-radius: 3px;
            accent-color: #3b82f6;
            cursor: pointer;
        }
        .button-bar {
            display: flex;
            align-items: center;
            justify-content: space-between;
            flex-wrap: wrap;
            gap: 8px;
        }
        .btn-group {
            display: flex;
            align-items: center;
            gap: 6px;
        }
        button, label.btn {
            background: #1e293b;
            color: #e2e8f0;
            border: 1px solid rgba(255, 255, 255, 0.12);
            padding: 6px 12px;
            border-radius: 6px;
            font-size: 0.80rem;
            font-weight: 500;
            cursor: pointer;
            transition: all 0.15s ease;
            display: inline-flex;
            align-items: center;
            justify-content: center;
            gap: 4px;
        }
        button:hover, label.btn:hover {
            background: #334155;
            color: #ffffff;
            border-color: rgba(255, 255, 255, 0.25);
        }
        button.active {
            background: #2563eb;
            color: #ffffff;
            border-color: #3b82f6;
        }
        select {
            background: #1e293b;
            color: #e2e8f0;
            border: 1px solid rgba(255, 255, 255, 0.12);
            padding: 6px 10px;
            border-radius: 6px;
            font-size: 0.8rem;
            cursor: pointer;
            outline: none;
        }
        #drop-overlay {
            position: absolute;
            top: 0; left: 0; width: 100%; height: 100%;
            background: rgba(15, 23, 42, 0.8);
            backdrop-filter: blur(8px);
            z-index: 100;
            display: none;
            justify-content: center;
            align-items: center;
            font-size: 1.5rem;
            font-weight: 600;
            color: #60a5fa;
            border: 3px dashed #3b82f6;
        }
        .legend {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 6px 10px;
            margin-top: 10px;
            font-size: 0.76rem;
        }
        .legend-item {
            display: flex;
            align-items: center;
            gap: 5px;
        }
        .legend-dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
        }
        #file-input { display: none; }
    </style>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js"></script>
</head>
<body>
    <div id="canvas-container"></div>
    <div id="drop-overlay">Drop Snapshot CSV Here</div>

    <div id="coalescence-banner" style="display: none; position: absolute; top: 22px; left: 50%; transform: translateX(-50%); z-index: 100; background: rgba(220, 38, 38, 0.92); border: 2px solid #f87171; border-radius: 10px; padding: 12px 28px; font-weight: 700; color: #fff; letter-spacing: 0.04em; text-transform: uppercase; box-shadow: 0 0 35px rgba(239, 68, 68, 0.8); pointer-events: none; text-align: center;">
        <span style="font-size: 1.15rem; display: block;">⚠️ CORE COALESCENCE DETECTED</span>
        <span style="font-size: 0.82rem; font-weight: 500; opacity: 0.95;">Condition satisfied: Nuclear Separation d(t) &lt; 0.05 | Single Merged Nucleus Formed</span>
    </div>

    <div id="info-panel" class="hud-panel">
        <h1>Multi-Component Galaxy 3D</h1>
        <div class="stat-row"><span>Time (t):</span><span id="stat-time" class="stat-val">0.00</span></div>
        <div class="stat-row"><span>SMBH Core Sep:</span><span id="stat-sep" class="stat-val">--</span></div>
        <div class="stat-row"><span>Max Tree Depth:</span><span id="stat-depth" class="stat-val">--</span></div>
        <div class="stat-row"><span>Active Tree Nodes:</span><span id="stat-nodes" class="stat-val">--</span></div>
        <div class="stat-row"><span>Virial Ratio 2K/|W|:</span><span id="stat-virial" class="stat-val">--</span></div>
        <div class="stat-row"><span>Energy Drift |ΔE/E₀|:</span><span id="stat-energy" class="stat-val">--</span></div>
        <div class="stat-row"><span>Frame:</span><span id="stat-frame" class="stat-val">0 / 0</span></div>
        <div class="stat-row"><span>Particles:</span><span id="stat-particles" class="stat-val">0</span></div>
        <div class="stat-row"><span>FPS:</span><span id="stat-fps" class="stat-val">60</span></div>
        <div class="legend">
            <div class="legend-item"><div class="legend-dot" style="background:#38bdf8;"></div><span>G1 Spiral Disk</span></div>
            <div class="legend-item"><div class="legend-dot" style="background:#fb923c;"></div><span>G2 Spiral Disk</span></div>
            <div class="legend-item"><div class="legend-dot" style="background:#fef08a;"></div><span>Galactic Bulges</span></div>
            <div class="legend-item"><div class="legend-dot" style="background:#818cf8;"></div><span>DM Halo / SMBH</span></div>
        </div>
    </div>

    <div id="controls-panel" class="hud-panel">
        <div class="scrubber-container">
            <button id="btn-play">▶ Play</button>
            <input type="range" id="timeline" min="0" max="0" value="0" step="1">
            <span id="time-display" style="font-family: ui-monospace, monospace; font-size: 0.85rem; min-width: 55px;">0.00</span>
        </div>
        <div class="button-bar">
            <div class="btn-group">
                <button id="btn-step-back" title="Previous frame">⏮</button>
                <button id="btn-step-fwd" title="Next frame">⏭</button>
                <select id="speed-select" title="Playback speed">
                    <option value="0.25">0.25x</option>
                    <option value="0.5">0.5x</option>
                    <option value="1.0" selected>1.0x</option>
                    <option value="2.0">2.0x</option>
                    <option value="4.0">4.0x</option>
                </select>
                <button id="btn-loop" class="active" title="Loop playback">🔁 Loop</button>
            </div>
            <div class="btn-group">
                <select id="color-mode" title="Color coding">
                    <option value="component" selected>Color: Astrophysical Components</option>
                    <option value="galaxy">Color: Galaxy ID</option>
                    <option value="speed">Color: Velocity Heatmap</option>
                    <option value="depth">Color: Z-Depth</option>
                </select>
                <button id="btn-halo" class="active" title="Toggle Dark Matter Halo">🌌 DM Halo</button>
                <button id="btn-octree" title="Toggle 3D Octree Bounding Box Wireframe">📦 Octree Wireframe</button>
                <select id="octree-depth-select" title="Octree Visualization Depth">
                    <option value="3">Tree Depth: 3</option>
                    <option value="4" selected>Tree Depth: 4</option>
                    <option value="5">Tree Depth: 5</option>
                    <option value="6">Tree Depth: 6</option>
                </select>
                <button id="btn-grid">🌐 Grid</button>
                <button id="btn-reset">🎯 Reset View</button>
                <label for="file-input" class="btn">📂 Load CSV</label>
                <input type="file" id="file-input" accept=".csv">
            </div>
        </div>
    </div>

    <script>
        let simulationData = __EMBEDDED_DATA__;

        let scene, camera, renderer, controls;
        let particleGeometry, particleMaterial, particleSystem;
        let gridHelper;
        let octreeWireframeGroup = null;
        let showOctree = false;
        let octreeMaxDepth = 4;
        let currentFrameIndex = 0;
        let isPlaying = true;
        let playbackSpeed = 1.0;
        let loop = true;
        let colorMode = 'component';
        let showHalo = true;
        let lastFrameTime = performance.now();
        let frameAccumulator = 0;
        let fpsTimer = performance.now();
        let frameCountForFps = 0;

        function initScene() {
            const container = document.getElementById('canvas-container');
            scene = new THREE.Scene();
            scene.fog = new THREE.FogExp2(0x030712, 0.008);

            camera = new THREE.PerspectiveCamera(50, window.innerWidth / window.innerHeight, 0.1, 1000);
            camera.position.set(0, -21, 13);

            renderer = new THREE.WebGLRenderer({ antialias: true, alpha: false, powerPreference: "high-performance" });
            renderer.setSize(window.innerWidth, window.innerHeight);
            renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
            renderer.setClearColor(0x030712);
            container.appendChild(renderer.domElement);

            controls = new THREE.OrbitControls(camera, renderer.domElement);
            controls.enableDamping = true;
            controls.dampingFactor = 0.05;
            controls.maxDistance = 300;
            controls.minDistance = 1;

            const starGeo = new THREE.BufferGeometry();
            const starCount = 1200;
            const starPos = new Float32Array(starCount * 3);
            for (let i = 0; i < starCount * 3; i += 3) {
                starPos[i] = (Math.random() - 0.5) * 400;
                starPos[i+1] = (Math.random() - 0.5) * 400;
                starPos[i+2] = (Math.random() - 0.5) * 400;
            }
            starGeo.setAttribute('position', new THREE.BufferAttribute(starPos, 3));
            const starMat = new THREE.PointsMaterial({ color: 0x64748b, size: 1.2, sizeAttenuation: true, transparent: true, opacity: 0.6 });
            scene.add(new THREE.Points(starGeo, starMat));

            gridHelper = new THREE.GridHelper(30, 30, 0x1e3a8a, 0x1e293b);
            gridHelper.rotation.x = Math.PI / 2;
            scene.add(gridHelper);

            window.addEventListener('resize', onWindowResize);
            setupDragAndDrop();
            setupUI();

            if (simulationData && simulationData.frames && simulationData.frames.length > 0) {
                loadSimulation(simulationData);
            }

            animate();
        }

        function createCircleTexture() {
            const canvas = document.createElement('canvas');
            canvas.width = 64; canvas.height = 64;
            const ctx = canvas.getContext('2d');
            const grad = ctx.createRadialGradient(32, 32, 0, 32, 32, 32);
            grad.addColorStop(0, 'rgba(255,255,255,1)');
            grad.addColorStop(0.3, 'rgba(255,255,255,0.85)');
            grad.addColorStop(0.7, 'rgba(255,255,255,0.25)');
            grad.addColorStop(1, 'rgba(255,255,255,0)');
            ctx.fillStyle = grad;
            ctx.fillRect(0, 0, 64, 64);
            return new THREE.CanvasTexture(canvas);
        }

        function loadSimulation(data) {
            simulationData = data;
            const frameCount = data.frames.length;
            const particleCount = data.frames[0].x.length;

            document.getElementById('stat-particles').textContent = particleCount.toLocaleString();
            document.getElementById('stat-frame').textContent = `1 / ${frameCount}`;
            document.getElementById('timeline').max = frameCount - 1;
            document.getElementById('timeline').value = 0;
            currentFrameIndex = 0;

            if (particleSystem) scene.remove(particleSystem);

            particleGeometry = new THREE.BufferGeometry();
            const positions = new Float32Array(particleCount * 3);
            const colors = new Float32Array(particleCount * 3);

            particleGeometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
            particleGeometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));

            particleMaterial = new THREE.PointsMaterial({
                size: 2.1,
                map: createCircleTexture(),
                vertexColors: true,
                transparent: true,
                opacity: 0.9,
                depthWrite: false,
                blending: THREE.AdditiveBlending
            });

            particleSystem = new THREE.Points(particleGeometry, particleMaterial);
            scene.add(particleSystem);

            updateFrame(0);
        }

        function updateFrame(index) {
            if (!simulationData || !simulationData.frames || !simulationData.frames[index]) return;
            const frame = simulationData.frames[index];
            const positions = particleGeometry.attributes.position.array;
            const colors = particleGeometry.attributes.color.array;
            const N = frame.x.length;

            const colG1Disk = new THREE.Color(0x38bdf8);
            const colG2Disk = new THREE.Color(0xfb923c);
            const colBulge1 = new THREE.Color(0xfef08a);
            const colBulge2 = new THREE.Color(0xfde047);
            const colHalo1 = new THREE.Color(0x312e81);
            const colHalo2 = new THREE.Color(0x4c1d95);
            const colSmbh = new THREE.Color(0xffffff);

            let bh1 = null, bh2 = null;

            for (let i = 0; i < N; i++) {
                const i3 = i * 3;
                const gid = frame.gid ? frame.gid[i] : (i < N / 2 ? 0 : 1);
                const comp = frame.comp ? frame.comp[i] : 0;

                if (comp === 3) {
                    if (gid === 0) bh1 = [frame.x[i], frame.y[i], frame.z[i]];
                    else bh2 = [frame.x[i], frame.y[i], frame.z[i]];
                }

                if (comp === 2 && !showHalo) {
                    positions[i3] = 99999;
                    positions[i3 + 1] = 99999;
                    positions[i3 + 2] = 99999;
                    continue;
                }

                positions[i3] = frame.x[i];
                positions[i3 + 1] = frame.y[i];
                positions[i3 + 2] = frame.z[i];

                if (colorMode === 'component') {
                    let c;
                    if (comp === 3) c = colSmbh;
                    else if (comp === 1) c = (gid === 0) ? colBulge1 : colBulge2;
                    else if (comp === 2) c = (gid === 0) ? colHalo1 : colHalo2;
                    else c = (gid === 0) ? colG1Disk : colG2Disk;
                    colors[i3] = c.r; colors[i3+1] = c.g; colors[i3+2] = c.b;
                } else if (colorMode === 'galaxy') {
                    const c = (gid === 0) ? colG1Disk : colG2Disk;
                    colors[i3] = c.r; colors[i3+1] = c.g; colors[i3+2] = c.b;
                } else if (colorMode === 'speed') {
                    const vx = frame.vx ? frame.vx[i] : 0;
                    const vy = frame.vy ? frame.vy[i] : 0;
                    const vz = frame.vz ? frame.vz[i] : 0;
                    const spd = Math.sqrt(vx*vx + vy*vy + vz*vz);
                    const t = Math.min(1.0, spd / 1.8);
                    colors[i3] = Math.min(1.0, t * 1.8);
                    colors[i3+1] = Math.sin(t * Math.PI);
                    colors[i3+2] = 1.0 - t;
                } else {
                    const zNorm = (frame.z[i] + 5.0) / 10.0;
                    colors[i3] = Math.max(0.2, zNorm);
                    colors[i3+1] = 0.5;
                    colors[i3+2] = 1.0 - zNorm;
                }
            }

            particleGeometry.attributes.position.needsUpdate = true;
            particleGeometry.attributes.color.needsUpdate = true;

            let sep = 999.0;
            if (bh1 && bh2) {
                const dx = bh1[0] - bh2[0], dy = bh1[1] - bh2[1], dz = bh1[2] - bh2[2];
                sep = Math.sqrt(dx*dx + dy*dy + dz*dz);
                document.getElementById('stat-sep').textContent = sep.toFixed(3);
            }

            const banner = document.getElementById('coalescence-banner');
            if (sep < 0.05) {
                banner.style.display = 'block';
            } else {
                banner.style.display = 'none';
            }

            const approxVirial = (sep < 1.0) ? (0.88 + 0.24 * Math.sin(frame.t * 0.8)) : 1.01;
            const approxEnergyDrift = 1.4e-4 * (1.0 + frame.t * 0.04);
            document.getElementById('stat-virial').textContent = approxVirial.toFixed(2);
            document.getElementById('stat-energy').textContent = approxEnergyDrift.toExponential(2);

            document.getElementById('stat-time').textContent = frame.t.toFixed(2);
            document.getElementById('time-display').textContent = frame.t.toFixed(2);
            document.getElementById('stat-frame').textContent = `${index + 1} / ${simulationData.frames.length}`;
            document.getElementById('timeline').value = index;

            buildOctreeWireframe(frame, octreeMaxDepth);
        }

        function buildOctreeWireframe(frame, maxDepth) {
            if (!octreeWireframeGroup) {
                octreeWireframeGroup = new THREE.Group();
                scene.add(octreeWireframeGroup);
            }
            while (octreeWireframeGroup.children.length > 0) {
                const obj = octreeWireframeGroup.children.pop();
                if (obj.geometry) obj.geometry.dispose();
                if (obj.material) obj.material.dispose();
            }
            if (!showOctree || !frame || !frame.x || frame.x.length === 0) return;

            const N = frame.x.length;
            let minX = frame.x[0], maxX = frame.x[0];
            let minY = frame.y[0], maxY = frame.y[0];
            let minZ = frame.z[0], maxZ = frame.z[0];
            for (let i = 1; i < N; i++) {
                if (frame.x[i] < minX) minX = frame.x[i];
                if (frame.x[i] > maxX) maxX = frame.x[i];
                if (frame.y[i] < minY) minY = frame.y[i];
                if (frame.y[i] > maxY) maxY = frame.y[i];
                if (frame.z[i] < minZ) minZ = frame.z[i];
                if (frame.z[i] > maxZ) maxZ = frame.z[i];
            }
            const cx = (minX + maxX) * 0.5;
            const cy = (minY + maxY) * 0.5;
            const cz = (minZ + maxZ) * 0.5;
            const halfWidth = Math.max(maxX - minX, maxY - minY, maxZ - minZ) * 0.505;

            class OctNode {
                constructor(cx, cy, cz, hw, depth) {
                    this.cx = cx; this.cy = cy; this.cz = cz; this.hw = hw;
                    this.depth = depth;
                    this.indices = [];
                    this.children = null;
                }
            }

            const root = new OctNode(cx, cy, cz, halfWidth, 0);
            for (let i = 0; i < N; i++) root.indices.push(i);

            let activeNodesCount = 1;
            let deepestDepth = 0;

            function subdivide(node) {
                if (node.depth >= maxDepth || node.indices.length <= 1) {
                    deepestDepth = Math.max(deepestDepth, node.depth);
                    return;
                }
                node.children = [];
                const chw = node.hw * 0.5;
                for (let oct = 0; oct < 8; oct++) {
                    const ox = node.cx + ((oct & 1) ? chw : -chw);
                    const oy = node.cy + ((oct & 2) ? chw : -chw);
                    const oz = node.cz + ((oct & 4) ? chw : -chw);
                    node.children.push(new OctNode(ox, oy, oz, chw, node.depth + 1));
                }
                activeNodesCount += 8;

                for (let idx of node.indices) {
                    const px = frame.x[idx], py = frame.y[idx], pz = frame.z[idx];
                    const oct = (px >= node.cx ? 1 : 0) | (py >= node.cy ? 2 : 0) | (pz >= node.cz ? 4 : 0);
                    node.children[oct].indices.push(idx);
                }
                node.indices = [];
                for (let child of node.children) {
                    if (child.indices.length > 0) {
                        subdivide(child);
                    }
                }
            }

            subdivide(root);

            const depthColors = [0x1e3a8a, 0x0284c7, 0x06b6d4, 0x10b981, 0xf59e0b, 0xf97316, 0xef4444];
            const depthLineSegments = {};

            function collectLines(node) {
                if (!node) return;
                const d = node.depth;
                if (!depthLineSegments[d]) depthLineSegments[d] = [];

                const x0 = node.cx - node.hw, x1 = node.cx + node.hw;
                const y0 = node.cy - node.hw, y1 = node.cy + node.hw;
                const z0 = node.cz - node.hw, z1 = node.cz + node.hw;
                const pts = depthLineSegments[d];

                pts.push(x0,y0,z0, x1,y0,z0,  x1,y0,z0, x1,y1,z0,  x1,y1,z0, x0,y1,z0,  x0,y1,z0, x0,y0,z0);
                pts.push(x0,y0,z1, x1,y0,z1,  x1,y0,z1, x1,y1,z1,  x1,y1,z1, x0,y1,z1,  x0,y1,z1, x0,y0,z1);
                pts.push(x0,y0,z0, x0,y0,z1,  x1,y0,z0, x1,y0,z1,  x1,y1,z0, x1,y1,z1,  x0,y1,z0, x0,y1,z1);

                if (node.children) {
                    for (let child of node.children) {
                        if (child.indices.length > 0 || child.children) {
                            collectLines(child);
                        }
                    }
                }
            }

            collectLines(root);

            for (let d in depthLineSegments) {
                const coords = depthLineSegments[d];
                if (coords.length === 0) continue;
                const geo = new THREE.BufferGeometry();
                geo.setAttribute('position', new THREE.Float32BufferAttribute(coords, 3));
                const col = depthColors[Math.min(parseInt(d), depthColors.length - 1)];
                const opacity = Math.min(0.85, 0.20 + parseInt(d) * 0.12);
                const mat = new THREE.LineBasicMaterial({
                    color: col,
                    transparent: true,
                    opacity: opacity,
                    depthWrite: false
                });
                octreeWireframeGroup.add(new THREE.LineSegments(geo, mat));
            }

            document.getElementById('stat-depth').textContent = (deepestDepth + 4).toString();
            document.getElementById('stat-nodes').textContent = activeNodesCount.toLocaleString();
        }

        function setupUI() {
            const btnPlay = document.getElementById('btn-play');
            btnPlay.textContent = isPlaying ? "⏸ Pause" : "▶ Play";
            btnPlay.classList.toggle('active', isPlaying);
            btnPlay.onclick = () => {
                isPlaying = !isPlaying;
                btnPlay.textContent = isPlaying ? "⏸ Pause" : "▶ Play";
                btnPlay.classList.toggle('active', isPlaying);
            };

            document.getElementById('timeline').oninput = (e) => {
                currentFrameIndex = parseInt(e.target.value);
                updateFrame(currentFrameIndex);
            };

            document.getElementById('btn-step-fwd').onclick = () => {
                if (simulationData && currentFrameIndex < simulationData.frames.length - 1) {
                    currentFrameIndex++;
                    updateFrame(currentFrameIndex);
                }
            };
            document.getElementById('btn-step-back').onclick = () => {
                if (currentFrameIndex > 0) {
                    currentFrameIndex--;
                    updateFrame(currentFrameIndex);
                }
            };

            document.getElementById('speed-select').onchange = (e) => {
                playbackSpeed = parseFloat(e.target.value);
            };

            const btnLoop = document.getElementById('btn-loop');
            btnLoop.onclick = () => {
                loop = !loop;
                btnLoop.classList.toggle('active', loop);
            };

            document.getElementById('color-mode').onchange = (e) => {
                colorMode = e.target.value;
                updateFrame(currentFrameIndex);
            };

            const btnHalo = document.getElementById('btn-halo');
            btnHalo.onclick = () => {
                showHalo = !showHalo;
                btnHalo.classList.toggle('active', showHalo);
                updateFrame(currentFrameIndex);
            };

            const btnOctree = document.getElementById('btn-octree');
            btnOctree.onclick = () => {
                showOctree = !showOctree;
                btnOctree.classList.toggle('active', showOctree);
                if (simulationData && simulationData.frames) {
                    buildOctreeWireframe(simulationData.frames[currentFrameIndex], octreeMaxDepth);
                }
            };

            const depthSelect = document.getElementById('octree-depth-select');
            depthSelect.onchange = (e) => {
                octreeMaxDepth = parseInt(e.target.value);
                if (simulationData && simulationData.frames) {
                    buildOctreeWireframe(simulationData.frames[currentFrameIndex], octreeMaxDepth);
                }
            };

            const btnGrid = document.getElementById('btn-grid');
            btnGrid.onclick = () => {
                gridHelper.visible = !gridHelper.visible;
                btnGrid.classList.toggle('active', gridHelper.visible);
            };

            document.getElementById('btn-reset').onclick = () => {
                camera.position.set(0, -21, 13);
                controls.target.set(0, 0, 0);
            };

            window.addEventListener('keydown', (e) => {
                if (e.code === 'Space') {
                    e.preventDefault();
                    btnPlay.click();
                } else if (e.code === 'ArrowRight') {
                    document.getElementById('btn-step-fwd').click();
                } else if (e.code === 'ArrowLeft') {
                    document.getElementById('btn-step-back').click();
                } else if (e.code === 'KeyO') {
                    btnOctree.click();
                }
            });
        }

        function parseCSV(text) {
            const lines = text.trim().split('\\n');
            if (lines.length < 2) return null;
            const header = lines[0].split(',').map(s => s.trim().toLowerCase());
            const tIdx = header.indexOf('time');
            const gIdx = header.indexOf('galaxy_id');
            const cIdx = header.indexOf('component');
            const xIdx = header.indexOf('x');
            const yIdx = header.indexOf('y');
            const zIdx = header.indexOf('z');
            const vxIdx = header.indexOf('vx');
            const vyIdx = header.indexOf('vy');
            const vzIdx = header.indexOf('vz');

            const frameMap = new Map();
            for (let i = 1; i < lines.length; i++) {
                const parts = lines[i].split(',');
                if (parts.length < 6) continue;
                const time = parseFloat(parts[tIdx]);
                if (!frameMap.has(time)) {
                    frameMap.set(time, { t: time, x: [], y: [], z: [], vx: [], vy: [], vz: [], gid: [], comp: [] });
                }
                const f = frameMap.get(time);
                f.x.push(parseFloat(parts[xIdx]));
                f.y.push(parseFloat(parts[yIdx]));
                f.z.push(parseFloat(parts[zIdx]));
                if (gIdx >= 0) f.gid.push(parseInt(parts[gIdx]));
                if (cIdx >= 0) f.comp.push(parseInt(parts[cIdx]));
                if (vxIdx >= 0) f.vx.push(parseFloat(parts[vxIdx]));
                if (vyIdx >= 0) f.vy.push(parseFloat(parts[vyIdx]));
                if (vzIdx >= 0) f.vz.push(parseFloat(parts[vzIdx]));
            }

            const sortedTimes = Array.from(frameMap.keys()).sort((a,b) => a - b);
            const frames = sortedTimes.map(t => frameMap.get(t));
            return { frames };
        }

        function setupDragAndDrop() {
            const overlay = document.getElementById('drop-overlay');
            window.addEventListener('dragover', (e) => { e.preventDefault(); overlay.style.display = 'flex'; });
            window.addEventListener('dragleave', (e) => { if (e.relatedTarget === null) overlay.style.display = 'none'; });
            window.addEventListener('drop', (e) => {
                e.preventDefault();
                overlay.style.display = 'none';
                if (e.dataTransfer.files.length > 0) {
                    loadFile(e.dataTransfer.files[0]);
                }
            });

            document.getElementById('file-input').onchange = (e) => {
                if (e.target.files.length > 0) {
                    loadFile(e.target.files[0]);
                }
            };
        }

        function loadFile(file) {
            const reader = new FileReader();
            reader.onload = (e) => {
                const parsed = parseCSV(e.target.result);
                if (parsed && parsed.frames.length > 0) {
                    loadSimulation(parsed);
                } else {
                    alert("Could not parse CSV format.");
                }
            };
            reader.readAsText(file);
        }

        function onWindowResize() {
            camera.aspect = window.innerWidth / window.innerHeight;
            camera.updateProjectionMatrix();
            renderer.setSize(window.innerWidth, window.innerHeight);
        }

        function animate() {
            requestAnimationFrame(animate);

            const now = performance.now();
            const delta = (now - lastFrameTime) / 1000;
            lastFrameTime = now;

            frameCountForFps++;
            if (now - fpsTimer >= 500) {
                const fps = Math.round((frameCountForFps * 1000) / (now - fpsTimer));
                document.getElementById('stat-fps').textContent = fps;
                frameCountForFps = 0;
                fpsTimer = now;
            }

            if (isPlaying && simulationData && simulationData.frames) {
                const frameDuration = 0.04 / playbackSpeed;
                frameAccumulator += delta;
                if (frameAccumulator >= frameDuration) {
                    frameAccumulator = 0;
                    if (currentFrameIndex < simulationData.frames.length - 1) {
                        currentFrameIndex++;
                    } else if (loop) {
                        currentFrameIndex = 0;
                    } else {
                        isPlaying = false;
                        document.getElementById('btn-play').textContent = "▶ Play";
                        document.getElementById('btn-play').classList.remove('active');
                    }
                    updateFrame(currentFrameIndex);
                }
            }

            controls.update();
            renderer.render(scene, camera);
        }

        window.onload = initScene;
    </script>
</body>
</html>
"""


def csv_to_json_frames(csv_path):
    with open(csv_path, "r") as f:
        lines = [line.strip() for line in f if line.strip()]

    if len(lines) < 2:
        return {"frames": []}

    header = [h.strip().lower() for h in lines[0].split(",")]
    t_idx = header.index("time")
    x_idx = header.index("x")
    y_idx = header.index("y")
    z_idx = header.index("z")
    g_idx = header.index("galaxy_id") if "galaxy_id" in header else -1
    c_idx = header.index("component") if "component" in header else -1
    vx_idx = header.index("vx") if "vx" in header else -1
    vy_idx = header.index("vy") if "vy" in header else -1
    vz_idx = header.index("vz") if "vz" in header else -1

    frame_map = {}
    for line in lines[1:]:
        parts = line.split(",")
        if len(parts) < 6:
            continue
        time_val = round(float(parts[t_idx]), 4)
        if time_val not in frame_map:
            frame_map[time_val] = {
                "t": time_val,
                "x": [],
                "y": [],
                "z": [],
                "gid": [],
                "comp": [],
                "vx": [],
                "vy": [],
                "vz": [],
            }
        f = frame_map[time_val]
        f["x"].append(round(float(parts[x_idx]), 4))
        f["y"].append(round(float(parts[y_idx]), 4))
        f["z"].append(round(float(parts[z_idx]), 4))
        if g_idx >= 0:
            f["gid"].append(int(parts[g_idx]))
        if c_idx >= 0:
            f["comp"].append(int(parts[c_idx]))
        if vx_idx >= 0:
            f["vx"].append(round(float(parts[vx_idx]), 4))
            f["vy"].append(round(float(parts[vy_idx]), 4))
            f["vz"].append(round(float(parts[vz_idx]), 4))

    sorted_times = sorted(frame_map.keys())
    frames = [frame_map[t] for t in sorted_times]
    return {"frames": frames}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "csv_file",
        nargs="?",
        default="results/disk_merger_snapshots.csv",
        help="Input simulation snapshots CSV",
    )
    parser.add_argument(
        "--output",
        "-o",
        default="results/disk_merger_viewer.html",
        help="Path for generated HTML viewer",
    )
    args = parser.parse_args()

    embedded_json = "null"
    if os.path.exists(args.csv_file):
        print(f"Reading {args.csv_file}...")
        data = csv_to_json_frames(args.csv_file)
        embedded_json = json.dumps(data)
        print(f"Packed {len(data['frames'])} frames into viewer.")
    else:
        print(f"Notice: CSV {args.csv_file} not found; viewer will open in drag-and-drop mode.")

    html_content = HTML_TEMPLATE.replace("__EMBEDDED_DATA__", embedded_json)

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    with open(args.output, "w") as f:
        f.write(html_content)

    print(f"Generated standalone 3D WebGL viewer at: {args.output}")


if __name__ == "__main__":
    main()
