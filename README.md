# Barnes–Hut N-Body Simulator

Basic C++20 particle data model, direct-summation gravity, a vector-backed 3D octree, and a kick-drift-kick leapfrog integrator. The executable first compares Barnes–Hut acceleration against direct summation on a deterministic 64-particle sample, then runs a deterministic two-body orbit. Per-particle force calculations can use multiple `std::thread` workers.

## Mac prerequisites

- macOS with Apple Command Line Tools or Xcode installed (provides Apple Clang)
- CMake 3.20 or newer

Install the Command Line Tools if needed:

```sh
xcode-select --install
```

Install CMake with Homebrew if it is not already available:

```sh
brew install cmake
```

## Configure and build

Build directly using `make`:

```sh
make
```

Or configure and build with CMake:

```sh
cmake -S . -B build
cmake --build build
```

## Run with your own settings in VS Code

In VS Code, open this project folder with **File → Open Folder…**. Open the integrated terminal with **Terminal → New Terminal** (or press **Ctrl+`**; on macOS, **Control+`**). The terminal should start in the project folder. Build the project, then run the merger with named options:

```sh
cmake -S . -B build
cmake --build build
./build/nbody --time 1 --dt 0.01 --particles 256 --separation 10
```

`--time` sets the total simulated time, `--dt` sets the maximum timestep, `--particles` sets the total particle count (split equally between both galaxies), and `--separation` sets the initial distance between galaxy centers. If the requested time is not divisible by the timestep, the last step is shortened to end exactly at that time. The particle count must be even. This command saves the snapshots to `results/nbody_run_snapshots.csv`; change the destination with `--output`, for example `--output results/my_run.csv`. `--snapshot-every N` optionally saves a snapshot every `N` integration steps. The run also prints regular energy and orbit diagnostics. The named mode uses the existing illustrative Hernquist merger setup, Barnes–Hut forces, default thread count, and default galaxy masses and scales.

Run the default force-comparison and two-body examples with timestep `0.01` and `1000` steps:

```sh
./build/barnes_hut_simulator
```

Pass a timestep and number of steps to configure the run:

```sh
./build/barnes_hut_simulator 0.005 2000
```

At the initial time, every 100 steps, and at the final step, the program prints simulation time, both particle positions, total energy, and relative energy error. Total energy is kinetic energy plus softened pair potential energy, where the softening length matches the force calculation. Relative error is `|E - E0| / |E0|`, measured against the initial total energy. With leapfrog integration, this error should remain bounded and oscillate around a small value for a suitably small timestep; increasing the timestep generally increases the error.

Before the orbit, the example reports the relative RMS acceleration error between the direct solver and the octree solver for opening angles `θ = 0.2`, `0.5`, and `0.8`. A smaller `θ` causes more nodes to be opened and should generally produce a closer match to direct summation.

## Plummer sphere demo

Pass a particle count and random seed to run the deterministic Plummer sphere using N-body units (`G = 1`, total mass `M = 1`). The default run uses timestep `0.005` for 100 steps and writes a CSV snapshot every 10 steps, including the initial and final states:

```sh
./build/barnes_hut_simulator --plummer 128 42
```

Optionally provide a timestep and step count:

```sh
./build/barnes_hut_simulator --plummer 128 42 0.0025 200
```

The optional sixth and seventh arguments set the snapshot interval in steps and the CSV path. For example, write snapshots every 5 steps to `run.csv`:

```sh
./build/barnes_hut_simulator --plummer 128 42 0.0025 200 5 run.csv
```

The generator recenters the sampled positions and velocities so the center of mass and bulk velocity are zero. At each diagnostic interval, the program reports kinetic energy `K`, softened potential energy `W`, total energy `E = K + W`, relative energy error from the initial energy, virial ratio `2K / |W|`, total momentum `(Px, Py, Pz)` and its magnitude, and radii enclosing 10%, 50%, and 90% of the mass (`r10`, `r50`, `r90`). Enclosing radii are measured from the current center of mass. Energy uses direct pair summation even when the simulation force solver is Barnes–Hut.

Both direct and Barnes–Hut per-particle force calculations use dynamically claimed chunks of eight particles. Tree construction and node mass aggregation stay single-threaded. By default, the program uses `std::thread::hardware_concurrency()` (or one thread if the hardware count is unavailable); pass an explicit thread count to compare modes. For the regular example, the optional third argument sets the count:

```sh
./build/barnes_hut_simulator 0.01 1000 1  # single-threaded
./build/barnes_hut_simulator 0.01 1000 4  # four workers
```

For Plummer mode, thread count is the optional eighth argument, after the snapshot path. The next argument selects `direct` or `barnes-hut`; Barnes–Hut also accepts a `THETA` value:

```sh
./build/barnes_hut_simulator --plummer 128 42 0.005 100 10 run.csv 1
./build/barnes_hut_simulator --plummer 128 42 0.005 100 10 run.csv 4
./build/barnes_hut_simulator --plummer 128 42 0.005 100 10 run.csv 4 barnes-hut 0.5
```

