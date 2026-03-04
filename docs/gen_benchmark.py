#!/usr/bin/env python3
"""Generate benchmark SVG charts for multiple devices."""

from pathlib import Path
import math

BENCHMARKS = [
    {
        "title": "AMD Vega (gfx90c)",
        "outfile": "benchmark.svg",
        "data": [
            # N, std::sort (ms), OpenCL total (ms)
            (1_024, 0.04, 6.90),
            (8_192, 0.93, 0.55),
            (65_536, 3.13, 1.54),
            (262_144, 13.79, 8.61),
            (524_288, 29.35, 26.33),
            (1_048_576, 61.90, 55.98),
        ],
    },
    {
        "title": "NVIDIA GTX 1080 Ti",
        "outfile": "benchmark_gtx1080.svg",
        "data": [
            # N, std::sort (ms), OpenCL total (ms)
            (1_024, 0.03, 0.36),
            (8_192, 0.32, 0.43),
            (65_536, 3.19, 0.93),
            (262_144, 14.28, 1.87),
            (524_288, 30.22, 2.94),
            (1_048_576, 63.52, 7.35),
        ],
    },
]


COLOR_CPU = "#8b949e"
COLOR_OCL = "#58a6ff"
COLOR_TEXT = "#c9d1d9"
COLOR_GRID = "#30363d"
COLOR_AXIS = "#484f58"
COLOR_DIM = "#6e7681"


TOTAL_W = 680
PLOT_H = 300
LEFT_PAD = 72
RIGHT_PAD = 24
TOP_PAD = 42
BOT_PAD = 96
TOTAL_H = TOP_PAD + PLOT_H + BOT_PAD


def format_n(n: int) -> str:
    if n >= 1_000_000:
        return f"{n // 1_000_000}M"
    if n >= 1_000:
        return f"{n // 1_000}K"
    return str(n)


