#!/usr/bin/env python3
"""Generate bitonic sorting network SVG for N=8.

Shows all compare-exchange steps grouped by phase (k=2, k=4, k=8).
Transparent background — designed for GitHub dark theme.
"""

N = 8

# ── layout ────────────────────────────────────────────────────────────────────
WIRE_SPACING = 44
LEFT_PAD     = 80
RIGHT_PAD    = 80
STEP_WIDTH   = 64
TOP_PAD      = 64
BOTTOM_PAD   = 48
DOT_R        = 5
ARR_W        = 6    # arrowhead half-width
ARR_H        = 9    # arrowhead height

# ── colours ───────────────────────────────────────────────────────────────────
WIRE_COLOR   = "#8b949e"
LABEL_COLOR  = "#c9d1d9"
DIM_COLOR    = "#6e7681"
PHASE_COLORS = ["#58a6ff", "#bc8cff", "#3fb950"]   # k=2, k=4, k=8

# ── build comparator list ─────────────────────────────────────────────────────
all_steps = []
k = 2
while k <= N:
    j = k // 2
    while j >= 1:
        for i in range(N):
            ixj = i ^ j
            if ixj > i:
                ascending = (i & k) == 0
                all_steps.append((i, ixj, ascending, k))
        j //= 2
    k *= 2

# Pack into parallel columns (no wire appears twice per column)
def pack_columns(steps):
    columns = []
    for step in steps:
        lo, hi, asc, k = step
        placed = False
        for col in columns:
            used = {s[0] for s in col} | {s[1] for s in col}
            if lo not in used and hi not in used:
                col.append(step)
                placed = True
                break
        if not placed:
            columns.append([step])
    return columns

columns     = pack_columns(all_steps)
n_cols      = len(columns)
phase_of_col = []
for col in columns:
    k_val = max(s[3] for s in col)
    phase_of_col.append({2: 0, 4: 1, 8: 2}[k_val])

# ── geometry helpers ──────────────────────────────────────────────────────────
width  = LEFT_PAD + n_cols * STEP_WIDTH + RIGHT_PAD
height = TOP_PAD + (N - 1) * WIRE_SPACING + BOTTOM_PAD

def wy(i):  return TOP_PAD + i * WIRE_SPACING
def cx(ci): return LEFT_PAD + (ci + 0.5) * STEP_WIDTH

SLOT_W = 10   # horizontal spread between comparators within one column

def arrow_down(x, y, color):
    """Filled downward triangle with tip at (x, y)."""
    pts = f"{x - ARR_W},{y - ARR_H} {x + ARR_W},{y - ARR_H} {x},{y}"
    return f'  <polygon points="{pts}" fill="{color}"/>'

def arrow_up(x, y, color):
    """Filled upward triangle with tip at (x, y)."""
    pts = f"{x - ARR_W},{y + ARR_H} {x + ARR_W},{y + ARR_H} {x},{y}"
    return f'  <polygon points="{pts}" fill="{color}"/>'

# ── phase column ranges ───────────────────────────────────────────────────────
phase_ranges = {0: [], 1: [], 2: []}
for ci, ph in enumerate(phase_of_col):
    phase_ranges[ph].append(ci)

# ── SVG ───────────────────────────────────────────────────────────────────────
L = []
L.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">')

# title
L.append(f'  <text x="{width//2}" y="22" text-anchor="middle"')
L.append(f'        font-family="sans-serif" font-size="14" font-weight="600" fill="{LABEL_COLOR}">')
L.append(f'    Bitonic Sorting Network — N=8')
L.append(f'  </text>')

# "input" / "sorted" column headers
L.append(f'  <text x="{LEFT_PAD - 8}" y="42" text-anchor="end"')
L.append(f'        font-family="sans-serif" font-size="11" fill="{DIM_COLOR}">input</text>')
L.append(f'  <text x="{LEFT_PAD + n_cols * STEP_WIDTH + 8}" y="42" text-anchor="start"')
L.append(f'        font-family="sans-serif" font-size="11" fill="{DIM_COLOR}">sorted</text>')

# wire labels — left
for i in range(N):
    y = wy(i)
    L.append(f'  <text x="{LEFT_PAD - 12}" y="{y + 5}" text-anchor="end"')
    L.append(f'        font-family="monospace" font-size="12" fill="{LABEL_COLOR}">a[{i}]</text>')

