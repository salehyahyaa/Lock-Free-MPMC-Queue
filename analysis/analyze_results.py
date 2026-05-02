#!/usr/bin/env python3
"""
analyze_results.py
==================
Lock-Based vs Lock-Free MPMC Queue — Performance Dashboard
-----------------------------------------------------------
Pipeline:
  1. Pandas  — load & clean raw CSVs
  2. SQLite  — SQL queries (drop-in Postgres-compatible dialect)
  3. Matplotlib — 6-panel dark dashboard saved as PNG

Usage
-----
  # From the repo root:
  python analysis/analyze_results.py

  # Custom CSV paths:
  python analysis/analyze_results.py \
      --lock-based results/raw_data/lock_based_raw.csv \
      --lock-free  results/raw_data/lock_free_raw.csv  \
      --out        results/dashboard.png

Requirements
------------
  pip install pandas matplotlib
  (sqlite3 is part of Python's standard library)
"""

import argparse
import sqlite3
import warnings
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.gridspec as gridspec
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

warnings.filterwarnings("ignore")


# ── CLI ────────────────────────────────────────────────────────────────────────
def parse_args():
    p = argparse.ArgumentParser(description="Queue benchmark dashboard")
    p.add_argument(
        "--lock-based",
        default="results/raw_data/lock_based_raw.csv",
        help="Path to lock-based raw CSV (default: results/raw_data/lock_based_raw.csv)",
    )
    p.add_argument(
        "--lock-free",
        default="results/raw_data/lock_free_raw.csv",
        help="Path to lock-free raw CSV  (default: results/raw_data/lock_free_raw.csv)",
    )
    p.add_argument(
        "--out",
        default="results/dashboard.png",
        help="Output PNG path           (default: results/dashboard.png)",
    )
    p.add_argument(
        "--spike-threshold",
        type=float,
        default=10.0,
        help="Latency threshold (µs) for spike-rate panel (default: 10)",
    )
    return p.parse_args()


# ── 1. LOAD & CLEAN (Pandas) ───────────────────────────────────────────────────
def load_and_clean(lb_path: str, lf_path: str) -> pd.DataFrame:
    print(f"[1/3] Loading CSVs …")
    lb = pd.read_csv(lb_path)
    lf = pd.read_csv(lf_path)

    df = pd.concat([lb, lf], ignore_index=True)

    # Normalise column names
    df.columns = df.columns.str.strip().str.lower()

    # Strip whitespace from string columns
    for col in ("queue_type", "op_type"):
        df[col] = df[col].str.strip()

    # Coerce & validate latency
    df["latency_us"] = pd.to_numeric(df["latency_us"], errors="coerce")
    before = len(df)
    df = df.dropna(subset=["latency_us"])
    df = df[df["latency_us"] >= 0]
    dropped = before - len(df)
    if dropped:
        print(f"  ⚠  Dropped {dropped:,} invalid rows during cleaning.")

    print(f"  ✓  Clean rows: {len(df):,}")
    return df


# ── 2. SQL QUERIES (SQLite in-memory) ─────────────────────────────────────────
def run_queries(df: pd.DataFrame, spike_us: float):
    print("[2/3] Running SQL queries …")
    con = sqlite3.connect(":memory:")
    df.to_sql("perf", con, index=False, if_exists="replace")

    # Q1 – aggregate stats: mean, p50, p95, p99
    q1 = pd.read_sql(
        """
        SELECT queue_type, op_type,
               ROUND(AVG(latency_us), 4) AS mean_us,
               ROUND(AVG(CASE WHEN pct = 0.5  THEN latency_us END), 4) AS p50_us,
               ROUND(AVG(CASE WHEN pct = 0.95 THEN latency_us END), 4) AS p95_us,
               ROUND(AVG(CASE WHEN pct = 0.99 THEN latency_us END), 4) AS p99_us
        FROM (
            SELECT queue_type, op_type, latency_us,
                   CAST(NTILE(100) OVER (
                       PARTITION BY queue_type, op_type
                       ORDER BY latency_us
                   ) AS REAL) / 100.0 AS pct
            FROM perf
        )
        GROUP BY queue_type, op_type
        """,
        con,
    )

    # Q2 – per-thread mean latency
    q2 = pd.read_sql(
        """
        SELECT queue_type, op_type, thread_id,
               AVG(latency_us) AS mean_latency
        FROM perf
        GROUP BY queue_type, op_type, thread_id
        ORDER BY thread_id
        """,
        con,
    )

    # Q3 – spike rate (ops above threshold)
    q3 = pd.read_sql(
        f"""
        SELECT queue_type, op_type,
               ROUND(100.0 * SUM(CASE WHEN latency_us > {spike_us} THEN 1 ELSE 0 END)
                     / COUNT(*), 4) AS spike_pct
        FROM perf
        GROUP BY queue_type, op_type
        """,
        con,
    )

    # Q4 – 500-op rolling mean (thread 1, sampled every 500th op)
    q4 = pd.read_sql(
        """
        SELECT queue_type, op_type, op_id,
               AVG(latency_us) OVER (
                   PARTITION BY queue_type, op_type
                   ORDER BY op_id
                   ROWS BETWEEN 499 PRECEDING AND CURRENT ROW
               ) AS rolling_mean
        FROM perf
        WHERE thread_id = 1 AND op_id % 500 = 0
        ORDER BY queue_type, op_type, op_id
        """,
        con,
    )

    con.close()
    print("  ✓  Queries complete")
    print(q1.to_string(index=False))
    return q1, q2, q3, q4