def chart_svg(title: str, data: list[tuple[int, float, float]]) -> str:
    n_groups = len(data)
    plot_w = TOTAL_W - LEFT_PAD - RIGHT_PAD
    group_w = plot_w / n_groups
    bar_w = group_w * 0.28
    gap = group_w * 0.06

    max_val = max(max(cpu, ocl) for _, cpu, ocl in data) * 1.10

    def px_h(ms: float) -> float:
        return PLOT_H * ms / max_val

    def bar_x(group_idx: int, bar_idx: int) -> float:
        group_cx = LEFT_PAD + (group_idx + 0.5) * group_w
        if bar_idx == 0:
            return group_cx - gap / 2 - bar_w
        return group_cx + gap / 2

    def chart_y(height: float) -> float:
        return TOP_PAD + PLOT_H - height

    raw_step = max_val / 5
    magnitude = 10 ** math.floor(math.log10(raw_step))
    grid_step = round(raw_step / magnitude) * magnitude
    grid_vals = []
    v = 0.0
    while v <= max_val * 1.01:
        grid_vals.append(round(v, 6))
        v += grid_step

    lines = []
    lines.append(
        f'<svg xmlns="http://www.w3.org/2000/svg" '
        f'width="{TOTAL_W}" height="{TOTAL_H}" viewBox="0 0 {TOTAL_W} {TOTAL_H}">'
    )
    lines.append(
        f'  <text x="{TOTAL_W / 2:.0f}" y="24" text-anchor="middle" '
        f'font-family="sans-serif" font-size="15" fill="{COLOR_TEXT}">{title}</text>'
    )

    for gv in grid_vals:
        py = chart_y(px_h(gv))
        lines.append(
            f'  <line x1="{LEFT_PAD}" y1="{py:.1f}" x2="{TOTAL_W - RIGHT_PAD}" y2="{py:.1f}" '
            f'stroke="{COLOR_GRID}" stroke-width="1"/>'
        )
        lines.append(
            f'  <text x="{LEFT_PAD - 8}" y="{py + 4:.1f}" text-anchor="end" '
            f'font-family="monospace" font-size="11" fill="{COLOR_TEXT}">{gv:.0f}</text>'
        )

    mid_y = TOP_PAD + PLOT_H / 2
    lx = LEFT_PAD - 40
    lines.append(
        f'  <text x="{lx}" y="{mid_y:.0f}" text-anchor="middle" '
        f'font-family="sans-serif" font-size="12" fill="{COLOR_DIM}" '
        f'transform="rotate(-90,{lx},{mid_y:.0f})">time, ms</text>'
    )

    lines.append(
        f'  <line x1="{LEFT_PAD}" y1="{TOP_PAD}" x2="{LEFT_PAD}" y2="{TOP_PAD + PLOT_H}" '
        f'stroke="{COLOR_AXIS}" stroke-width="1.5"/>'
    )
    lines.append(
        f'  <line x1="{LEFT_PAD}" y1="{TOP_PAD + PLOT_H}" x2="{TOTAL_W - RIGHT_PAD}" y2="{TOP_PAD + PLOT_H}" '
        f'stroke="{COLOR_AXIS}" stroke-width="1.5"/>'
    )

    for gi, (n, cpu, ocl) in enumerate(data):
        hc = px_h(cpu)
        xc = bar_x(gi, 0)
        yc = chart_y(hc)
        lines.append(
            f'  <rect x="{xc:.1f}" y="{yc:.1f}" width="{bar_w:.1f}" height="{hc:.1f}" '
            f'fill="{COLOR_CPU}" rx="2"/>'
        )

        ho = px_h(ocl)
        xo = bar_x(gi, 1)
        yo = chart_y(ho)
        lines.append(
            f'  <rect x="{xo:.1f}" y="{yo:.1f}" width="{bar_w:.1f}" height="{ho:.1f}" '
            f'fill="{COLOR_OCL}" rx="2"/>'
        )

        group_cx = LEFT_PAD + (gi + 0.5) * group_w
        ly = TOP_PAD + PLOT_H + 12
        lines.append(
            f'  <text x="{group_cx:.1f}" y="{ly}" text-anchor="middle" '
            f'font-family="monospace" font-size="11" fill="{COLOR_TEXT}">{format_n(n)}</text>'
        )
        lines.append(
            f'  <text x="{group_cx:.1f}" y="{ly + 13}" text-anchor="middle" '
            f'font-family="monospace" font-size="9" fill="{COLOR_DIM}">N={n:,}</text>'
        )

    lines.append(
        f'  <text x="{LEFT_PAD + plot_w / 2:.0f}" y="{TOP_PAD + PLOT_H + 42}" text-anchor="middle" '
        f'font-family="sans-serif" font-size="12" fill="{COLOR_DIM}">Array size (N)</text>'
    )

    leg_y = TOP_PAD + PLOT_H + 68
    sq = 11
    leg_x = LEFT_PAD + 16
    lines.append(
        f'  <rect x="{leg_x}" y="{leg_y - sq + 2}" width="{sq}" height="{sq}" fill="{COLOR_CPU}" rx="2"/>'
    )
    lines.append(
        f'  <text x="{leg_x + sq + 6}" y="{leg_y + 1}" font-family="sans-serif" font-size="12" fill="{COLOR_TEXT}">'
        f'std::sort (CPU baseline)</text>'
    )
    lines.append(
        f'  <rect x="{leg_x + 180}" y="{leg_y - sq + 2}" width="{sq}" height="{sq}" fill="{COLOR_OCL}" rx="2"/>'
    )
    lines.append(
        f'  <text x="{leg_x + 180 + sq + 6}" y="{leg_y + 1}" font-family="sans-serif" font-size="12" fill="{COLOR_TEXT}">'
        f'bitonic sort (OpenCL)</text>'
    )

    lines.append("</svg>")
    return "\n".join(lines)


def main() -> None:
    script_dir = Path(__file__).resolve().parent
    for bench in BENCHMARKS:
        svg = chart_svg(bench["title"], bench["data"])
        out_path = script_dir / bench["outfile"]
        out_path.write_text(svg, encoding="utf-8")
        print(f"Written {out_path} ({TOTAL_W}x{TOTAL_H}px)")


if __name__ == "__main__":
    main()