# wire labels — right (min / max only)
rx = LEFT_PAD + n_cols * STEP_WIDTH + 12
for i, lbl in [(0, "min"), (N - 1, "max")]:
    y = wy(i)
    L.append(f'  <text x="{rx}" y="{y + 5}" text-anchor="start"')
    L.append(f'        font-family="monospace" font-size="11" fill="{DIM_COLOR}">{lbl}</text>')

# horizontal wires
for i in range(N):
    y = wy(i)
    L.append(f'  <line x1="{LEFT_PAD - 8}" y1="{y}" x2="{LEFT_PAD + n_cols * STEP_WIDTH + 8}" y2="{y}"')
    L.append(f'        stroke="{WIRE_COLOR}" stroke-width="1.5"/>')

# phase background bands + k= labels
for ph, cols in phase_ranges.items():
    if not cols:
        continue
    color  = PHASE_COLORS[ph]
    x0     = LEFT_PAD + min(cols) * STEP_WIDTH + 3
    x1     = LEFT_PAD + (max(cols) + 1) * STEP_WIDTH - 3
    band_y = TOP_PAD - 18
    band_h = (N - 1) * WIRE_SPACING + 28
    k_val  = [2, 4, 8][ph]
    L.append(f'  <rect x="{x0}" y="{band_y}" width="{x1-x0}" height="{band_h}"')
    L.append(f'        rx="4" fill="{color}" fill-opacity="0.06"')
    L.append(f'        stroke="{color}" stroke-opacity="0.25" stroke-width="1"/>')
    lx = (x0 + x1) / 2
    ly = band_y + band_h + 18
    L.append(f'  <text x="{lx:.0f}" y="{ly}" text-anchor="middle"')
    L.append(f'        font-family="monospace" font-size="11" fill="{color}">k={k_val}</text>')

# comparators
for ci, col in enumerate(columns):
    ph    = phase_of_col[ci]
    color = PHASE_COLORS[ph]
    n_in_col = len(col)
    col_cx   = cx(ci)

    # thin dashed spine through the column centre — marks one parallel step
    all_wires = [w for s in col for w in (s[0], s[1])]
    spine_y1  = wy(min(all_wires))
    spine_y2  = wy(max(all_wires))
    L.append(f'  <line x1="{col_cx:.1f}" y1="{spine_y1}" x2="{col_cx:.1f}" y2="{spine_y2}"')
    L.append(f'        stroke="{color}" stroke-width="0.75" stroke-opacity="0.3"')
    L.append(f'        stroke-dasharray="3,4"/>')

    for si, (lo, hi, ascending, k) in enumerate(col):
        # spread comparators horizontally within the column
        if n_in_col > 1:
            x = col_cx + (si - (n_in_col - 1) / 2) * SLOT_W
        else:
            x = col_cx
        y_lo = wy(lo)
        y_hi = wy(hi)

        if ascending:
            # arrow points down (toward hi wire = smaller index after sort)
            tip_y   = y_hi
            stem_y1 = y_lo + DOT_R
            stem_y2 = y_hi - ARR_H
        else:
            # arrow points up (toward lo wire)
            tip_y   = y_lo
            stem_y1 = y_lo + ARR_H
            stem_y2 = y_hi - DOT_R

        # stem (stops before arrowhead so they don't overlap)
        L.append(f'  <line x1="{x:.1f}" y1="{stem_y1}" x2="{x:.1f}" y2="{stem_y2}"')
        L.append(f'        stroke="{color}" stroke-width="2"/>')

        # arrowhead polygon
        if ascending:
            L.append(arrow_down(x, tip_y, color))
        else:
            L.append(arrow_up(x, tip_y, color))

        # endpoint dots (drawn last so they appear on top of stem)
        L.append(f'  <circle cx="{x:.1f}" cy="{y_lo}" r="{DOT_R}" fill="{color}"/>')
        L.append(f'  <circle cx="{x:.1f}" cy="{y_hi}" r="{DOT_R}" fill="{color}"/>')

L.append('</svg>')

out  = "\n".join(L)
path = __file__.replace("gen_network.py", "bitonic_network.svg")
with open(path, "w") as f:
    f.write(out)
print(f"Written {path}  ({width}×{height}px, {n_cols} columns)")
