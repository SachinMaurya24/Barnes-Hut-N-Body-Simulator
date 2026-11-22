# Barnes–Hut N-Body Simulator: Codebase Architecture & Explanation Guide

This document provides a comprehensive, file-by-file technical breakdown of the entire codebase, its mathematical and astrophysical foundations, and key talking points to confidently explain how the simulator works.

---

## 1. Executive Summary (The Elevator Pitch)

> **"This project is a high-performance $\mathcal{O}(N \log N)$ 3D N-body gravitational simulator written in modern C++20 using the Barnes–Hut hierarchical octree algorithm with dynamic multithreading. It models full galactic dynamics—from isotropic Plummer star clusters to multi-component spiral galaxy mergers featuring exponential stellar disks, Hernquist bulges, live dark matter halos, and central supermassive black holes. Integration uses a time-reversible, symplectic Kick-Drift-Kick Leapfrog scheme to guarantee long-term energy conservation, complemented by an interactive WebGL 3D viewer and hardware-accelerated rendering pipelines."**

---

## 2. System Architecture Overview

```
+--------------------------------------------------------------------------+
|                               src/main.cpp                               |
|                (CLI Parser, Simulation Orchestrator, Telemetry)          |
+--------------------------------------------------------------------------+
       |                                                    |
       v                                                    v
+-------------------------------+             +----------------------------+
|     Astrophysical Models      |             |       Physics Engine       |
|  - disk.h (Spiral Galaxy)     |             |  - octree.h (Barnes-Hut)   |
|  - hernquist.h (Bulge/Halo)   |             |  - gravity.h (Direct Sum)  |
|  - plummer.h (Star Cluster)   |             |  - leapfrog.h (Symplectic) |
|  - bodies.h (SoA Data Layout) |             |  - energy.h (Diagnostics)  |
+-------------------------------+             |  - parallel.h (ThreadPool) |
                                              |  - vec3.h (Vector Math)    |
                                              +----------------------------+
                                                            |
                                                            v
+--------------------------------------------------------------------------+
|                               Output Data                                |
|        (CSV Snapshot Logs: positions, velocities, components, time)       |
+--------------------------------------------------------------------------+
       |                                                    |
       v                                                    v
+-------------------------------+             +----------------------------+
|      WebGL 3D Viewer          |             |       Video Showcase       |
|  - scripts/export_web_viewer  |             | - scripts/plot_spiral_...  |
|  - results/*.html             |             | - CoreGraphics / FFmpeg    |
+-------------------------------+             +----------------------------+
```

---

## 3. Core Physics & Math Engine (`src/`)

### `src/vec3.h`
* **Language & Paradigm:** Modern C++20 header-only struct, `constexpr` vector math.
* **Role:** Fundamental 3D Euclidean vector `(x, y, z)`.
* **Details:**
  * Implements arithmetic operators (`+`, `-`, `*`, `/`, `+=`) and scalar commutativity.
  * Marked `constexpr` to allow compile-time evaluation where inputs are known.
  * Free of virtual methods or pointer overhead, ensuring SIMD register compatibility.

### `src/bodies.h`
* **Language & Paradigm:** C++20 Data-Oriented Design (DOD) structure & `enum class`.
* **Role:** Stores all particle states and defines galactic component types (`Disk = 0`, `Bulge = 1`, `Halo = 2`, `BlackHole = 3`).
* **Key Architecture Choice (Structure of Arrays vs Array of Structures):**
  * Rather than storing an array of objects (`struct Particle { Vec3 pos, vel, acc; double m; }; std::vector<Particle>`), it uses **Structure of Arrays (SoA)**:
    * `std::vector<Vec3> positions`
    * `std::vector<Vec3> velocities`
    * `std::vector<Vec3> accelerations`
    * `std::vector<double> masses`
    * `std::vector<int> components`
  * **Why:** Drastically improves **CPU L1/L2 cache locality**. When calculating forces or updating positions, the CPU memory controller streams contiguous floats without loading unused velocity or mass data into the cache line.

### `src/octree.h`
* **Language & Paradigm:** Spatial indexing algorithm, C++20 class (~290 lines).
* **Role:** Implements the **Barnes–Hut hierarchical spatial decomposition**, bringing gravitational force evaluation from $\mathcal{O}(N^2)$ down to $\mathcal{O}(N \log N)$.
* **Internal Mechanics:**
  1. Computes the axis-aligned bounding box enclosing all particles.
  2. Recursively subdivides space into 8 octants until each leaf contains at most one particle (or hits maximum depth 64).
  3. Computes the total mass and Center of Mass (CoM) for every internal node bottom-up.
  4. During force calculation for particle $i$, traverses the tree. If a node has width $s$ and distance $d$ satisfying the **Multipole Acceptance Criterion (MAC)**:
     $$\frac{s}{d} < \theta$$
     (where $\theta \approx 0.5$), the entire subtree is approximated as a single point mass at its center of mass.