The force comparison reports relative RMS differences between single-threaded and threaded direct/tree results. The same target particle always accumulates source forces in the same order, so single-threaded and multithreaded results should agree within floating-point tolerance.

## Plummer validation

Run the deterministic timestep convergence check with defaults (`N=64`, seed `42`, `dt=0.005`, 100 steps, direct forces):

```sh
./build/barnes_hut_simulator --convergence
```

Arguments are `[PARTICLES] [SEED] [TIMESTEP] [STEPS] [direct|barnes-hut] [THREADS] [THETA]`. Both trials start from the same seeded particles and run for the same total time; the second uses `dt/2` and twice as many steps. The reported ratio is the `dt` energy error divided by the `dt/2` error. Leapfrog should approach a ratio of four when timestep error dominates. With Barnes–Hut at nonzero `θ`, force approximation can dominate; use `θ=0` to isolate timestep convergence while retaining the tree traversal solver.

For example, validate direct and exact-tree-force convergence on the same small setup:

```sh
./build/barnes_hut_simulator --convergence 32 42 0.005 40 direct 1
./build/barnes_hut_simulator --convergence 32 42 0.005 40 barnes-hut 1 0
```

Snapshot files are plain CSV with one particle per row and this header:

```text
time,particle_id,x,y,z,vx,vy,vz,mass
```

All rows with the same `time` make one snapshot. Install Python 3, NumPy, and Matplotlib to plot the positions:

```sh
python3 -m pip install numpy matplotlib
python3 scripts/plot_plummer.py plummer_snapshots.csv --output plummer_positions.png
```

The script plots the final snapshot as a 3D scatter plot. The default image name is `plummer_positions.png`.

## Illustrative Hernquist halo merger

Run the two-galaxy approach demo with its default setup:

```sh
./build/barnes_hut_simulator --merger
```

The default has 128 particles per halo, model masses and scale radii of 1, a 20-scale-radius truncation, initial separation 8, relative approach speed 0.45, timestep 0.01, and 600 steps. It is deliberately a modest, illustrative collisionless N-body setup; the short run shows the halos approaching and responding to one another rather than modeling a fully relaxed merger remnant. The values are dimensionless N-body units with `G = 1`; mass and length units are set by the supplied Hernquist parameters and time follows from the dynamical scaling. Each model mass is the untruncated Hernquist mass parameter. Particle masses sum to the retained mass `M(R_t) = M R_t^2/(R_t+a)^2` after the outer cut.

