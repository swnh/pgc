#!/usr/bin/env python3
"""
Parse *_res.txt files under golden_ref_26-03-04.
Per scene (averaged over frames):
  1) Scatter plot of dx, dy, dz per ring — big points to spot outliers
  2) Grouped bar chart of mode counts per ring — one bar per mode
"""

import os
import glob
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

BASE_DIR = "/home/swnh/pgc/experiments/golden_ref_26-03-04"
OUTPUT_DIR = os.path.join(BASE_DIR, "plots")
os.makedirs(OUTPUT_DIR, exist_ok=True)

COLUMNS = ["ring", "plyIdx", "dx", "dy", "dz", "mode"]


def parse_res_file(filepath):
    return pd.read_csv(
        filepath, sep=r"\s+", comment="#", header=None, names=COLUMNS,
        dtype={"ring": int, "plyIdx": int, "dx": int, "dy": int, "dz": int, "mode": int},
    )


def load_all_data(base_dir):
    scene_data = {}
    scene_dirs = sorted(
        [d for d in os.listdir(base_dir)
         if os.path.isdir(os.path.join(base_dir, d)) and d.startswith("scene-")]
    )
    for scene in scene_dirs:
        res_files = sorted(glob.glob(os.path.join(base_dir, scene, "*_res.txt")))
        if not res_files:
            continue
        frames = []
        for f in res_files:
            try:
                frames.append(parse_res_file(f))
            except Exception as e:
                print(f"[WARN] skip {f}: {e}")
        scene_data[scene] = frames
        print(f"  {scene}: {len(frames)} frames")
    return scene_data


def average_residuals_per_ring(frames):
    combined = pd.concat(frames, ignore_index=True)
    avg = combined.groupby(["ring", "plyIdx"])[["dx", "dy", "dz"]].mean().reset_index()
    return avg


def average_mode_counts_per_ring(frames):
    per_frame = []
    for df in frames:
        counts = df.groupby(["ring", "mode"]).size().reset_index(name="count")
        per_frame.append(counts)
    all_counts = pd.concat(per_frame, ignore_index=True)
    avg_counts = all_counts.groupby(["ring", "mode"])["count"].mean().reset_index()
    return avg_counts


# ── Plot 1: residual scatter ──────────────────────────────────────────────────
def plot_residuals(avg_df, scene_name, out_dir):
    rings = sorted(avg_df["ring"].unique())
    fig, axes = plt.subplots(3, 1, figsize=(16, 12), sharex=True)
    fig.suptitle(f"{scene_name} — dx/dy/dz per ring (avg over frames)", fontsize=14)

    for ax, comp, color in zip(axes, ["dx", "dy", "dz"], ["blue", "green", "red"]):
        for ring in rings:
            vals = avg_df.loc[avg_df["ring"] == ring, comp].values
            x = np.full(len(vals), ring) + np.random.uniform(-0.3, 0.3, len(vals))
            ax.scatter(x, vals, s=8, alpha=0.5, color=color, edgecolors="none")
        ax.set_ylabel(comp, fontsize=12)
        ax.axhline(0, color="grey", lw=0.5, ls="--")
        ax.set_xticks(rings)
        ax.grid(axis="y", alpha=0.3)

    axes[-1].set_xlabel("Ring", fontsize=12)
    plt.tight_layout()
    path = os.path.join(out_dir, f"{scene_name}_residuals.png")
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"    -> {path}")


