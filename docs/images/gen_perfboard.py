"""Generate docs/images/perfboard.svg: a single-channel XIAO ESP32-C3 controller on
2.54 mm perfboard, component side and solder side, with a netlist check.

Usage: python gen_perfboard.py docs/images/perfboard.svg

Holes are (column, row) from the top-left of the component side. Columns are lettered
A.. and rows numbered 1.. in the drawing. The solder side is the same board flipped
left to right, so it is drawn mirrored.
"""
import sys
from xml.sax.saxutils import escape

OUT = sys.argv[1]
COLS, ROWS = 20, 18

# ---------------------------------------------------------------- nets
NET_COLOUR = {
    'GND': '#1f2328', '3V3': '#cf222e', '5V': '#bc4c00', 'PWM': '#d4a017',
    'BASE': '#8c959f', 'DIM+': '#8250df', '1W': '#0969da',
    'KA': '#1a7f37', 'KB': '#1a7f37', 'KSW': '#1a7f37',
}
NET_NAME = [('GND', 'GND'), ('3V3', '3.3 V'), ('5V', '5 V'), ('PWM', 'PWM (D1)'),
            ('BASE', 'Q1 base'), ('DIM+', 'DIM+'), ('1W', '1-Wire (D6)'), ('KA', 'knob D2–D4')]

# wires on the solder side: polylines through hole centres, orthogonal segments only
TRACES = {
    'KA':   [[(2, 3), (0, 3)]],
    'KB':   [[(2, 4), (0, 4)]],
    'KSW':  [[(2, 5), (0, 5)]],
    'GND':  [[(0, 6), (0, 17), (18, 17)], [(8, 2), (9, 2), (9, 17)],
             [(3, 16), (3, 17)], [(18, 12), (18, 17)]],
    '5V':   [[(8, 1), (10, 1), (10, 15), (11, 15), (11, 16)]],
    '3V3':  [[(8, 3), (7, 3), (7, 16), (5, 16)], [(7, 9), (8, 9)], [(7, 11), (6, 11)]],
    '1W':   [[(2, 7), (2, 13), (4, 13), (4, 16)]],
    'PWM':  [[(2, 2), (3, 2), (3, 0), (11, 0), (11, 10)]],
    'BASE': [[(12, 9), (17, 9), (17, 12)], [(15, 10), (17, 10)]],
    'DIM+': [[(16, 12), (16, 16)]],
}

# component pins: name -> list of (hole, net or None for not connected, pin label)
XL = ['D0', 'D1', 'D2', 'D3', 'D4', 'D5', 'D6']
XR = ['5V', 'GND', '3V3', 'D10', 'D9', 'D8', 'D7']
XL_NET = {'D1': 'PWM', 'D2': 'KA', 'D3': 'KB', 'D4': 'KSW', 'D6': '1W'}
XR_NET = {'5V': '5V', 'GND': 'GND', '3V3': '3V3'}
PARTS = {
    'U1': [((2, i + 1), XL_NET.get(n), n) for i, n in enumerate(XL)]
        + [((8, i + 1), XR_NET.get(n), n) for i, n in enumerate(XR)],
    'J4': [((0, 3), 'KA', 'CLK'), ((0, 4), 'KB', 'DT'), ((0, 5), 'KSW', 'SW'), ((0, 6), 'GND', 'GND')],
    'J3': [((3, 16), 'GND', 'GND'), ((4, 16), '1W', 'DQ'), ((5, 16), '3V3', 'VDD')],
    'R3': [((2, 11), '1W', ''), ((6, 11), '3V3', '')],
    'R2': [((8, 9), '3V3', ''), ((12, 9), 'BASE', '')],
    'R1': [((11, 10), 'PWM', ''), ((15, 10), 'BASE', '')],
    'Q1': [((16, 12), 'DIM+', 'C'), ((17, 12), 'BASE', 'B'), ((18, 12), 'GND', 'E')],
    'C1': [((9, 13), 'GND', '−'), ((10, 13), '5V', '+')],
    'J1': [((9, 16), 'GND', 'GND'), ((11, 16), '5V', '+5V')],
    'J2': [((16, 16), 'DIM+', 'DIM+'), ((18, 16), 'GND', 'DIM−')],
}


