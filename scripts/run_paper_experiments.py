#!/usr/bin/env python3
"""
Master Reproducibility Pipeline for Barnes-Hut N-Body Research Paper.

Executes the entire research workflow with a single command:
  1. Compiles optimized modern C++20 simulation binaries via Make.
  2. Executes algorithmic scaling benchmarks (Direct O(N^2) vs Barnes-Hut O(N log N) across N in [512, 16384]).
  3. Executes controlled parameter sweep: Prograde (i=0°) vs Retrograde (i=180°) galactic collisions.
  4. Runs Symplectic Leapfrog vs RK4 vs Euler stability validation test.
  5. Exports interactive 3D WebGL viewers with live Octree Wireframe and Telemetry HUD.
  6. Renders publication-grade 300 DPI figures and vector PDFs.
"""

import os
import subprocess
import sys
import time

BINARY_NAME = "build/barnes_hut_simulator"


def log_banner(msg):
    print("\n" + "=" * 90)
    print(f"  {msg}")
    print("=" * 90 + "\n")


def run_command(cmd, desc):
    print(f">> [RUNNING] {desc}")
    print(f"   $ {cmd}")
    start = time.time()
    res = subprocess.run(cmd, shell=True)
    duration = time.time() - start
    if res.returncode != 0:
        print(f"!! [FAILED] {desc} (Exited with code {res.returncode})")
        sys.exit(res.returncode)
    print(f">> [DONE] {desc} in {duration:.2f}s\n")


def main():
    log_banner("BARNES-HUT N-BODY SIMULATOR: REPRODUCIBLE RESEARCH EXPERIMENT PIPELINE")

    # 1. Compile binaries
    run_command("make clean && make", "Compiling high-performance C++20 simulation binaries")

    # 2. Benchmark Scaling (N up to 16384)
    run_command(f"./{BINARY_NAME} --benchmark --min-n 512 --max-n 16384 --output results/scaling_benchmark.csv",
                "Algorithmic Complexity & Scaling Benchmark Suite (O(N^2) Direct vs O(N log N) Barnes-Hut)")

    # 3. Controlled Prograde vs Retrograde Parameter Sweep
    run_command(f"./{BINARY_NAME} --merger-sweep --particles 2048 --steps 1800 --snapshot-every 15",
                "Controlled Prograde vs Retrograde Galactic Merger Parameter Sweep")

    # 4. Symplectic Integrator Comparison
    run_command(f"./{BINARY_NAME} --integrator-test --periods 30 --output results/integrator_comparison.csv",
                "Symplectic Leapfrog vs RK4 vs Euler Long-Term Orbital Energy Conservation Test")

    # 5. Generate Standalone 3D WebGL Viewers
    run_command("python3 scripts/export_web_viewer.py results/merger_prograde_snapshots.csv --output results/galaxy_viewer.html",
                "Exporting 3D Interactive WebGL Viewer (Prograde Merger with Live Octree Wireframe)")
    run_command("python3 scripts/export_web_viewer.py results/merger_retrograde_snapshots.csv --output results/merger_retrograde_viewer.html",
                "Exporting 3D Interactive WebGL Viewer (Retrograde Merger)")

    # 6. Render Publication Figures
    run_command("python3 scripts/plot_paper_figures.py",
                "Rendering Publication-Grade Plots (PNG @ 300 DPI & Vector PDF)")

    log_banner("ALL RESEARCH EXPERIMENTS SUCCESSFULLY EXECUTED")
    print("Generated Artifacts:")
    print("  - Data CSVs:       results/scaling_benchmark.csv")
    print("                     results/merger_sweep_telemetry.csv")
    print("                     results/merger_octree_boxes.csv")
    print("                     results/integrator_comparison.csv")
    print("  - WebGL Viewers:   results/galaxy_viewer.html (with 3D Octree Wireframe & Telemetry HUD)")
    print("                     results/merger_retrograde_viewer.html")
    print("  - Paper Figures:   results/fig1_computational_scaling.[png|pdf]")
    print("                     results/fig2_force_error_pareto.[png|pdf]")
    print("                     results/fig3_prograde_vs_retrograde_merger.[png|pdf]")
    print("                     results/fig4_symplectic_energy_conservation.[png|pdf]")
    print("\nReady for paper inclusion and academic submission!")


if __name__ == '__main__':
    main()
