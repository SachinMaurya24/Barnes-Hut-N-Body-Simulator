#!/usr/bin/env python3
"""Plot particle positions from a Barnes-Hut Plummer CSV snapshot file."""

import argparse
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
    parser.add_argument("snapshot_csv", help="CSV file written by --plummer mode")
    parser.add_argument(
        "--output", default="plummer_positions.png", help="output image path"
    )
    args = parser.parse_args()

    try:
        data = np.genfromtxt(args.snapshot_csv, delimiter=",", names=True)
    except OSError as error:
        parser.error("could not read snapshot file: {}".format(error))
    if data.size == 0 or data.dtype.names is None:
        parser.error("snapshot file has no particle rows or CSV header")

    required_columns = ("time", "particle_id", "x", "y", "z")
    missing_columns = [name for name in required_columns if name not in data.dtype.names]
    if missing_columns:
        parser.error("snapshot file is missing columns: {}".format(", ".join(missing_columns)))

    data = np.atleast_1d(data)
    final_time = np.max(data["time"])
    final_snapshot = data[data["time"] == final_time]
    figure = plt.figure(figsize=(8, 6))
    axis = figure.add_subplot(111, projection="3d")
    axis.scatter(final_snapshot["x"], final_snapshot["y"], final_snapshot["z"], s=12)
    axis.set_xlabel("x")
    axis.set_ylabel("y")
    axis.set_zlabel("z")
    axis.set_title("Plummer sphere positions at t={:.4g} (N={})".format(
        final_time, final_snapshot.size
    ))
    figure.tight_layout()
    figure.savefig(args.output, dpi=150)
    print("Plotted {} particles at t={} to {}".format(
        final_snapshot.size, final_time, args.output
    ))


if __name__ == "__main__":
    main()