* **Engineering Optimizations:**
  * **Contiguous Vector Nodes:** Stored in a flat `std::vector<Node>` with integer indices instead of dynamic heap pointers (`new`/`delete`), eliminating memory fragmentation.
  * **Iterative Stack Traversal:** Uses a fixed-size `std::array<StackEntry, 256>` rather than recursion, eliminating call-stack overhead and compiler frame management.
  * **Squared MAC Comparison:** Compares `(cell_width * cell_width) < (theta * theta * distance_squared)`, completely avoiding expensive `std::sqrt()` calls during node acceptance checks.

### `src/gravity.h`
* **Language & Paradigm:** C++20 direct all-pairs parallel solver.
* **Role:** Evaluates Newton's Universal Law of Gravitation directly using Plummer softening:
  $$\vec{a}_i = \sum_{j \ne i} \frac{G \cdot m_j \cdot (\vec{r}_j - \vec{r}_i)}{(|\vec{r}_j - \vec{r}_i|^2 + \epsilon^2)^{3/2}}$$
* **Purpose:** Serves as the **exact $\mathcal{O}(N^2)$ ground-truth reference** to benchmark and validate Barnes–Hut accuracy across multiple opening angles ($\theta = 0.2, 0.5, 0.8$). Softening length $\epsilon$ prevents non-physical infinite accelerations during close stellar encounters.

### `src/parallel.h`
* **Language & Paradigm:** Persistent multithreaded thread pool utilizing modern C++20 concurrency primitives.
* **Role:** Distributes particle force and potential calculations across all available hardware threads (`std::thread::hardware_concurrency()`).
* **Technical Highlights:**
  * **Persistent Workers:** Reusable worker threads avoid the costly OS overhead of repeatedly creating and joining threads every simulation timestep.
  * **C++20 `std::latch` Synchronization:** Uses lightweight countdown latches to synchronize worker barrier completion without heavy mutex contention.
  * **Chunk-Based Work-Stealing:** Uses `std::atomic<std::size_t> next_index` with chunk size of 16 particles. Threads dynamically claim chunks, ensuring optimal load balancing even when different spatial regions require unequal octree traversal depths.

### `src/leapfrog.h`
* **Language & Paradigm:** Numerical integration engine.
* **Role:** Integrates equations of motion using the **Kick-Drift-Kick (KDK) Leapfrog scheme**:
  1. **Kick:** $\vec{v}\left(t + \frac{\Delta t}{2}\right) = \vec{v}(t) + \vec{a}(t) \cdot \frac{\Delta t}{2}$
  2. **Drift:** $\vec{r}(t + \Delta t) = \vec{r}(t) + \vec{v}\left(t + \frac{\Delta t}{2}\right) \cdot \Delta t$
  3. Recompute gravitational field $\vec{a}(t + \Delta t)$ using Barnes–Hut or direct summation.
  4. **Kick:** $\vec{v}(t + \Delta t) = \vec{v}\left(t + \frac{\Delta t}{2}\right) + \vec{a}(t + \Delta t) \cdot \frac{\Delta t}{2}$