# ---------------------------------------------------------------- netlist check
def expand(poly):
    holes = [poly[0]]
    for (c0, r0), (c1, r1) in zip(poly, poly[1:]):
        assert c0 == c1 or r0 == r1, f'diagonal segment {poly}'
        dc, dr = (c1 > c0) - (c1 < c0), (r1 > r0) - (r1 < r0)
        c, r = c0, r0
        while (c, r) != (c1, r1):
            c, r = c + dc, r + dr
            holes.append((c, r))
    return holes


def check():
    owner = {}          # hole -> net, from traces
    parent = {}

    def find(h):
        while parent.setdefault(h, h) != h:
            h = parent[h]
        return h

    for net, polys in TRACES.items():
        for poly in polys:
            hs = expand(poly)
            for h in hs:
                assert 0 <= h[0] < COLS and 0 <= h[1] < ROWS, f'{net} leaves the board at {h}'
                assert owner.setdefault(h, net) == net, f'short: {net} and {owner[h]} share hole {h}'
            for a, b in zip(hs, hs[1:]):
                parent[find(a)] = find(b)

    used = {}
    for part, pins in PARTS.items():
        for hole, net, label in pins:
            assert hole not in used, f'{part} and {used[hole]} use hole {hole}'
            used[hole] = part
            got = owner.get(hole)
            assert got == net, f'{part}.{label} at {hole}: expected {net}, trace says {got}'

    for net in TRACES:
        roots = {find(h) for part in PARTS.values() for h, n, _ in part if n == net}
        assert len(roots) == 1, f'net {net} is split into {len(roots)} pieces'
    print('netlist OK:', ', '.join(f'{n} {sum(1 for p in PARTS.values() for _, m, _ in p if m == n)} pins'
                                   for n in TRACES))


check()

# ---------------------------------------------------------------- drawing
P = 20                       # px per 2.54 mm
TOP = (66, 116)              # origin of hole (0,0), component side
BOT = (530, 116)             # origin of the left-most drawn hole, solder side
out = []
w = out.append


def xy(view, c, r):
    ox, oy = TOP if view == 'top' else BOT
    cc = c if view == 'top' else COLS - 1 - c
    return ox + cc * P, oy + r * P


def board(view, title, sub):
    ox, oy = TOP if view == 'top' else BOT
    x0, y0 = ox - 0.8 * P, oy - 0.8 * P
    bw, bh = (COLS - 1 + 1.6) * P, (ROWS - 1 + 1.6) * P
    w(f'<text class="t h" x="{x0}" y="{oy - 52}">{escape(title)}</text>')
    w(f'<text class="s" x="{x0}" y="{oy - 38}">{escape(sub)}</text>')
    w(f'<rect x="{x0}" y="{y0}" width="{bw}" height="{bh}" rx="6" fill="#e9d7a5" stroke="#b08d3c" stroke-width="1.2"/>')
    for c in range(COLS):
        x, _ = xy(view, c, 0)
        w(f'<text class="g" x="{x}" y="{y0 - 5}" text-anchor="middle">{chr(65 + c)}</text>')
    for r in range(ROWS):
        _, y = xy(view, 0, r)
        lx = x0 - 4
        w(f'<text class="g" x="{lx}" y="{y + 3.5}" text-anchor="end">{r + 1}</text>')
    for c in range(COLS):
        for r in range(ROWS):
            x, y = xy(view, c, r)
            if view == 'top':
                w(f'<circle cx="{x}" cy="{y}" r="2.6" fill="#8a6d2b"/>')
            else:
                w(f'<circle cx="{x}" cy="{y}" r="6" fill="#d9a441" stroke="#b07d1e" stroke-width="0.8"/>'
                  f'<circle cx="{x}" cy="{y}" r="2.2" fill="#6b5420"/>')


def traces(view):
    for net, polys in TRACES.items():
        col = NET_COLOUR[net]
        for poly in polys:
            pts = ' '.join('%g,%g' % xy(view, c, r) for c, r in poly)
            if view == 'top':
                w(f'<polyline points="{pts}" fill="none" stroke="{col}" stroke-width="2" '
                  f'stroke-dasharray="4 3" stroke-linecap="round" stroke-linejoin="round" opacity="0.55"/>')
            else:
                w(f'<polyline points="{pts}" fill="none" stroke="{col}" stroke-width="5.5" '
                  f'stroke-linecap="round" stroke-linejoin="round"/>')