The radius sampler inverts the Hernquist cumulative mass profile `M(<r)/M = r^2/(r+a)^2`, conditioned on the configured truncation radius. Velocities are sampled from the isotropic Hernquist distribution function: at each radius, the speed CDF is tabulated from `p(v|r) ∝ v^2 f(E)` up to escape speed, and velocity directions are isotropic. This uses the analytic isotropic Hernquist DF derived from Eddington inversion ([formula and sampling procedure](https://galaxiesbook.org/chapters/I-04.-Equilibria-of-Collisionless-Stellar-Systems_6-Spherical-distribution-functions.html)). It does not use the Plummer velocity model. The DF is for the untruncated Hernquist potential; the outer cut removes a small fraction of its tail, so isolation validation checks the resulting finite realization before assembly. Each isolated realization is recentered in position and velocity before the two halos are placed in the center-of-mass frame. The approach velocities are mass weighted so the initial total momentum is zero.

Before assembly, each halo is evolved alone with direct forces for five dynamical times. The program reports changes in `r50`, `r90`, and enclosed mass fractions inside `a`, `2a`, and `5a`; it stops before assembly if either profile exceeds the built-in stability tolerances. The two initial halo centers and approach velocities are offset in proportion to their represented (truncated) masses, giving the requested separation and relative speed in the center-of-mass frame. The merger itself defaults to Barnes–Hut forces (`theta=0.5`), while diagnostics always use direct summation for energy. Output labels the solver and reports energy, virial ratio, total momentum, enclosing radii, and the two halo centers' separation.

Merger snapshots are written by default to `results/galaxy_merger_snapshots.csv`. A snapshot is a set of rows sharing the same time, with this header:

```text
time,particle_id,galaxy_id,x,y,z,vx,vy,vz,mass
```

`galaxy_id` is `0` for the first halo and `1` for the second, preserving their origin labels during the encounter. The positional arguments are `[N_PER_GALAXY] [M1] [A1] [M2] [A2] [SEPARATION] [APPROACH_SPEED] [TRUNCATION_IN_SCALE_RADII] [TIMESTEP] [STEPS] [THREADS] [direct|barnes-hut] [THETA] [SEED] [SNAPSHOT_INTERVAL] [CSV_PATH]`. Snapshot interval is in steps. For an easy showcase workflow in the VS Code terminal, open the project folder, choose **Terminal → New Terminal**, then run:

```sh
cmake -S . -B build
cmake --build build
./build/nbody --time 1 --dt 0.01 --particles 256 --separation 8 --snapshot-every 1 --output results/galaxy_merger_showcase_snapshots.csv
python3 scripts/plot_galaxy_merger.py results/galaxy_merger_showcase_snapshots.csv --frames-dir results/galaxy_merger_showcase_frames --output results/galaxy_merger_showcase.png --frame-count 150
"$(brew --prefix)/bin/ffmpeg" -framerate 30 -i results/galaxy_merger_showcase_frames/frame_%04d.png -c:v libx264 -pix_fmt yuv420p results/galaxy_merger.mp4
open results/galaxy_merger.mp4
```

Python 3, NumPy, and Matplotlib are required for plotting (see the install command above). If `command -v ffmpeg` prints nothing, install FFmpeg with:

```sh
brew install ffmpeg
```

The plotter writes one PNG per saved snapshot under `results/galaxy_merger_showcase_frames/`, with the simulation time, particle count, axes, and two halo labels; both halos retain distinct colors and the axes are fixed across all frames. It also saves the final frame as `results/galaxy_merger_showcase.png`. After installing FFmpeg (or if it was already installed), create and open the movie:

```sh
"$(brew --prefix)/bin/ffmpeg" -framerate 30 -i results/galaxy_merger_showcase_frames/frame_%04d.png -c:v libx264 -pix_fmt yuv420p results/galaxy_merger.mp4
open results/galaxy_merger.mp4
```

The simulation writes the initial state, each configured interval, and the final state, using consistent plot axes across frames. `--frame-count 150` renders intermediate positions between saved states as needed, so the 30 fps MP4 lasts at least five seconds without repeating a still frame or extending the simulation time. The merger remains an illustrative short approach/interaction and does not model a fully relaxed remnant.

### Longer MP4 showcase

To create a longer video from a fresh run, build the simulator, simulate 10 time units, save a snapshot every 10 steps, render all snapshots, then encode at 10 frames per second:

```sh
cmake -S . -B build
cmake --build build --target barnes_hut_simulator
./build/barnes_hut_simulator --merger 128 1 1 1 1 8 0.45 20 0.01 1000 4 barnes-hut 0.5 42 10 results/galaxy_merger_long_snapshots.csv
python3 scripts/plot_galaxy_merger.py results/galaxy_merger_long_snapshots.csv --frames-dir results/galaxy_merger_long_frames --output results/galaxy_merger_long.png
"$(brew --prefix)/bin/ffmpeg" -framerate 10 -i results/galaxy_merger_long_frames/frame_%04d.png -c:v libx264 -pix_fmt yuv420p results/galaxy_merger_long.mp4
open results/galaxy_merger_long.mp4
```

If FFmpeg is missing, install it with `brew install ffmpeg`. The simulation duration here is 10 N-body time units, ten times the earlier one-unit showcase. Saving every 10 steps produces 101 frames, including the initial state. At 10 frames per second the video lasts about 10.1 seconds. Simulation time controls how much evolution is shown; snapshot interval controls how many images are made; playback frame rate controls how quickly those images play. For a fixed snapshot interval, increasing simulation time creates more frames and a longer video at the same frame rate. The plotted frames retain the two galaxy colors, fixed axes, and simulation-time title.

For a direct-force comparison, the command is:

```sh
./build/barnes_hut_simulator --merger 128 1 1 1 1 8 0.45 20 0.01 600 4 direct 0.5 42
```

## Multi-Component Rotating Spiral Galaxy Merger (Disk + Bulge + Live DM Halo + SMBH)

Simulate two realistic inclined spiral galaxies, each composed of a 2-arm exponential stellar disk (`component=0`), a Hernquist stellar bulge (`component=1`), an extended live Hernquist dark matter halo (`component=2`) that maintains a flat outer rotation curve and drives Chandrasekhar dynamical friction, and a central Supermassive Black Hole (`component=3`) anchor particle:

```sh
./build/nbody --spiral --particles 2048 --separation 11.0 --impact 2.4 --speed 0.78 --steps 2400 --snapshot-every 15 --output results/disk_merger_snapshots.csv
python3 scripts/plot_spiral_showcase.py results/disk_merger_snapshots.csv --output-video results/spiral_galaxy_merger_showcase.mp4
```

As the two galaxies pass first pericenter (`t ≈ 9.6`), tidal forces strip stars into sweeping spiral tidal tails and an inter-galactic stellar bridge. Overlapping live dark matter halos then brake the galaxies via dynamical friction at apocenter (`t ≈ 14.4`) so they fall back together and coalesce into a single remnant (`t ≈ 21.6–24.0`).

## Interactive 3D WebGL Simulation Viewer

Explore the multi-component simulation in real-time 3D in your web browser with 360° orbit controls, timeline scrubber, astrophysical component coloring (Spiral Disks, Golden Bulges, Dark Matter Halos, and SMBH cores), a Dark Matter Halo visibility toggle (`🌌 DM Halo`), velocity heatmaps, and live SMBH core separation:

```sh
# Generate and open the interactive 3D viewer
python3 scripts/export_web_viewer.py results/disk_merger_snapshots.csv --output results/disk_merger_viewer.html
open results/disk_merger_viewer.html
```

You can also drag-and-drop any `.csv` snapshot file directly onto the viewer window to load and inspect it immediately.

