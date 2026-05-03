
"""
MPMC Queue Benchmark Dashboard
Run from repo root:
    python dashboard/visualization.py
"""

import io
import numpy
import pandas
import plotly.graph_objects as go
from plotly.subplots import make_subplots
from pathlib import Path


# ── Paths ─────────────────────────────────────────────────────────────────────

ROOT = Path(__file__).resolve().parent.parent
SUMMARY_CSV   = ROOT / "results" / "benchmark_results.csv"
LB_RAW_CSV    = ROOT / "results" / "raw_data_operations" / "lock_based_raw.csv"
LF_RAW_CSV    = ROOT / "results" / "raw_data_operations" / "lock_free_raw.csv"
OUT_DIR       = Path(__file__).resolve().parent / "out"
OUT_DIR.mkdir(parents=True, exist_ok=True)


# ── Colors ────────────────────────────────────────────────────────────────────

LB_COLOR  = "#1B5FA5"   # blue  — lock-based
LF_COLOR  = "#0F8C6A"   # teal  — lock-free


# ── Data loading ──────────────────────────────────────────────────────────────

def load_summary():
    """Strip inline comments then parse CSV."""
    lines = []
    with open(SUMMARY_CSV, encoding="utf-8") as f:
        for line in f:
            clean = line.split("#")[0].strip()
            if clean:
                lines.append(clean)
    df = pandas.read_csv(io.StringIO("\n".join(lines)))
    df["thread_label"] = df["num_producers"].astype(str) + " P / " + df["num_consumers"].astype(str) + " C"
    return df


def load_raw(path):
    chunks = []
    for chunk in pandas.read_csv(path, usecols=["op_type", "latency_us"], chunksize=500_000):
        chunks.append(chunk)
    return pandas.concat(chunks, ignore_index=True)


# ── Chart builders ────────────────────────────────────────────────────────────

def throughput_bar(df):
    lb = df[df["queue_type"] == "LockBased"]
    lf = df[df["queue_type"] == "LockFree"]
    fig = go.Figure()
    fig.add_bar(name="Lock-Based", x=lb["thread_label"], y=lb["throughput_ops_per_sec"],
                marker_color=LB_COLOR, text=lb["throughput_ops_per_sec"].apply(lambda v: f"{v/1e6:.2f}M"),
                textposition="outside")
    fig.add_bar(name="Lock-Free",  x=lf["thread_label"], y=lf["throughput_ops_per_sec"],
                marker_color=LF_COLOR, text=lf["throughput_ops_per_sec"].apply(lambda v: f"{v/1e6:.2f}M"),
                textposition="outside")
    fig.update_layout(
        title="Throughput — ops per second by thread count",
        yaxis_title="ops / sec",
        xaxis_title="Thread configuration",
        barmode="group",
        legend=dict(orientation="h", y=1.12),
        yaxis=dict(tickformat=".2s"),
    )
    return fig


def latency_bar(df):
    lb = df[df["queue_type"] == "LockBased"]
    lf = df[df["queue_type"] == "LockFree"]
    fig = go.Figure()
    fig.add_bar(name="Lock-Based", x=lb["thread_label"], y=lb["avg_latency_us"],
                marker_color=LB_COLOR, text=lb["avg_latency_us"].apply(lambda v: f"{v:.2f}µs"),
                textposition="outside")
    fig.add_bar(name="Lock-Free",  x=lf["thread_label"], y=lf["avg_latency_us"],
                marker_color=LF_COLOR, text=lf["avg_latency_us"].apply(lambda v: f"{v:.2f}µs"),
                textposition="outside")
    fig.update_layout(
        title="Average latency (µs) per operation by thread count",
        yaxis_title="latency (µs)",
        xaxis_title="Thread configuration",
        barmode="group",
        legend=dict(orientation="h", y=1.12),
    )
    return fig