def joints():
    for part, pins in PARTS.items():
        for (c, r), net, _ in pins:
            x, y = xy('bot', c, r)
            if net is None:
                w(f'<circle cx="{x}" cy="{y}" r="5" fill="#afb8c1" stroke="#6e7781" stroke-width="1"/>')
            else:
                w(f'<circle cx="{x}" cy="{y}" r="5.5" fill="#d0d7de" stroke="#57606a" stroke-width="1.2"/>')


def resistor(part, value):
    (a, _, _), (b, _, _) = PARTS[part]
    x1, y = xy('top', *a)
    x2, _ = xy('top', *b)
    w(f'<path d="M{x1},{y} L{x2},{y}" stroke="#8c959f" stroke-width="2"/>')
    bx, bl = (x1 + x2) / 2 - 24, 48
    w(f'<rect x="{bx}" y="{y - 7}" width="{bl}" height="14" rx="6" fill="#e6c98f" stroke="#8a6d2b" stroke-width="1"/>')
    for i, colour in enumerate(BANDS[value]):
        w(f'<rect x="{bx + 9 + i * 7}" y="{y - 7}" width="4" height="14" fill="{colour}"/>')
    w(f'<rect x="{bx + bl - 12}" y="{y - 7}" width="4" height="14" fill="#c69026"/>')
    for c in (x1, x2):
        w(f'<circle cx="{c}" cy="{y}" r="3.2" fill="#afb8c1" stroke="#57606a"/>')
    w(f'<text class="lbl" x="{(x1 + x2) / 2}" y="{y - 10}" text-anchor="middle">{part} {escape(value)}</text>')


BANDS = {  # 4-band colour code, gold tolerance drawn separately
    '1 kΩ': ['#8b4513', '#1f2328', '#cf222e'],
    '10 kΩ': ['#8b4513', '#1f2328', '#e36209'],
    '4.7 kΩ': ['#d4a017', '#8250df', '#cf222e'],
}


def header(part, horizontal, sub=None):
    pins = PARTS[part]
    xs = [xy('top', *h)[0] for h, _, _ in pins]
    ys = [xy('top', *h)[1] for h, _, _ in pins]
    x0, x1, y0, y1 = min(xs) - 9, max(xs) + 9, min(ys) - 9, max(ys) + 9
    w(f'<rect x="{x0}" y="{y0}" width="{x1 - x0}" height="{y1 - y0}" rx="2" fill="#24292f"/>')
    for (h, _, label), x, y in zip(pins, xs, ys):
        w(f'<rect x="{x - 2.5}" y="{y - 2.5}" width="5" height="5" fill="#d4a72c"/>')
    return x0, x1, y0, y1


def terminal(part, label):
    (a, _, la), (b, _, lb) = PARTS[part]
    xa, y = xy('top', *a)
    xb, _ = xy('top', *b)
    x0, x1 = xa - 0.95 * P, xb + 0.95 * P
    y0, y1 = y - 1.1 * P, y + 1.75 * P
    w(f'<rect x="{x0}" y="{y0}" width="{x1 - x0}" height="{y1 - y0}" rx="3" fill="#3fa266" stroke="#1a7f37" stroke-width="1.2"/>')
    for x, l in ((xa, la), (xb, lb)):
        w(f'<circle cx="{x}" cy="{y - 0.25 * P}" r="7" fill="#d0d7de" stroke="#57606a"/>'
          f'<path d="M{x - 4.5},{y - 0.25 * P - 4.5} L{x + 4.5},{y - 0.25 * P + 4.5}" stroke="#57606a" stroke-width="1.6"/>')
        w(f'<rect x="{x - 6}" y="{y1 - 9}" width="12" height="7" rx="1" fill="#1a5c37"/>')
        w(f'<text class="tiny" x="{x}" y="{y + 0.68 * P}" text-anchor="middle" fill="#ffffff">{escape(l)}</text>')
    w(f'<text class="lbl" x="{(xa + xb) / 2}" y="{y0 - 4}" text-anchor="middle">{escape(label)}</text>')


