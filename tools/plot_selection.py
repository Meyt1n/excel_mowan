#!/usr/bin/env python3
"""
Simple chart script for a small course project.

Usage:
python tools/plot_selection.py --input data.csv --chart line --output out.png --title "My Chart"
"""

import argparse
import csv
import math
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


plt.rcParams["font.sans-serif"] = ["Microsoft YaHei", "SimHei", "Arial Unicode MS", "DejaVu Sans"]
plt.rcParams["axes.unicode_minus"] = False


def is_number(text: str) -> bool:
    if text is None:
        return False
    text = text.strip()
    if text == "":
        return False
    try:
        float(text)
        return True
    except ValueError:
        return False


def to_number(text: str) -> float:
    try:
        return float(text.strip())
    except Exception:
        return math.nan


def read_csv_rows(path: Path) -> list[list[str]]:
    with path.open("r", encoding="utf-8", newline="") as f:
        return list(csv.reader(f))


def build_plot_data(rows: list[list[str]]):
    if not rows:
        raise ValueError("CSV is empty.")

    max_cols = max(len(r) for r in rows)
    rows = [r + [""] * (max_cols - len(r)) for r in rows]

    has_header = False
    if max_cols > 1:
        has_header = any(not is_number(c) for c in rows[0][1:])

    if has_header:
        header = rows[0]
        data_rows = rows[1:]
    else:
        header = [f"Series {i + 1}" for i in range(max_cols)]
        data_rows = rows

    if not data_rows:
        raise ValueError("No data rows.")

    if max_cols >= 2:
        x_labels = [r[0] if r[0].strip() else str(i + 1) for i, r in enumerate(data_rows)]
        start_col = 1
        x_axis_name = header[0].strip() if has_header and header[0].strip() else "Category"
    else:
        x_labels = [str(i + 1) for i in range(len(data_rows))]
        start_col = 0
        x_axis_name = "Category"

    names = []
    series = []
    for col in range(start_col, max_cols):
        ys = [to_number(r[col]) for r in data_rows]
        if not any(not math.isnan(v) for v in ys):
            continue
        name = header[col].strip() if col < len(header) and header[col].strip() else f"Series {len(names) + 1}"
        names.append(name)
        series.append(ys)

    if not series:
        raise ValueError("No numeric series found.")

    y_axis_name = names[0] if len(names) == 1 else "Value"
    return x_labels, names, series, x_axis_name, y_axis_name


def draw_line_or_bar(
    x_labels: list[str],
    names: list[str],
    series: list[list[float]],
    x_axis_name: str,
    y_axis_name: str,
    chart_type: str,
    title: str,
):
    fig, ax = plt.subplots(figsize=(10, 5.6))
    x = list(range(len(x_labels)))

    if chart_type == "bar":
        bar_width = 0.8 / max(1, len(series))
        for i, (name, ys) in enumerate(zip(names, series)):
            xs = [k - 0.4 + i * bar_width + bar_width / 2 for k in x]
            ax.bar(xs, ys, width=bar_width, label=name)
    else:
        for name, ys in zip(names, series):
            ax.plot(x, ys, marker="o", linewidth=1.8, label=name)

    ax.set_title(title if title else "Selection Chart")
    ax.set_xlabel(x_axis_name)
    ax.set_ylabel(y_axis_name)
    ax.set_xticks(x)
    ax.set_xticklabels(x_labels, rotation=30, ha="right")
    ax.grid(axis="y", linestyle="--", alpha=0.35)
    ax.legend(loc="best")
    fig.tight_layout()
    return fig


def draw_pie(x_labels: list[str], names: list[str], series: list[list[float]], title: str):
    values = []
    labels = []
    for label, v in zip(x_labels, series[0]):
        if math.isnan(v) or v <= 0:
            continue
        labels.append(label)
        values.append(v)

    if not values:
        raise ValueError("Pie chart needs positive numbers.")

    fig, ax = plt.subplots(figsize=(8, 6))
    ax.pie(values, labels=labels, autopct="%1.1f%%", startangle=90, counterclock=False)
    ax.axis("equal")
    ax.set_title(title if title else f"{names[0]} Pie Chart")
    fig.tight_layout()
    return fig


def main() -> int:
    parser = argparse.ArgumentParser(description="Plot CSV as line/bar/pie.")
    parser.add_argument("--input", required=True, help="Input CSV path")
    parser.add_argument("--chart", choices=["line", "bar", "pie"], default="line", help="Chart type")
    parser.add_argument("--output", required=True, help="Output PNG path")
    parser.add_argument("--title", default="", help="Chart title")
    args = parser.parse_args()

    input_path = Path(args.input)
    if not input_path.exists():
        print(f"Input CSV not found: {input_path}", file=sys.stderr)
        return 1

    rows = read_csv_rows(input_path)
    try:
        x_labels, names, series, x_axis_name, y_axis_name = build_plot_data(rows)

        if args.chart == "pie":
            fig = draw_pie(x_labels, names, series, args.title)
        else:
            fig = draw_line_or_bar(
                x_labels, names, series, x_axis_name, y_axis_name, args.chart, args.title
            )
    except Exception as e:
        print(str(e), file=sys.stderr)
        return 1

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, dpi=150)
    plt.close(fig)
    print(str(output_path))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