def scalability_line(df):
    lb = df[df["queue_type"] == "LockBased"].sort_values("num_producers")
    lf = df[df["queue_type"] == "LockFree"].sort_values("num_producers")
    fig = go.Figure()
    fig.add_scatter(name="Lock-Based", x=lb["num_producers"], y=lb["throughput_ops_per_sec"],
                    mode="lines+markers", line=dict(color=LB_COLOR, width=2),
                    marker=dict(size=8))
    fig.add_scatter(name="Lock-Free",  x=lf["num_producers"], y=lf["throughput_ops_per_sec"],
                    mode="lines+markers", line=dict(color=LF_COLOR, width=2, dash="dash"),
                    marker=dict(size=8))
    fig.update_layout(
        title="Scalability — throughput as thread count grows",
        yaxis_title="ops / sec",
        xaxis_title="Number of producers (= consumers)",
        xaxis=dict(tickvals=[1, 2, 4, 8]),
        yaxis=dict(tickformat=".2s"),
        legend=dict(orientation="h", y=1.12),
    )
    return fig


def raw_latency_histogram(raw_df, title, color):
    push_df = raw_df[raw_df["op_type"] == "push"]["latency_us"]
    pop_df  = raw_df[raw_df["op_type"] == "pop"]["latency_us"]

    p95 = float(raw_df["latency_us"].quantile(0.95))

    fig = go.Figure()
    fig.add_histogram(name="push", x=push_df.clip(upper=p95),
                      opacity=0.65, marker_color=color, nbinsx=100)
    fig.add_histogram(name="pop",  x=pop_df.clip(upper=p95),
                      opacity=0.65, marker_color="#E07B39", nbinsx=100)
    fig.update_layout(
        barmode="overlay",
        title=title + f"  (clipped at p95 = {p95:.2f}µs for readability)",
        xaxis_title="latency (µs)",
        yaxis_title="operation count",
        legend=dict(orientation="h", y=1.12),
    )
    return fig


def push_pop_latency_box(lb_raw, lf_raw):
    fig = go.Figure()
    for label, df, color in [("LockBased push", lb_raw[lb_raw["op_type"]=="push"], LB_COLOR),
                              ("LockBased pop",  lb_raw[lb_raw["op_type"]=="pop"],  "#5B9BD5"),
                              ("LockFree push",  lf_raw[lf_raw["op_type"]=="push"], LF_COLOR),
                              ("LockFree pop",   lf_raw[lf_raw["op_type"]=="pop"],  "#70C49E")]:
        sample = df["latency_us"].sample(min(50_000, len(df)), random_state=42)
        fig.add_box(name=label, y=sample, marker_color=color, boxpoints=False)
    fig.update_layout(
        title="Latency distribution — push vs pop (sampled 50k ops each)",
        yaxis_title="latency (µs)",
        yaxis=dict(range=[0, float(pandas.concat([lb_raw["latency_us"], lf_raw["latency_us"]]).quantile(0.95))]),
        legend=dict(orientation="h", y=1.12),
    )
    return fig


# ── Assemble dashboard ────────────────────────────────────────────────────────

def main():
    print("Loading summary CSV...")
    df = load_summary()

    print("Loading raw CSVs (this may take a moment)...")
    lb_raw = load_raw(LB_RAW_CSV)
    lf_raw = load_raw(LF_RAW_CSV)

    print("Building charts...")

    charts = [
        (throughput_bar(df),    "Throughput by thread count"),
        (latency_bar(df),       "Avg latency by thread count"),
        (scalability_line(df),  "Scalability trend"),
        (raw_latency_histogram(lb_raw, "Lock-Based — latency histogram (full raw data)", LB_COLOR),
                                "Lock-Based latency histogram"),
        (raw_latency_histogram(lf_raw, "Lock-Free — latency histogram (full raw data)", LF_COLOR),
                                "Lock-Free latency histogram"),
        (push_pop_latency_box(lb_raw, lf_raw), "Push vs Pop latency box plot"),
    ]

    fig = make_subplots(
        rows=len(charts), cols=1,
        subplot_titles=[c[1] for c in charts],
        vertical_spacing=0.05,
    )

    for i, (chart, _) in enumerate(charts, start=1):
        for trace in chart.data:
            fig.add_trace(trace, row=i, col=1)

    fig.update_layout(
        height=420 * len(charts) + 200,
        title_text="MPMC Queue Benchmark Dashboard",
        title_font_size=22,
        showlegend=True,
    )

    out = OUT_DIR / "benchmark_dashboard.html"
    fig.write_html(out, include_plotlyjs="cdn")
    print(f"\nDashboard written to: {out.resolve()}")
    print("Open it in any browser.")


if __name__ == "__main__":
    main()