def component_side():
    board('top', 'Component side', 'parts on top · dashed lines are the wires underneath')
    traces('top')
    ox, oy = TOP
    # XIAO on female headers
    xl, yt = xy('top', 2, 1)
    xr, yb = xy('top', 8, 7)
    mx0, mx1 = xl - 0.5 * P, xr + 0.5 * P
    my0, my1 = yt - 1.13 * P, yb + 1.13 * P
    for x in (xl, xr):
        w(f'<rect x="{x - 9}" y="{yt - 10}" width="18" height="{yb - yt + 20}" fill="#57606a"/>')
    w(f'<rect x="{mx0}" y="{my0}" width="{mx1 - mx0}" height="{my1 - my0}" rx="5" fill="#24292f" fill-opacity="0.9"/>')
    w(f'<rect x="{(mx0 + mx1) / 2 - 18}" y="{my0 - 6}" width="36" height="16" rx="5" fill="#afb8c1" stroke="#6e7781"/>')
    w(f'<rect x="{(mx0 + mx1) / 2 - 30}" y="{my0 + 22}" width="60" height="44" rx="2" fill="#57606a" fill-opacity="0.8"/>')
    w(f'<text x="{(mx0 + mx1) / 2}" y="{my0 + 41}" text-anchor="middle" font-size="9" font-weight="600" fill="#ffffff">XIAO</text>')
    w(f'<text x="{(mx0 + mx1) / 2}" y="{my0 + 53}" text-anchor="middle" font-size="9" fill="#ffffff">ESP32-C3</text>')
    w(f'<text x="{(mx0 + mx1) / 2}" y="{my1 - 8}" text-anchor="middle" font-size="9" fill="#afb8c1">U1 · USB-C up</text>')
    for (c, r), net, label in PARTS['U1']:
        x, y = xy('top', c, r)
        w(f'<circle cx="{x}" cy="{y}" r="3" fill="#d4a72c"/>')
        dx, anchor = (8, 'start') if c == 2 else (-8, 'end')
        fill = '#ffffff' if net else '#8c959f'
        w(f'<text class="tiny" x="{x + dx}" y="{y + 3}" text-anchor="{anchor}" fill="{fill}">{label}</text>')
    # J4 knob header, right-angle, pointing off the left edge
    x0, x1, y0, y1 = header('J4', False)
    for h, _, label in PARTS['J4']:
        x, y = xy('top', *h)
        w(f'<path d="M{x - 9},{y} L{x - 22},{y}" stroke="#d4a72c" stroke-width="3"/>')
    w(f'<text class="lbl" x="{x0 - 26}" y="{y0 - 6}">J4 knob</text>')
    for h, _, label in PARTS['J4']:
        x, y = xy('top', *h)
    # J3 sensor header
    x0, x1, y0, y1 = header('J3', True)
    w(f'<text class="lbl" x="{(x0 + x1) / 2}" y="{y0 - 5}" text-anchor="middle">J3 DS18B20</text>')
    for h, _, label in PARTS['J3']:
        x, y = xy('top', *h)
        w(f'<text class="tiny" x="{x}" y="{y1 + 10}" text-anchor="middle">{label}</text>')
    resistor('R1', '1 kΩ')
    resistor('R2', '10 kΩ')
    resistor('R3', '4.7 kΩ')
    # Q1 TO-92, flat face toward the bottom edge: C B E left to right
    xc, y = xy('top', 16, 12)
    xe, _ = xy('top', 18, 12)
    xm = (xc + xe) / 2
    w(f'<path d="M{xm - 26},{y + 6} L{xm + 26},{y + 6} A27,27 0 0 0 {xm - 26},{y + 6} Z" fill="#24292f"/>')
    for (c, r), _, label in PARTS['Q1']:
        x, _ = xy('top', c, r)
        w(f'<circle cx="{x}" cy="{y}" r="2.6" fill="#afb8c1"/>')
        w(f'<text class="tiny" x="{x}" y="{y + 17}" text-anchor="middle">{label}</text>')
    w(f'<text class="lbl" x="{xm}" y="{y - 26}" text-anchor="middle">Q1 BC547</text>')
    w(f'<text class="tiny" x="{xm}" y="{y - 11}" text-anchor="middle" fill="#afb8c1">flat ↓</text>')
    # C1 radial electrolytic, minus stripe toward GND (left)
    xg, y = xy('top', 9, 13)
    xp, _ = xy('top', 10, 13)
    xm = (xg + xp) / 2
    w(f'<circle cx="{xm}" cy="{y}" r="14" fill="#0b3d91" stroke="#24292f"/>')
    w(f'<path d="M{xm - 7},{y - 11.6} A14,14 0 0 0 {xm - 7},{y + 11.6} Z" fill="#c9d1d9"/>')
    w(f'<text x="{xm - 10.5}" y="{y + 3}" text-anchor="middle" font-size="9" font-weight="700" fill="#24292f">−</text>')
    w(f'<text class="lbl" x="{xm + 18}" y="{y + 4}">C1</text>')
    terminal('J1', 'J1 5 V in')
    terminal('J2', 'J2 to driver')


