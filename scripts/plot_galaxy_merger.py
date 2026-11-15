#!/usr/bin/env python3
"""Render labeled x-y image frames from an illustrative galaxy merger CSV."""

import argparse
import os
import sys

try:
    import matplotlib.pyplot as plt
    import numpy as np
except ImportError as error:
    sys.exit(
        "Plot dependencies are missing. Install them with: "
        "python3 -m pip install numpy matplotlib\n"
        "Import error: {}".format(error)
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "snapshot_csv",
        nargs="?",
        default="results/galaxy_merger_snapshots.csv",
        help="CSV written by --merger mode",
    )
    parser.add_argument(
        "--output",
        default="results/galaxy_merger_xy.png",
        help="final-frame image path",
    )
    parser.add_argument(
        "--frames-dir",
        default="results/galaxy_merger_frames",
        help="directory for one labeled PNG per snapshot (default: %(default)s)",
    )
    parser.add_argument(
        "--frame-count",
        type=int,
        help="render this many frames evenly across the run, interpolating between saved snapshots if needed",
    )
    args = parser.parse_args()

    try:
        data = np.genfromtxt(args.snapshot_csv, delimiter=",", names=True)
    except OSError as error:
        parser.error("could not read snapshot file: {}".format(error))
    if data.size == 0 or data.dtype.names is None:
        parser.error("snapshot file has no particle rows or CSV header")

    data = np.atleast_1d(data)
    required_columns = ("time", "particle_id", "galaxy_id", "x", "y", "z")
    missing_columns = [name for name in required_columns if name not in data.dtype.names]
    if missing_columns:
        parser.error("snapshot file is missing columns: {}".format(", ".join(missing_columns)))
    if args.frame_count is not None and args.frame_count < 1:
        parser.error("--frame-count must be positive")

    times = np.unique(data["time"])
    snapshot_rows = []
    particle_ids = None
    galaxy_ids = None
    for time in times:
        rows = data[data["time"] == time]
        rows = rows[np.argsort(rows["particle_id"])]
        if particle_ids is None:
            particle_ids = rows["particle_id"]
            galaxy_ids = rows["galaxy_id"].astype(int)
        elif rows.size != particle_ids.size or not np.array_equal(rows["particle_id"], particle_ids):
            parser.error("particle IDs must be consistent across snapshots")
        snapshot_rows.append(rows)

    colors = ("tab:blue", "tab:orange")
    extent = max(float(np.max(np.abs(data["x"]))), float(np.max(np.abs(data["y"]))))
    extent = max(extent * 1.05, 1.0)
    frame_dir = args.frames_dir
    os.makedirs(frame_dir, exist_ok=True)

    frame_times = (np.linspace(times[0], times[-1], args.frame_count)
                   if args.frame_count is not None else times)
    for frame_index, time in enumerate(frame_times):
        right_index = int(np.searchsorted(times, time, side="right"))
        if right_index == 0:
            snapshot = snapshot_rows[0]
        elif right_index >= len(times):
            snapshot = snapshot_rows[-1]
        else:
            left_index = right_index - 1
            left_time = times[left_index]
            right_time = times[right_index]
            if time == left_time:
                snapshot = snapshot_rows[left_index]
            else:
                fraction = (time - left_time) / (right_time - left_time)
                left_rows = snapshot_rows[left_index]
                right_rows = snapshot_rows[right_index]
                snapshot = left_rows.copy()
                for coordinate in ("x", "y", "z"):
                    snapshot[coordinate] = (
                        left_rows[coordinate] * (1.0 - fraction)
                        + right_rows[coordinate] * fraction
                    )
        figure, axis = plt.subplots(figsize=(8, 7))
        for index, galaxy_id in enumerate(np.unique(galaxy_ids)):
            particles = snapshot[snapshot["galaxy_id"] == galaxy_id]
            color = colors[index % len(colors)]
            axis.scatter(
                particles["x"], particles["y"], s=12, alpha=0.75,
                color=color, label="Galaxy {}".format(galaxy_id + 1),
            )
        axis.set_xlabel("x (N-body length units)")
        axis.set_ylabel("y (N-body length units)")
        axis.set_xlim(-extent, extent)
        axis.set_ylim(-extent, extent)
        axis.set_aspect("equal", adjustable="box")
        axis.set_title("Galaxy merger at t={:.4g} (N={})".format(time, snapshot.size))
        axis.legend(loc="upper right")
        figure.tight_layout()
        frame_path = os.path.join(frame_dir, "frame_{:04d}.png".format(frame_index))
        figure.savefig(frame_path, dpi=160)
        if frame_index == len(frame_times) - 1:
            figure.savefig(args.output, dpi=160)
        plt.close(figure)

    print("Rendered {} labeled frames to {}".format(len(frame_times), frame_dir))
    print("Final frame: {} particles at t={} to {}".format(
        snapshot_rows[-1].size, times[-1], args.output
    ))


if __name__ == "__main__":
    main()
