#!/usr/bin/env python3
"""Generate benchmark bar chart SVG.

Transparent background — designed for GitHub dark theme.
Real measurements on AMD Vega (gfx90c) GPU.
"""

# ── data ──────────────────────────────────────────────────────────────────────
data = [
    #  N          std::sort (ms)   OpenCL total (ms)
    (    1_024,      0.04,            6.90),
    (    8_192,      0.93,            0.55),
    (   65_536,      3.13,            1.54),
    (  262_144,     13.79,            8.61),
    (  524_288,     29.35,           26.33),
    (1_048_576,     61.90,           55.98),
]

# ── colours ───────────────────────────────────────────────────────────────────
COLOR_CPU  = "#8b949e"   # muted grey  — std::sort
COLOR_OCL  = "#58a6ff"   # blue        — OpenCL
COLOR_TEXT = "#c9d1d9"
COLOR_GRID = "#30363d"
COLOR_AXIS = "#484f58"
COLOR_DIM  = "#6e7681"

# ── layout ────────────────────────────────────────────────────────────────────
TOTAL_W  = 680
PLOT_H   = 300
LEFT_PAD = 72    # Y-axis labels
RIGHT_PAD = 24
TOP_PAD  = 24    # no title
BOT_PAD  = 96    # X-axis labels + X-axis title + legend

TOTAL_H  = TOP_PAD + PLOT_H + BOT_PAD

N_GROUPS = len(data)
PLOT_W   = TOTAL_W - LEFT_PAD - RIGHT_PAD
GROUP_W  = PLOT_W / N_GROUPS
BAR_W    = GROUP_W * 0.28
GAP      = GROUP_W * 0.06   # between the two bars in a group

max_val = max(max(r[1], r[2]) for r in data) * 1.10

def px_h(ms):
    return PLOT_H * ms / max_val

def bar_x(gi, bi):
    group_cx = LEFT_PAD + (gi + 0.5) * GROUP_W
    # bi=0 → left bar (CPU), bi=1 → right bar (OCL)
    if bi == 0:
        return group_cx - GAP / 2 - BAR_W
    else:
        return group_cx + GAP / 2

def chart_y(h):
    return TOP_PAD + PLOT_H - h

# Y-axis grid
import math
raw_step  = max_val / 5
magnitude = 10 ** math.floor(math.log10(raw_step))
grid_step = round(raw_step / magnitude) * magnitude
grid_vals = []
v = 0.0
while v <= max_val * 1.01:
    grid_vals.append(round(v, 6))
    v += grid_step

# SVG
L = []
L.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{TOTAL_W}" height="{TOTAL_H}" viewBox="0 0 {TOTAL_W} {TOTAL_H}">')

# grid + Y labels
for gv in grid_vals:
    py = chart_y(px_h(gv))
    L.append(f'  <line x1="{LEFT_PAD}" y1="{py:.1f}" x2="{TOTAL_W - RIGHT_PAD}" y2="{py:.1f}"')
    L.append(f'        stroke="{COLOR_GRID}" stroke-width="1"/>')
    lbl = f"{gv:.0f}"
    L.append(f'  <text x="{LEFT_PAD - 8}" y="{py + 4:.1f}" text-anchor="end"')
    L.append(f'        font-family="monospace" font-size="11" fill="{COLOR_TEXT}">{lbl}</text>')

# Y-axis label — closer to axis
mid_y = TOP_PAD + PLOT_H / 2
lx    = LEFT_PAD - 40
L.append(f'  <text x="{lx}" y="{mid_y:.0f}" text-anchor="middle"')
L.append(f'        font-family="sans-serif" font-size="12" fill="{COLOR_DIM}"')
L.append(f'        transform="rotate(-90,{lx},{mid_y:.0f})">time, ms</text>')

# axes
L.append(f'  <line x1="{LEFT_PAD}" y1="{TOP_PAD}" x2="{LEFT_PAD}" y2="{TOP_PAD + PLOT_H}"')
L.append(f'        stroke="{COLOR_AXIS}" stroke-width="1.5"/>')
L.append(f'  <line x1="{LEFT_PAD}" y1="{TOP_PAD + PLOT_H}" x2="{TOTAL_W - RIGHT_PAD}" y2="{TOP_PAD + PLOT_H}"')
L.append(f'        stroke="{COLOR_AXIS}" stroke-width="1.5"/>')

# bars + X labels
for gi, (n, cpu, ocl) in enumerate(data):
    # CPU bar
    hc = px_h(cpu)
    xc = bar_x(gi, 0)
    yc = chart_y(hc)
    L.append(f'  <rect x="{xc:.1f}" y="{yc:.1f}" width="{BAR_W:.1f}" height="{hc:.1f}"')
    L.append(f'        fill="{COLOR_CPU}" rx="2"/>')

    # OCL bar
    ho = px_h(ocl)
    xo = bar_x(gi, 1)
    yo = chart_y(ho)
    L.append(f'  <rect x="{xo:.1f}" y="{yo:.1f}" width="{BAR_W:.1f}" height="{ho:.1f}"')
    L.append(f'        fill="{COLOR_OCL}" rx="2"/>')

    # X-axis label — horizontal, centred under group
    group_cx = LEFT_PAD + (gi + 0.5) * GROUP_W
    ly = TOP_PAD + PLOT_H + 12

    # Format N compactly: 1K, 8K, 64K, 256K, 512K, 1M
    if n >= 1_000_000:
        lbl = f"{n // 1_000_000}M"
    elif n >= 1_000:
        lbl = f"{n // 1_000}K"
    else:
        lbl = str(n)

    L.append(f'  <text x="{group_cx:.1f}" y="{ly}" text-anchor="middle"')
    L.append(f'        font-family="monospace" font-size="11" fill="{COLOR_TEXT}">{lbl}</text>')

    # sub-label: array size
    L.append(f'  <text x="{group_cx:.1f}" y="{ly + 13}" text-anchor="middle"')
    L.append(f'        font-family="monospace" font-size="9" fill="{COLOR_DIM}">N={n:,}</text>')

# X-axis label
L.append(f'  <text x="{LEFT_PAD + PLOT_W / 2:.0f}" y="{TOP_PAD + PLOT_H + 42}" text-anchor="middle"')
L.append(f'        font-family="sans-serif" font-size="12" fill="{COLOR_DIM}">Array size (N)</text>')

# legend — below X-axis label
leg_y = TOP_PAD + PLOT_H + 68
sq    = 11
leg_x = LEFT_PAD + 16
L.append(f'  <rect x="{leg_x}" y="{leg_y - sq + 2}" width="{sq}" height="{sq}" fill="{COLOR_CPU}" rx="2"/>')
L.append(f'  <text x="{leg_x + sq + 6}" y="{leg_y + 1}" font-family="sans-serif" font-size="12" fill="{COLOR_TEXT}">std::sort (CPU baseline)</text>')
L.append(f'  <rect x="{leg_x + 180}" y="{leg_y - sq + 2}" width="{sq}" height="{sq}" fill="{COLOR_OCL}" rx="2"/>')
L.append(f'  <text x="{leg_x + 180 + sq + 6}" y="{leg_y + 1}" font-family="sans-serif" font-size="12" fill="{COLOR_TEXT}">bitonic sort (OpenCL)</text>')

L.append('</svg>')

out  = "\n".join(L)
path = __file__.replace("gen_benchmark.py", "benchmark.svg")
with open(path, "w") as f:
    f.write(out)
print(f"Written {path}  ({TOTAL_W}×{TOTAL_H}px)")