def solder_side():
    board('bot', 'Solder side (board flipped left to right)', 'wires and solder joints · build from this view')
    xl, yt = xy('bot', 2, 1)
    xr, yb = xy('bot', 8, 7)
    w(f'<rect x="{min(xl, xr) - 0.5 * P}" y="{yt - 1.13 * P}" width="{abs(xr - xl) + P}" height="{yb - yt + 2.26 * P}" '
      f'rx="5" fill="none" stroke="#57606a" stroke-width="1.2" stroke-dasharray="5 4"/>')
    xm, _ = xy('bot', 5, 0)
    w(f'<text class="tiny" x="{xm}" y="{yt + 3.2 * P}" text-anchor="middle" fill="#57606a">XIAO</text>')
    w(f'<text class="tiny" x="{xm}" y="{yt + 3.7 * P}" text-anchor="middle" fill="#57606a">above</text>')
    traces('bot')
    joints()
    # pin labels next to connector and XIAO joints
    def tag(c, r, text, side, dy=3.5):
        x, y = xy('bot', c, r)
        dx, anchor = {'r': (9, 'start'), 'l': (-9, 'end')}[side]
        w(f'<text class="tiny" x="{x + dx}" y="{y + dy}" text-anchor="{anchor}">{escape(text)}</text>')
    for (c, r), net, label in PARTS['U1']:
        if net:
            tag(c, r, label, 'r' if c == 2 else 'l', dy=-5)
    for (c, r), _, label in PARTS['J4']:
        tag(c, r, label, 'r')
    x, y = xy('bot', 0, 2)
    w(f'<text class="lbl" x="{x + 10}" y="{y + 4}">J4</text>')
    for name, (c, r) in (('J3', (5.5, 14.5)), ('J1', (13, 16)), ('J2', (17, 15.5))):
        x, y = xy('bot', c, r)
        w(f'<text class="lbl" x="{x}" y="{y + 4}" text-anchor="middle">{name}</text>')
    for name, (c, r), dy in (('R1', (13, 10), 4), ('R2', (12, 8.5), 4), ('R3', (4, 11), 4),
                             ('Q1', (17, 13.5), 4), ('C1', (8, 13), 4)):
        x, y = xy('bot', c, r)
        w(f'<text class="lbl" x="{x}" y="{y + dy}" text-anchor="middle" fill="#57606a">{name}</text>')


W, H = 960, 918
w(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" '
  f'font-family="system-ui, -apple-system, Segoe UI, Roboto, sans-serif" font-size="13">')
w('<title>Perfboard layout, one XIAO ESP32-C3 channel</title>')
w('<desc>A XIAO ESP32-C3 controller for one Mean Well driver on a 20 by 18 hole piece of 2.54 mm perfboard. '
  'Component side: XIAO on female headers at the top left with USB-C at the top edge, knob header J4 on the left edge, '
  'DS18B20 header J3, 5 V input terminal J1 and driver terminal J2 along the bottom edge, R1 1 kOhm, R2 10 kOhm, '
  'R3 4.7 kOhm, Q1 BC547 and C1 100 uF. Solder side: the same board flipped left to right with every wire coloured by net.</desc>')
w('''<defs><style>
  .box{fill:#ffffff;stroke:#8c959f;stroke-width:1.2}
  .t{fill:#1f2328}
  .h{font-weight:600;font-size:14px}
  .s{fill:#57606a;font-size:11px}
  .g{fill:#8c959f;font-size:8px;font-family:ui-monospace, Consolas, monospace}
  .lbl{fill:#1f2328;font-size:10px;font-weight:600}
  .tiny{fill:#1f2328;font-size:8px;font-family:ui-monospace, Consolas, monospace}
</style></defs>''')
w(f'<rect width="{W}" height="{H}" fill="#f6f8fa" rx="12"/>')
w('<text class="t" x="40" y="30" font-weight="600" font-size="16">Perfboard prototype: one XIAO ESP32-C3, one driver</text>')

