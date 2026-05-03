"""
Build an HTML dashboard from benchmark CSVs under results/.

Run from repo root:
  python dashboard/visualization.py
"""
import io
from pathlib import Path

import numpy
import pandas
import plotly.express as px
import plotly.graph_objects as go
from plotly.subplots import make_subplots


def project_root():
    return Path(__file__).resolve().parent.parent


def results_dir():
    return project_root() / "results"


def summary_results_csv_path():
    return results_dir() / "benchmark_results.csv"


def raw_lock_based_csv_path():
    return results_dir() / "raw_data_operations" / "lock_based_raw.csv"


def raw_lock_free_csv_path():
    return results_dir() / "raw_data_operations" / "lock_free_raw.csv"


def dashboard_output_dir():
    out = Path(__file__).resolve().parent / "out"
    out.mkdir(parents=True, exist_ok=True)
    return out


def read_summary_dataframe():
    path = summary_results_csv_path()
    if not path.is_file():
        raise FileNotFoundError("Missing %s — run ./benchmark from build/ first." % path)
    cleaned_lines = []
    with open(path, "r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.split("#")[0].strip()
            if not line:
                continue
            cleaned_lines.append(line + "\n")
    buffer = io.StringIO("".join(cleaned_lines))
    return pandas.read_csv(buffer)


def scan_max_latency_microseconds(csv_path):
    if not csv_path.is_file():
        raise FileNotFoundError("Missing %s — run ./benchmark from build/ first." % csv_path)
    max_latency = 0.0
    for chunk in pandas.read_csv(
        csv_path,
        usecols=["latency_us"],
        chunksize=500_000,
    ):
        chunk_max = float(chunk["latency_us"].max())
        if chunk_max > max_latency:
            max_latency = chunk_max
    return max_latency


def accumulate_push_and_pop_histograms(csv_path, bin_edges):
    bin_count = len(bin_edges) - 1
    push_counts = numpy.zeros(bin_count, dtype=numpy.int64)
    pop_counts = numpy.zeros(bin_count, dtype=numpy.int64)
    for chunk in pandas.read_csv(
        csv_path,
        usecols=["latency_us", "op_type"],
        chunksize=500_000,
    ):
        push_mask = chunk["op_type"] == "push"
        if push_mask.any():
            push_values = chunk.loc[push_mask, "latency_us"].to_numpy(dtype=numpy.float64)
            push_counts += numpy.histogram(push_values, bins=bin_edges)[0]
        pop_mask = chunk["op_type"] == "pop"
        if pop_mask.any():
            pop_values = chunk.loc[pop_mask, "latency_us"].to_numpy(dtype=numpy.float64)
            pop_counts += numpy.histogram(pop_values, bins=bin_edges)[0]
    return push_counts, pop_counts


def bin_centers_from_edges(bin_edges):
    return (bin_edges[:-1] + bin_edges[1:]) / 2.0


def build_throughput_line_chart(summary_dataframe):
    return px.line(
        summary_dataframe,
        x="num_producers",
        y="throughput_ops_per_sec",
        color="queue_type",
        markers=True,
        title="Throughput vs producers (consumers match)",
    )


def build_average_latency_line_chart(summary_dataframe):
    return px.line(
        summary_dataframe,
        x="num_producers",
        y="avg_latency_us",
        color="queue_type",
        markers=True,
        title="Average latency (µs) vs producers",
    )


def build_latency_bar_trace(bin_centers, counts, trace_name):
    return go.Bar(
        x=bin_centers,
        y=counts,
        name=trace_name,
    )


def build_full_data_latency_figure(csv_path, title):
    max_latency = scan_max_latency_microseconds(csv_path)
    upper = max(max_latency * 1.05, 1e-6)
    bin_edges = numpy.linspace(0.0, upper, 201)
    centers = bin_centers_from_edges(bin_edges)
    push_counts, pop_counts = accumulate_push_and_pop_histograms(csv_path, bin_edges)
    push_trace = build_latency_bar_trace(centers, push_counts, "push")
    pop_trace = build_latency_bar_trace(centers, pop_counts, "pop")
    latency_figure = go.Figure(data=[push_trace, pop_trace])
    latency_figure.update_layout(
        barmode="overlay",
        title=title,
        xaxis_title="latency (µs)",
        yaxis_title="count (all rows; 1 scan for max + 1 chunked scan)",
    )
    latency_figure.update_traces(opacity=0.65)
    return latency_figure


def main():
    summary_dataframe = read_summary_dataframe()

    throughput_line_chart = build_throughput_line_chart(summary_dataframe)
    average_latency_line_chart = build_average_latency_line_chart(summary_dataframe)

    lock_based_latency_chart = None
    lock_free_latency_chart = None
    if raw_lock_based_csv_path().is_file():
        lock_based_latency_chart = build_full_data_latency_figure(
            raw_lock_based_csv_path(),
            "Lock-based queue — latency histogram (full data)",
        )
    if raw_lock_free_csv_path().is_file():
        lock_free_latency_chart = build_full_data_latency_figure(
            raw_lock_free_csv_path(),
            "Lock-free queue — latency histogram (full data)",
        )

    output_path = dashboard_output_dir() / "benchmark_dashboard.html"

    subplot_count = 2 + (1 if lock_based_latency_chart else 0) + (1 if lock_free_latency_chart else 0)
    row_titles = [
        "Throughput (summary)",
        "Average latency (summary)",
    ]
    if lock_based_latency_chart:
        row_titles.append("Lock-based latency (full raw counts)")
    if lock_free_latency_chart:
        row_titles.append("Lock-free latency (full raw counts)")

    combined_dashboard = make_subplots(
        rows=subplot_count,
        cols=1,
        subplot_titles=tuple(row_titles),
        vertical_spacing=0.06,
    )

    row_index = 1
    for trace in throughput_line_chart.data:
        combined_dashboard.add_trace(trace, row=row_index, col=1)
    row_index += 1
    for trace in average_latency_line_chart.data:
        combined_dashboard.add_trace(trace, row=row_index, col=1)
    row_index += 1

    if lock_based_latency_chart:
        for trace in lock_based_latency_chart.data:
            combined_dashboard.add_trace(trace, row=row_index, col=1)
        row_index += 1
    if lock_free_latency_chart:
        for trace in lock_free_latency_chart.data:
            combined_dashboard.add_trace(trace, row=row_index, col=1)

    combined_dashboard.update_layout(
        height=260 * subplot_count + 200,
        title_text="Lock-Free MPMC benchmark dashboard",
        showlegend=True,
    )
    combined_dashboard.write_html(output_path, include_plotlyjs="cdn")
    print("Wrote", output_path.resolve())


if __name__ == "__main__":
    main()