# ── Plot 2: mode counts — grouped bars ───────────────────────────────────────
def plot_modes(avg_mode_df, scene_name, out_dir):
    rings = sorted(avg_mode_df["ring"].unique())
    modes = sorted(avg_mode_df["mode"].unique())
    n_modes = len(modes)
    bar_width = 0.8 / n_modes

    fig, ax = plt.subplots(figsize=(16, 6))
    fig.suptitle(f"{scene_name} — mode counts per ring (avg over frames)", fontsize=14)

    mode_colors = {0: "red", 1: "blue", 2: "green", 3: "orange"}

    for i, mode in enumerate(modes):
        subset = avg_mode_df[avg_mode_df["mode"] == mode].set_index("ring")
        counts = [subset.loc[r, "count"] if r in subset.index else 0 for r in rings]
        positions = np.array(rings) + (i - n_modes / 2 + 0.5) * bar_width
        bars = ax.bar(positions, counts, width=bar_width,
                      label=f"mode {mode}", color=mode_colors.get(mode, "gray"))
        # exact number on top of each bar
        for bar, c in zip(bars, counts):
            if c > 0:
                ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height(),
                        f"{c:.0f}", ha="center", va="bottom", fontsize=5)

    ax.set_xlabel("Ring", fontsize=12)
    ax.set_ylabel("Count", fontsize=12)
    ax.set_xticks(rings)
    ax.legend(fontsize=9)
    ax.grid(axis="y", alpha=0.3)
    plt.tight_layout()
    path = os.path.join(out_dir, f"{scene_name}_modes.png")
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"    -> {path}")


# ── Plot 3: combined dx/dy/dz in one plot ─────────────────────────────────────
def plot_residuals_combined(avg_df, scene_name, out_dir):
    rings = sorted(avg_df["ring"].unique())
    fig, ax = plt.subplots(figsize=(16, 8))
    fig.suptitle(f"{scene_name} — dx/dy/dz combined per ring (avg over frames)", fontsize=14)

    for comp, color, offset in [("dx", "blue", -0.15), ("dy", "green", 0.0), ("dz", "red", 0.15)]:
        for ring in rings:
            vals = avg_df.loc[avg_df["ring"] == ring, comp].values
            x = np.full(len(vals), ring + offset) + np.random.uniform(-0.05, 0.05, len(vals))
            ax.scatter(x, vals, s=8, alpha=0.5, color=color, edgecolors="none",
                       label=comp if ring == rings[0] else None)

    ax.set_xlabel("Ring", fontsize=12)
    ax.set_ylabel("Residual", fontsize=12)
    ax.axhline(0, color="grey", lw=0.5, ls="--")
    ax.set_xticks(rings)
    ax.legend(fontsize=10)
    ax.grid(axis="y", alpha=0.3)
    plt.tight_layout()
    path = os.path.join(out_dir, f"{scene_name}_residuals_combined.png")
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"    -> {path}")


# ── Main ──────────────────────────────────────────────────────────────────────
def main():
    print("Loading …")
    scene_data = load_all_data(BASE_DIR)

    for scene_name, frames in scene_data.items():
        print(f"\n{scene_name} ({len(frames)} frames)")
        avg_res = average_residuals_per_ring(frames)
        avg_mode = average_mode_counts_per_ring(frames)
        plot_residuals(avg_res, scene_name, OUTPUT_DIR)
        plot_residuals_combined(avg_res, scene_name, OUTPUT_DIR)
        plot_modes(avg_mode, scene_name, OUTPUT_DIR)

    # ── Combined plot across all scenes (just overlap the 10 per-scene results) ─
    print("\nCombining all scenes …")
    all_avg_res = []
    all_avg_mode = []
    for scene_name, frames in scene_data.items():
        all_avg_res.append(average_residuals_per_ring(frames))
        all_avg_mode.append(average_mode_counts_per_ring(frames))
    combined_res = pd.concat(all_avg_res, ignore_index=True)
    combined_mode = pd.concat(all_avg_mode, ignore_index=True)
    plot_residuals(combined_res, "all_scenes", OUTPUT_DIR)
    plot_residuals_combined(combined_res, "all_scenes", OUTPUT_DIR)
    plot_modes(combined_mode, "all_scenes", OUTPUT_DIR)

    print(f"\nDone — plots in {OUTPUT_DIR}")


if __name__ == "__main__":
    main()