component_side()
solder_side()

# legend
lx, ly = 40, 504
w(f'<text class="s" x="{lx}" y="{ly}">Wires:</text>')
x = lx + 44
for net, name in NET_NAME:
    w(f'<path d="M{x},{ly - 4} L{x + 18},{ly - 4}" stroke="{NET_COLOUR[net]}" stroke-width="5" stroke-linecap="round"/>')
    w(f'<text class="s" x="{x + 24}" y="{ly}">{escape(name)}</text>')
    x += 24 + 7 * len(name) + 18
w(f'<text class="s" x="{lx}" y="{ly + 18}">Grey joints on the XIAO are unused pins: solder the header pin but run no wire to it. '
  'Resistor bodies and the XIAO bridge over the wires below them.</text>')

# parts card
cy = 542
w(f'<rect class="box" x="40" y="{cy}" width="430" height="356" rx="8"/>')
w(f'<text class="t h" x="56" y="{cy + 24}">Parts</text>')
rows = [
    ('U1', 'Seeed XIAO ESP32-C3, on 2 × 1×7 female headers'),
    ('Q1', 'BC547 NPN, flat face toward the bottom edge'),
    ('', 'or 2N7000: same holes, flat face toward the top,'),
    ('', 'and R1 becomes 100 Ω'),
    ('R1', '1 kΩ ¼ W, PWM to base'),
    ('R2', '10 kΩ ¼ W, base to 3.3 V (light off at boot)'),
    ('R3', '4.7 kΩ ¼ W, 1-Wire pull-up'),
    ('C1', '100 µF 10 V electrolytic, 2.5 mm pitch, optional'),
    ('J1', '2-way screw terminal 5.08 mm, 5 V supply'),
    ('J2', '2-way screw terminal 5.08 mm, to DIM+ / DIM−'),
    ('J3', '1×3 pin header, DS18B20 (GND · DQ · VDD)'),
    ('J4', '1×4 right-angle header, knob (CLK · DT · SW · GND)'),
    ('', 'Perfboard 2.54 mm, 20 × 18 holes (51 × 46 mm),'),
    ('', 'tinned copper wire 0.5 mm for the wires'),
]
y = cy + 48
for ref, text in rows:
    if ref:
        w(f'<text class="t" x="56" y="{y}" font-size="12" font-weight="600">{ref}</text>')
    w(f'<text class="t" x="92" y="{y}" font-size="12">{escape(text)}</text>')
    y += 21

# build card
w(f'<rect class="box" x="490" y="{cy}" width="430" height="356" rx="8"/>')
w(f'<text class="t h" x="506" y="{cy + 24}">Build and check</text>')
steps = [
    ('1', 'Solder the two female headers first. Never solder the XIAO'),
    ('', 'flat on the board: the PWM and 3.3 V wires run under it.'),
    ('2', 'Lay the wires on the solder side as drawn. That view is'),
    ('', 'mirrored, so J4 is on the right when the board is flipped.'),
    ('3', 'Fit R1–R3, Q1, C1 (stripe = −), the headers and terminals.'),
    ('4', 'Before plugging in the XIAO, check with a meter:'),
    ('', 'no short between 5 V, 3.3 V and GND, and none between'),
    ('', 'neighbouring wires. J2 DIM+ to DIM− should read open.'),
    ('5', 'Power from J1 or from USB-C, never both at once.'),
    ('6', 'J2: DIM+ to DIM+, DIM− to DIM−. Never the LED output.'),
    ('7', 'Settings → Channels: tick Invert, Min % 8 (XLG) or 6 (HLG).'),
    ('', 'The knob and 1-Wire pins already match the xiao-c3 build.'),
    ('', 'A KY-040 module’s + pin can stay unconnected.'),
    ('', 'Relay: D7 is free; wire a module to it by hand if needed.'),
]
y = cy + 48
for n, text in steps:
    if n:
        w(f'<text class="t" x="506" y="{y}" font-size="12" font-weight="600">{n}</text>')
    w(f'<text class="t" x="524" y="{y}" font-size="12">{escape(text)}</text>')
    y += 21

w('</svg>')
with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(out) + '\n')
print('wrote', OUT)