# ── 3. MATPLOTLIB DASHBOARD ───────────────────────────────────────────────────
CB_BLUE = "#4C72B0"
CB_ORG  = "#DD8452"
COLORS  = {"LockBased": CB_BLUE, "LockFree": CB_ORG}
BG      = "#0F1117"
PANEL   = "#1A1D27"
TEXT    = "#E8EAF0"
GRID    = "#2A2D3A"


def _apply_style(ax, title, xlabel="", ylabel=""):
    ax.set_title(title, fontsize=13, fontweight="bold", color=TEXT, pad=10)
    ax.set_xlabel(xlabel, fontsize=10, color=TEXT)
    ax.set_ylabel(ylabel, fontsize=10, color=TEXT)
    ax.grid(axis="y", linestyle="--", alpha=0.4)
    ax.set_facecolor(PANEL)


def build_dashboard(df, q1, q2, q3, q4, out_path: str, spike_us: float):
    print("[3/3] Building dashboard …")

    plt.rcParams.update(
        {
            "figure.facecolor":  BG,
            "axes.facecolor":    PANEL,
            "axes.edgecolor":    GRID,
            "axes.labelcolor":   TEXT,
            "text.color":        TEXT,
            "xtick.color":       TEXT,
            "ytick.color":       TEXT,
            "grid.color":        GRID,
            "grid.linewidth":    0.6,
            "font.family":       "DejaVu Sans",
            "axes.spines.top":   False,
            "axes.spines.right": False,
        }
    )

    fig = plt.figure(figsize=(20, 22))
    fig.patch.set_facecolor(BG)
    gs = gridspec.GridSpec(
        4, 2, figure=fig,
        hspace=0.52, wspace=0.35,
        left=0.07, right=0.96, top=0.93, bottom=0.04,
    )

    qtys = ["LockBased", "LockFree"]
    ops  = sorted(q1["op_type"].unique())
    w    = 0.35

    # ── Panel A: Mean latency grouped bar ─────────────────────────────────────
    ax1 = fig.add_subplot(gs[0, 0])
    x = np.arange(len(ops))
    for i, qt in enumerate(qtys):
        vals = [q1[(q1.queue_type == qt) & (q1.op_type == op)]["mean_us"].values[0]
                for op in ops]
        bars = ax1.bar(x + i * w, vals, w, label=qt, color=COLORS[qt], alpha=0.9)
        for b, v in zip(bars, vals):
            ax1.text(b.get_x() + b.get_width() / 2, b.get_height() + 0.002,
                     f"{v:.3f}", ha="center", va="bottom", fontsize=9, color=TEXT)
    ax1.set_xticks(x + w / 2)
    ax1.set_xticklabels(ops, fontsize=11)
    ax1.legend(facecolor=PANEL, edgecolor=GRID, labelcolor=TEXT, fontsize=9)
    _apply_style(ax1, "Mean Latency by Queue & Op", ylabel="µs")

    # ── Panel B: Percentile ladder ─────────────────────────────────────────────
    ax2 = fig.add_subplot(gs[0, 1])
    pcts = ["p50_us", "p95_us", "p99_us"]
    labels_p = ["P50", "P95", "P99"]
    for qt in qtys:
        for op, ls, mk in [("push", "-", "o"), ("pop", "--", "s")]:
            row  = q1[(q1.queue_type == qt) & (q1.op_type == op)].iloc[0]
            vals = [row[p] for p in pcts]
            ax2.plot(labels_p, vals, marker=mk, linewidth=2.5, linestyle=ls,
                     color=COLORS[qt], label=f"{qt} {op}", alpha=0.85)
    ax2.set_yscale("log")
    ax2.legend(facecolor=PANEL, edgecolor=GRID, labelcolor=TEXT, fontsize=8, ncol=2)
    _apply_style(ax2, "Latency Percentiles (log scale)", ylabel="µs (log)")

    # ── Panel C: Per-thread mean latency (push) ────────────────────────────────
    ax3 = fig.add_subplot(gs[1, 0])
    push_q2 = q2[q2.op_type == "push"]
    threads = sorted(push_q2["thread_id"].unique())
    x3 = np.arange(len(threads))
    for i, qt in enumerate(qtys):
        vals = [push_q2[(push_q2.queue_type == qt) & (push_q2.thread_id == t)]["mean_latency"].values[0]
                for t in threads]
        ax3.bar(x3 + i * w, vals, w, label=qt, color=COLORS[qt], alpha=0.9)
    ax3.set_xticks(x3 + w / 2)
    ax3.set_xticklabels([f"T{t}" for t in threads], fontsize=9)
    ax3.legend(facecolor=PANEL, edgecolor=GRID, labelcolor=TEXT, fontsize=9)
    _apply_style(ax3, "Per-Thread Mean Latency (Push)", "Thread", "µs")

    # ── Panel D: Per-thread mean latency (pop) ─────────────────────────────────
    ax4 = fig.add_subplot(gs[1, 1])
    pop_q2 = q2[q2.op_type == "pop"]
    for i, qt in enumerate(qtys):
        vals = [pop_q2[(pop_q2.queue_type == qt) & (pop_q2.thread_id == t)]["mean_latency"].values[0]
                for t in threads]
        ax4.bar(x3 + i * w, vals, w, label=qt, color=COLORS[qt], alpha=0.9)
    ax4.set_xticks(x3 + w / 2)
    ax4.set_xticklabels([f"T{t}" for t in threads], fontsize=9)
    ax4.legend(facecolor=PANEL, edgecolor=GRID, labelcolor=TEXT, fontsize=9)
    _apply_style(ax4, "Per-Thread Mean Latency (Pop)", "Thread", "µs")

    # ── Panel E: Spike rate ────────────────────────────────────────────────────
    ax5 = fig.add_subplot(gs[2, 0])
    x5 = np.arange(len(ops))
    for i, qt in enumerate(qtys):
        vals = [q3[(q3.queue_type == qt) & (q3.op_type == op)]["spike_pct"].values[0]
                for op in ops]
        bars = ax5.bar(x5 + i * w, vals, w, label=qt, color=COLORS[qt], alpha=0.9)
        for b, v in zip(bars, vals):
            ax5.text(b.get_x() + b.get_width() / 2, b.get_height() + 0.001,
                     f"{v:.2f}%", ha="center", va="bottom", fontsize=9, color=TEXT)
    ax5.set_xticks(x5 + w / 2)
    ax5.set_xticklabels(ops, fontsize=11)
    ax5.legend(facecolor=PANEL, edgecolor=GRID, labelcolor=TEXT, fontsize=9)
    _apply_style(ax5, f"Spike Rate  (latency > {spike_us} µs)", ylabel="% of ops")

    # ── Panel F: Latency distribution histogram ────────────────────────────────
    ax6 = fig.add_subplot(gs[2, 1])
    for qt in qtys:
        sub = df[(df.queue_type == qt) & (df.op_type == "push") & (df.latency_us > 0)]
        sample = sub["latency_us"].sample(min(50_000, len(sub)), random_state=42)
        ax6.hist(np.log10(sample), bins=80, alpha=0.55,
                 color=COLORS[qt], label=qt, density=True)
    ax6.set_xlabel("log₁₀(latency µs)", fontsize=10, color=TEXT)
    ax6.legend(facecolor=PANEL, edgecolor=GRID, labelcolor=TEXT, fontsize=9)
    _apply_style(ax6, "Push Latency Distribution (log₁₀)", ylabel="Density")

    # ── Panel G: Rolling mean over time ───────────────────────────────────────
    ax7 = fig.add_subplot(gs[3, :])
    for qt in qtys:
        for op, ls in [("push", "-"), ("pop", "--")]:
            sub = q4[(q4.queue_type == qt) & (q4.op_type == op)]
            ax7.plot(sub["op_id"], sub["rolling_mean"],
                     color=COLORS[qt], linestyle=ls, linewidth=1.5,
                     label=f"{qt} {op}", alpha=0.85)
    ax7.set_yscale("log")
    ax7.legend(facecolor=PANEL, edgecolor=GRID, labelcolor=TEXT, fontsize=9, ncol=4)
    _apply_style(ax7,
                 "500-op Rolling Mean Latency Over Time (Thread 1, log scale)",
                 "Operation ID", "µs (log)")

    # ── Title ─────────────────────────────────────────────────────────────────
    total_ops = len(df)
    n_threads = df["thread_id"].nunique()
    fig.text(0.5, 0.965,
             "Lock-Based vs Lock-Free Queue  —  Concurrent Performance Dashboard",
             ha="center", va="top", fontsize=17, fontweight="bold", color=TEXT)
    fig.text(0.5, 0.948,
             f"{total_ops:,} ops total  ·  {n_threads} threads  ·  push & pop  ·  latency in microseconds",
             ha="center", va="top", fontsize=10, color="#9CA3AF")

    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(out_path, dpi=150, bbox_inches="tight", facecolor=BG)
    print(f"  ✓  Dashboard saved → {out_path}")


# ── MAIN ───────────────────────────────────────────────────────────────────────
def main():
    args = parse_args()
    df          = load_and_clean(args.lock_based, args.lock_free)
    q1, q2, q3, q4 = run_queries(df, args.spike_threshold)
    build_dashboard(df, q1, q2, q3, q4, args.out, args.spike_threshold)
    print("Done ✓")


if __name__ == "__main__":
    main()
