# 🌌 Barnes–Hut N-Body Gravitational Simulator & Galactic Merger Engine

[![C++20](https://img.shields.io/badge/Language-C%2B%2B20-00599C.svg?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![Algorithm](https://img.shields.io/badge/Algorithm-Barnes--Hut%20%F0%9D%9C%A9(N%20log%20N)-059669.svg?style=flat-square)](https://en.wikipedia.org/wiki/Barnes%E2%80%93Hut_simulation)
[![Integrator](https://img.shields.io/badge/Integrator-Symplectic%20KDK%20Leapfrog-d97706.svg?style=flat-square)](https://en.wikipedia.org/wiki/Leapfrog_integration)
[![Multithreading](https://img.shields.io/badge/Concurrency-std%3A%3Athread%20Work--Stealing-2563eb.svg?style=flat-square)](https://en.cppreference.com/w/cpp/thread)
[![Visualization](https://img.shields.io/badge/Viewer-Interactive%203D%20WebGL-7c3aed.svg?style=flat-square)](https://barnes-hut-n-body-simulator.vercel.app/)
[![License](https://img.shields.io/badge/License-MIT-gray.svg?style=flat-square)](LICENSE)

A high-performance $\mathcal{O}(N \log N)$ 3D gravitational N-body astrophysics simulation engine written in modern C++20. Features a vector-backed hierarchical octree spatial decomposition, dynamic thread-pool multithreading, and time-reversible symplectic Kick–Drift–Kick leapfrog integration. Models collisionless galactic encounters from isotropic Plummer clusters and Hernquist halos to inclined multi-component spiral galaxies (exponential stellar disks, bulges, live dark matter halos, and central supermassive black holes) complete with tidal tail resonance, Chandrasekhar dynamical friction, and nuclear coalescence.

---

## 🎬 Visual Simulation Showcase

### 🌀 Multi-Component Rotating Spiral Galaxy Merger

<p align="center">
  <img src="assets/spiral_galaxy_merger_showcase.gif" alt="Multi-Component Spiral Galaxy Merger Simulation" width="100%" />
</p>

<p align="center">
  <video src="assets/spiral_galaxy_merger_showcase.mp4" controls="controls" width="100%"></video>
</p>

<p align="center">
  <a href="assets/spiral_galaxy_merger_showcase.mp4"><b>▶️ Watch High-Definition MP4 Video (1600x900 @ 24 FPS)</b></a> &nbsp;|&nbsp; 
  <a href="https://barnes-hut-n-body-simulator.vercel.app/"><b>🌐 Launch Interactive 3D WebGL Viewer (Live on Vercel)</b></a>
</p>

Simulates two inclined spiral galaxies undergoing an eccentric collision. Each galaxy comprises four distinct physical components:
1. **Exponential Stellar Thin Disk** ($\Sigma(R) = \Sigma_0 e^{-R/R_d}$) with $\text{sech}^2(z / 2z_0)$ vertical distribution and Toomre $Q > 1$ velocity dispersion.
2. **Hernquist Stellar Bulge** ($\rho(r) = \frac{M_b a}{2\pi r(r+a)^3}$) with isotropic Eddington-inversion velocity distribution.
3. **Live Extended Dark Matter Halo** maintaining flat outer rotation curves and driving Chandrasekhar dynamical friction.
4. **Central Supermassive Black Hole (SMBH)** anchor particle at the galactic nucleus.

<p align="center">
  <img src="assets/spiral_merger_pericenter_tails.png" width="49%" alt="First Pericenter Passage & Tidal Tails" />
  <img src="assets/spiral_merger_coalescence.png" width="49%" alt="Final Core Coalescence & Remnant" />
</p>
<p align="center">
  <em>Left: First pericenter encounter (approx 9.6) producing sweeping resonant tidal tails and an intergalactic stellar bridge. Right: Orbital braking via dynamical friction leading to complete coalescence into an elliptical remnant (approx 22.0).</em>
</p>

---

## 🌐 Interactive 3D WebGL Simulation Viewer

Real-time browser-based 3D visualization engine rendered with Three.js / WebGL. Supports 360° orbital rotation, pan, zoom, timeline scrubber, component filtering, and live telemetry.

<p align="center">
  <img src="assets/webgl_viewer_screenshot.png" alt="Interactive 3D WebGL Simulation Viewer with Dynamic Octree Wireframe and Telemetry HUD" width="100%" />
</p>

### Key Viewer Features:
* **📦 Live Dynamic 3D Octree Wireframe (`Key O` / Toggle Button):** Visualizes the hierarchical Barnes–Hut spatial decomposition in real time. Cells are color-coded by tree depth (cool navy/cyan in the diffuse outer halo $\to$ glowing amber/red around the dense colliding galactic cores).
* **📊 Real-Time Telemetry HUD:** Displays simulation time $t$, SMBH core separation $d(t)$, maximum octree depth, active spatial nodes, virial ratio $2K / |W|$, and relative energy conservation $|\Delta E / E_0|$.
* **🚨 Core Coalescence Alert Banner:** Automatically triggers when nuclear SMBH separation drops below $d(t) < 0.05$.
* **📂 Universal Drag & Drop:** Drag any simulation snapshot CSV file directly into the browser window to visualize it immediately.

```sh
# Open the WebGL viewer in your default browser:
open results/galaxy_viewer.html
```

---

### 🌌 Hernquist Halo Collision Demo (1000 Steps)

<p align="center">
  <img src="assets/galaxy_merger_long.gif" alt="Hernquist Halo Approach and Tidal Interaction" width="70%" />
</p>

<p align="center">
  <a href="assets/galaxy_merger_long.mp4"><b>▶️ Watch Hernquist Halo Merger Video (MP4)</b></a>
</p>

Illustrates two spherical Hernquist halos approaching on a parabolic collision trajectory with isotropic distribution function velocity sampling and Barnes–Hut gravity.

---

## 🛠️ Mac Prerequisites & Build Instructions

### Prerequisites
* macOS with Apple Command Line Tools or Xcode (Apple Clang with C++20 support)
* CMake 3.20+ or Make
* Python 3 with NumPy, Matplotlib, and FFmpeg (for video rendering)

```sh
# Install Command Line Tools (if needed):
xcode-select --install

# Install CMake and FFmpeg via Homebrew:
brew install cmake ffmpeg

# Install Python plotting dependencies:
pip3 install numpy matplotlib
```

### Build with Make (Recommended)
```sh
make
```

### Build with CMake
```sh
cmake -S . -B build
cmake --build build
```

---

## 💻 Running Simulations from the Command Line

### 1. Multi-Component Spiral Galaxy Merger
```sh
./build/nbody --spiral --particles 2048 --separation 11.0 --impact 2.4 --speed 0.78 --steps 2400 --snapshot-every 15 --output results/disk_merger_snapshots.csv

# Render showcase video:
python3 scripts/plot_spiral_showcase.py results/disk_merger_snapshots.csv --output-video results/spiral_galaxy_merger_showcase.mp4
```

### 2. Algorithmic Scaling Benchmark
```sh
./build/barnes_hut_simulator --benchmark --min-n 512 --max-n 16384 --output results/scaling_benchmark.csv
```

### 3. Prograde vs. Retrograde Sweep
```sh
./build/barnes_hut_simulator --merger-sweep --particles 2048 --steps 1800 --snapshot-every 15
```

### 4. Symplectic Integrator Test (30 Orbits)
```sh
./build/barnes_hut_simulator --integrator-test --periods 30
```

### 5. Interactive WebGL Viewer Generation
```sh
python3 scripts/export_web_viewer.py results/merger_prograde_snapshots.csv --output results/galaxy_viewer.html
open results/galaxy_viewer.html
```

### 6. Hernquist Halo Collision
```sh
./build/barnes_hut_simulator --merger 128 1 1 1 1 8 0.45 20 0.01 1000 4 barnes-hut 0.5 42 10 results/galaxy_merger_long_snapshots.csv
```

### 7. Deterministic Plummer Sphere
```sh
./build/barnes_hut_simulator --plummer 128 42 0.005 100 10 results/plummer_run.csv 4 barnes-hut 0.5
```

### 8. Timestep Convergence Verification
```sh
./build/barnes_hut_simulator --convergence 32 42 0.005 40 direct 1
```

---

## 🏛️ Codebase Architecture

```
├── assets/                     # Simulation GIFs, videos, and UI screenshots
│   ├── spiral_galaxy_merger_showcase.gif
│   ├── spiral_galaxy_merger_showcase.mp4
│   ├── spiral_merger_pericenter_tails.png
│   ├── spiral_merger_coalescence.png
│   ├── galaxy_merger_long.gif
│   ├── galaxy_merger_long.mp4
│   └── webgl_viewer_screenshot.png
├── src/                        # High-Performance C++20 Core
│   ├── main.cpp                # CLI orchestrator, simulation modes, diagnostics
│   ├── bodies.h                # Data-Oriented Design (SoA) particle storage
│   ├── octree.h                # Vector-backed hierarchical Barnes-Hut octree
│   ├── gravity.h               # Exact O(N^2) direct all-pairs solver
│   ├── leapfrog.h              # Symplectic Kick-Drift-Kick integrator
│   ├── disk.h                  # Exponential disk + bulge + DM halo initial conditions
│   ├── hernquist.h             # Hernquist halo with numerical Eddington inversion
│   ├── plummer.h               # Aarseth-Hénon-Wielen Plummer sphere generator
│   ├── parallel.h              # Persistent thread pool with dynamic chunk work-stealing
│   ├── energy.h                # Symplectic energy & virial ratio diagnostics
│   └── vec3.h                  # SIMD-friendly constexpr 3D vector math
├── scripts/                    # Visualization & Video Rendering Tools
│   ├── plot_spiral_showcase.py # Galaxy merger video rendering pipeline
│   ├── export_web_viewer.py    # Standalone interactive 3D WebGL viewer generator
│   └── plot_galaxy_merger.py   # Halo collision frame generator
├── PROJECT_EXPLANATION.md      # In-depth architectural & mathematical reference guide
├── Makefile                    # High-level build recipes
└── CMakeLists.txt              # Standard CMake build configuration
```

For full mathematical derivations, velocity distribution function algorithms, and architectural deep-dives, see **[PROJECT_EXPLANATION.md](PROJECT_EXPLANATION.md)**.

---

## 📜 References

1. **Barnes, J., & Hut, P. (1986).** *A hierarchical O(N log N) force-calculation algorithm.* Nature, 324(6096), 446–449.
2. **Toomre, A., & Toomre, J. (1972).** *Galactic Bridges and Tails.* The Astrophysical Journal, 178, 623–666.
3. **Hernquist, L. (1990).** *An analytical model for spherical galaxies and bulges.* The Astrophysical Journal, 356, 359–364.
4. **Chandrasekhar, S. (1943).** *Dynamical Friction. I. General Considerations: the Coefficient of Dynamical Friction.* The Astrophysical Journal, 97, 255.
5. **Hairer, E., Lubich, C., & Wanner, G. (2006).** *Geometric Numerical Integration: Structure-Preserving Algorithms for Ordinary Differential Equations.* Springer.