* **Key Concept (Symplectic Geometry):** Leapfrog is a **symplectic integrator**—it preserves phase space volume (Liouville's theorem) and is time-reversible. Unlike standard Euler or Runge-Kutta 4 (RK4), it does not artificially inject or dissipate energy, keeping total orbital energy error strictly bounded over long simulation runs.

### `src/energy.h`
* **Language & Paradigm:** Diagnostic and physics validation module.
* **Role:** Computes system-wide kinetic energy ($K = \sum \frac{1}{2} m v^2$), softened gravitational potential energy ($W$), and total energy ($E = K + W$).
* **Metrics Tracked:**
  * Relative energy drift: $|E(t) - E_0| / |E_0|$
  * Virial Ratio: $2K / |W|$ (tests whether a self-gravitating system is in dynamical equilibrium; equal to $1$ for virialized systems).

---

## 4. Astrophysical Galaxy Initial Condition Models (`src/`)

### `src/plummer.h`
* **Role:** Generates an isotropic **Plummer Sphere** (ideal model for globular clusters or dwarf spheroidals).
* **Mathematics:** Implements the Aarseth, Hénon, and Wielen (1974) Monte Carlo method:
  * Inverts the cumulative mass profile: $M(r) = M \frac{r^3}{(r^2 + b^2)^{3/2}}$.
  * Applies von Neumann rejection sampling for particle velocities against the isotropic velocity distribution function up to the local escape velocity:
    $$v_{\text{esc}}(r) = \sqrt{\frac{2 G M}{\sqrt{r^2 + b^2}}}$$
  * Automatically recenters positions and bulk velocities to place the center of mass at rest.

### `src/hernquist.h`
* **Role:** Generates a spherical **Hernquist (1990) galaxy model** with cusp density $\rho(r) = \frac{M a}{2\pi r (r+a)^3}$ (the standard model for elliptical galaxies and bulges).
* **Technical Depth:**
  * Uses numerical **Eddington Inversion** to derive the exact isotropic distribution function $f(E)$.
  * Employs `long double` precision and precomputed 2048-cell cumulative probability tables to accurately sample velocity dispersion even deep inside the central cusp ($r \to 0$).

### `src/disk.h`
* **Role:** Constructs fully dynamic, multi-component **rotating spiral galaxies** (~270 lines).
* **Galaxy Components Built:**
  1. **Exponential Stellar Thin Disk:** Surface density $\Sigma(R) = \Sigma_0 e^{-R/R_d}$ (inverted via Newton-Raphson iteration) with vertical sech$^2\left(\frac{z}{2z_0}\right)$ scale height.
  2. **Hernquist Bulge:** Central spherical stellar concentration.
  3. **Extended Dark Matter Halo:** Enveloping massive halo creating flat outer rotation curves.
  4. **Central Supermassive Black Hole (SMBH):** Heavy sink particle at the galactic nucleus.
* **Astrophysical Rigor:**
  * Calculates circular velocity $v_c(R) = \sqrt{R \cdot |\partial\Phi/\partial R|}$ accounting for all combined gravitational potentials.
  * Computes the epicyclic frequency $\kappa(R)$ and applies the **Toomre Stability Criterion ($Q > 1$)** with asymmetric drift velocity dispersion correction so the cold disk does not collapse into unphysical numerical clumps.
  * Supports full 3D Euler coordinate transformations for orbital inclination ($i$) and argument of periapsis ($\omega$).

---

## 5. Main Driver & CLI (`src/main.cpp`)

### `src/main.cpp`
* **Role:** Central application orchestrator (~1650 lines).
* **Supported Simulation Modes:**
  * `--benchmark`: Stress-test & scaling suite running $\mathcal{O}(N^2)$ direct summation alongside Barnes–Hut ($\theta \in \{0.3, 0.5, 0.7\}$) across $N = 512$ to $16,384+$ particles, logging force error distributions, average node visits, and identifying the crossover point $N_{\rm crit}$.
  * `--merger-sweep`: Controlled parameter sweep comparing Prograde ($i=0^\circ$) vs Retrograde ($i=180^\circ$) collisions with identical mass and velocity, logging core separation $d(t)$, tidal stripping, and SMBH coalescence time.
  * `--integrator-test`: Numerical stability test comparing Symplectic Leapfrog vs RK4 vs Forward Euler over 30 eccentric Keplerian orbits.
  * `--spiral` / `--disk-merger`: Runs the full 4-component spiral galaxy collision, supporting `--prograde`, `--retrograde`, `--inclined`, and octree bounding box export.
  * `--time`, `--dt`, `--particles`, `--separation`: Parameterized Hernquist galactic collision run.
  * `--plummer`: Plummer cluster equilibrium and relaxation test.
  * `--convergence`: Evaluates energy conservation across multiple timesteps ($\Delta t, \Delta t/2, \dots$).
  * *Default Mode*: Deterministic Keplerian two-body orbit validation and Barnes–Hut vs Direct acceleration RMS error comparison.
* **Telemetry:** Formats and streams simulation time, particle coordinates, energy conservation errors, virial ratios, half-mass radii ($r_{10}, r_{50}, r_{90}$), and exports CSV trajectories.

---

## 6. Build System & Tooling

### `CMakeLists.txt`
* **Role:** Modern CMake 3.20+ configuration.
* **Targets:** Builds `barnes_hut_simulator` and `nbody`.
* **Compiler Flags:** Compiles in C++20 mode with high-performance optimizations: `-O3 -march=native -ffast-math -Wall -Wextra` and links OS native threading libraries (`Threads::Threads`).

### `Makefile`
* **Role:** Standalone POSIX/macOS build script.
* **Usage:** Allows building binaries instantly with a simple `make` command without requiring CMake.

---

## 7. Visualization & Analysis Pipeline (`scripts/`)

### `scripts/export_web_viewer.py`
* **Role:** Converts simulation snapshot CSVs into a standalone, single-file interactive **3D WebGL HTML viewer** (`results/galaxy_viewer.html` and `results/disk_merger_viewer.html`).
* **Features:**
  * Client-side 3D rendering with orbit/pan/zoom camera.
  * Timeline playback, scrub bar, and animation speed controls.
  * Color-coded galactic components with dark matter halo visibility toggle.
  * Dynamic SMBH core separation graph showing collision progress in real time.

### `scripts/plot_spiral_showcase.py`
* **Role:** Generates cinematic 3-panel video animations (`results/spiral_galaxy_merger_showcase.mp4`).
* **Layout:**
  * *Left Panel:* Orbital plane ($X$-$Y$) showing tidal tail and bridge ejection.
  * *Top-Right:* 3D inclined edge-on profile ($X$-$Z$) showing disk warps and vertical splash.
  * *Bottom-Right:* Live core separation decay $d(t)$ over time illustrating **Chandrasekhar Dynamical Friction**.
* **Clever Fallback:** If Python Matplotlib/NumPy are unavailable, it automatically compiles an embedded native **macOS CoreGraphics / CoreText C++ renderer** to produce PNG frames directly using native OS frameworks.

### `scripts/plot_galaxy_merger.py`
* **Role:** Python Matplotlib script rendering 2D scatter plots of colliding Hernquist galaxies with trajectory tails.

### `scripts/plot_plummer.py`
* **Role:** Python script visualizing particle distributions and density profiles of the Plummer sphere relaxation.

---

## 8. Output Artifacts & Media (`results/`)

| File / Folder | Description |
| :--- | :--- |
| `results/galaxy_viewer.html` | Standalone interactive 3D WebGL browser viewer for galaxy collisions. |
| `results/disk_merger_viewer.html` | Interactive 3D WebGL viewer specifically formatted for the spiral galaxy merger. |
| `results/spiral_galaxy_merger_showcase.mp4` | 3-panel synchronized video showing tidal bridge formation and SMBH coalescence. |
| `galaxy_merger_long.mp4` | Extended video recording of galaxy collision dynamics. |
| `results/disk_merger_snapshots.csv` | Output telemetry data containing positions, velocities, and component tags for each step. |
| `results/spiral_showcase_frames/` | High-resolution PNG frames generated for video assembly. |
| `Barnes-Hut_Galaxy_Merger_Simulator.pptx` | Presentation slide deck detailing project goals, algorithms, and results. |
| `README.md` | User documentation with build, setup, and CLI command instructions. |

---

## 9. Key Technical Questions & Answers for Defenses

### Q1: Why use the Barnes–Hut algorithm instead of Direct Summation?
> *"Direct summation evaluates gravitational forces across all particle pairs, scaling as $\mathcal{O}(N^2)$. For $100{,}000$ particles, that is $10^{10}$ calculations per timestep, which is intractable for long simulations. Barnes–Hut groups distant clusters into single nodes in an octree if the node size-to-distance ratio is below $\theta \approx 0.5$. This lowers complexity to $\mathcal{O}(N \log N)$ while preserving over 99% force accuracy."*

### Q2: Why choose Leapfrog over Runge–Kutta 4 (RK4)?
> *"Although RK4 has lower local truncation error per step, it is not symplectic—over thousands of orbits, it suffers from secular energy dissipation or artificial pumping. Leapfrog is a time-reversible, symplectic integrator that preserves phase-space volume (Liouville's theorem), ensuring that orbital energy errors remain strictly bounded around zero over long evolutionary timescales."*

### Q3: What prevents the simulated disk galaxies from immediately flying apart or collapsing?
> *"The particles are not placed at random. Disk particle velocities are derived from the gravitational potential's circular velocity curve $v_c(R) = \sqrt{R \cdot |\partial\Phi/\partial R|}$. Furthermore, radial and tangential velocity dispersions are calibrated to satisfy Toomre's stability criterion ($Q > 1$), and the bulge and halo follow self-consistent Hernquist distribution functions calculated via numerical Eddington inversion."*

### Q4: How is high computational performance achieved in C++?
> *"Three architectural decisions:
> 1. **Structure of Arrays (SoA):** Contiguous arrays for positions, velocities, and masses maximize CPU L1/L2 cache line hits.
> 2. **Vector-Backed Octree & Stack Traversal:** The tree uses integer indices in flat memory rather than heap-allocated pointers, avoiding memory fragmentation and recursion overhead.
> 3. **C++20 Parallelism:** A reusable thread pool with chunk-based dynamic work-stealing (`std::atomic`) and `std::latch` synchronization prevents thread creation latency and ensures balanced CPU utilization."*
