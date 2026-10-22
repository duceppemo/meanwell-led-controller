"""Generate docs/images/pin_maps.svg: header pinouts of the four supported boards
with the firmware's default role for each pin."""
import sys
from xml.sax.saxutils import escape

OUT = sys.argv[1]

# role -> (fill, stroke, text colour)
ROLE = {
    'ch':    ('#fff4cc', '#d4a017', '#7a5a00'),
    'ow':    ('#ddf4ff', '#0969da', '#0550ae'),
    'ui':    ('#eaeef2', '#8c959f', '#424a53'),
    'relay': ('#fbefff', '#8250df', '#6639ba'),
    'avoid': ('#ffebe9', '#cf222e', '#a40e26'),
}
PWR = {'3V3': '#cf222e', '5V': '#cf222e', 'GND': '#1f2328', 'G': '#1f2328', 'EN': '#57606a', 'RST': '#57606a'}

def pin(name, role=None, tag=None):
    return (name, role, tag)

# ---------- boards: (title, env, subtitle, left pins, right pins, top/bottom, module label, notes) ----------
XIAO = dict(
    title='Seeed XIAO ESP32-C3', env='xiao-c3',
    usb='top', module='ESP32-C3',
    left=[pin('D0·IO2', 'avoid', 'strapping'), pin('D1·IO3', 'ch', 'CH1'), pin('D2·IO4', 'ch', 'CH2'),
          pin('D3·IO5', 'ch', 'CH3'), pin('D4·IO6', 'ch', 'CH4'), pin('D5·IO7', 'ch', 'CH5'),
          pin('D6·IO21', 'ow', '1-Wire')],
    right=[pin('5V'), pin('GND'), pin('3V3'), pin('D10·IO10', 'ch', 'CH6'), pin('D9·IO9', 'ui', 'BOOT btn'),
           pin('D8·IO8', 'avoid', 'strapping'), pin('D7·IO20', 'relay', 'relay?')],
    notes=['Front view, USB-C up. One channel (D1) enabled by default.',
           'No user LED. Attach the u.FL antenna.'],
)
C3 = dict(
    title='ESP32-C3-DevKitM-1', env='esp32-c3',
    usb='bottom', module='C3-MINI-1',
    left=[pin('GND'), pin('3V3'), pin('3V3'), pin('IO2', 'avoid', 'strapping'), pin('IO3', 'ch', 'CH1'),
          pin('GND'), pin('RST'), pin('GND'), pin('IO0'), pin('IO1', 'ow', '1-Wire'),
          pin('IO10', 'ch', 'CH6'), pin('GND'), pin('5V'), pin('5V'), pin('GND')],
    right=[pin('GND'), pin('TX·IO21', 'ui', 'UART'), pin('RX·IO20', 'ui', 'UART'), pin('GND'),
           pin('IO9', 'ui', 'BOOT btn'), pin('IO8', 'ui', 'RGB LED'), pin('GND'), pin('IO7', 'ch', 'CH5'),
           pin('IO6', 'ch', 'CH4'), pin('IO5', 'ch', 'CH3'), pin('IO4', 'ch', 'CH2'), pin('GND'),
           pin('IO18', 'avoid', 'USB D−'), pin('IO19', 'avoid', 'USB D+'), pin('GND')],
    notes=['Top view, module up. J1 left, J3 right.',
           'IO8 (RGB LED) and IO9 (BOOT) are also strapping pins.'],
)
DEVKITC = dict(
    title='ESP32-DevKitC V4 (WROOM-32)', env='esp32dev',
    usb='bottom', module='WROOM-32',
    left=[pin('3V3'), pin('EN'), pin('VP·IO36', 'avoid', 'input only'), pin('VN·IO39', 'avoid', 'input only'),
          pin('IO34', 'avoid', 'input only'), pin('IO35', 'avoid', 'input only'), pin('IO32', 'ch', 'CH5'),
          pin('IO33', 'ch', 'CH6'), pin('IO25', 'ch', 'CH1'), pin('IO26', 'ch', 'CH2'), pin('IO27', 'ch', 'CH3'),
          pin('IO14', 'ch', 'CH4'), pin('IO12', 'avoid', 'strapping'), pin('GND'), pin('IO13', 'ch', 'CH7'),
          pin('D2·IO9', 'avoid', 'flash'), pin('D3·IO10', 'avoid', 'flash'), pin('CMD·IO11', 'avoid', 'flash'),
          pin('5V')],
    right=[pin('GND'), pin('IO23', 'ch', 'CH8'), pin('IO22'), pin('TX·IO1', 'ui', 'UART'), pin('RX·IO3', 'ui', 'UART'),
           pin('IO21'), pin('GND'), pin('IO19'), pin('IO18'), pin('IO5'), pin('IO17'), pin('IO16'),
           pin('IO4', 'ow', '1-Wire'), pin('IO0', 'ui', 'BOOT btn'), pin('IO2', 'ui', 'status LED'),
           pin('IO15', 'avoid', 'strapping'), pin('D1·IO8', 'avoid', 'flash'), pin('D0·IO7', 'avoid', 'flash'),
           pin('CLK·IO6', 'avoid', 'flash')],
    notes=['Top view, antenna up. J2 left, J3 right.',
           '30-pin DOIT boards order pins differently:',
           'wire by GPIO number, not position.'],
)
S3 = dict(
    title='ESP32-S3-DevKitC-1', env='esp32-s3',
    usb='bottom', module='S3-WROOM-1',
    left=[pin('3V3'), pin('3V3'), pin('RST'), pin('IO4', 'ch', 'CH1'), pin('IO5', 'ch', 'CH2'), pin('IO6', 'ch', 'CH3'),
          pin('IO7', 'ch', 'CH4'), pin('IO15', 'ch', 'CH5'), pin('IO16', 'ch', 'CH6'), pin('IO17', 'ch', 'CH7'),
          pin('IO18', 'ch', 'CH8'), pin('IO8', 'ow', '1-Wire'), pin('IO3', 'avoid', 'strapping'),
          pin('IO46', 'avoid', 'strapping'), pin('IO9'), pin('IO10'), pin('IO11'), pin('IO12'), pin('IO13'),
          pin('IO14'), pin('5V'), pin('G')],
    right=[pin('G'), pin('TX·IO43', 'ui', 'UART'), pin('RX·IO44', 'ui', 'UART'), pin('IO1'), pin('IO2'),
           pin('IO42'), pin('IO41'), pin('IO40'), pin('IO39'), pin('IO38', 'ui', 'RGB v1.1'),
           pin('IO37', 'avoid', 'PSRAM*'), pin('IO36', 'avoid', 'PSRAM*'), pin('IO35', 'avoid', 'PSRAM*'),
           pin('IO0', 'ui', 'BOOT btn'), pin('IO45', 'avoid', 'strapping'), pin('IO48', 'ui', 'RGB v1.0'),
           pin('IO47'), pin('IO21'), pin('IO20', 'avoid', 'USB D+'), pin('IO19', 'avoid', 'USB D−'),
           pin('G'), pin('G')],
    notes=['Top view, module up. J1 left, J3 right.',
           'Status blink uses GPIO 48: works on v1.0 boards only.',
           '* used by octal PSRAM on N8R8 / N16R8 modules.'],
)

W = 960
PITCH = 19
PANEL_W = 445
TOP = 96          # from panel top to first pin row
BOARD_W = 78

out = []
def add(s): out.append(s)

def text(x, y, s, cls='', anchor='start', fill=None, weight=None, size=None):
    a = f' text-anchor="{anchor}"' if anchor != 'start' else ''
    f = f' fill="{fill}"' if fill else ''
    w = f' font-weight="{weight}"' if weight else ''
    z = f' font-size="{size}"' if size else ''
    c = f' class="{cls}"' if cls else ''
    add(f'<text{c} x="{x}" y="{y}"{a}{f}{w}{z}>{escape(s)}</text>')

def chip(x_right, y, label, role, align='right'):
    fill, stroke, tc = ROLE[role]
    w = 8 + 6.2 * len(label)
    x = x_right - w if align == 'right' else x_right
    add(f'<rect x="{x:.1f}" y="{y-7.5:.1f}" width="{w:.1f}" height="15" rx="7.5" fill="{fill}" stroke="{stroke}" stroke-width="1"/>')
    text(f'{x + w/2:.1f}', f'{y+3.8:.1f}', label, 'chip', 'middle', tc)

def panel(px, py, b, rows):
    ph = TOP + rows * PITCH + 34 + 15 * len(b['notes'])
    add(f'<rect class="panel" x="{px}" y="{py}" width="{PANEL_W}" height="{ph}" rx="8"/>')
    text(px + 16, py + 24, b['title'], 'h')
    text(px + PANEL_W - 16, py + 24, f"env: {b['env']}", 'env', 'end')
    cx = px + PANEL_W / 2
    bx = cx - BOARD_W / 2
    n = max(len(b['left']), len(b['right']))
    y0 = py + TOP
    btop = y0 - 46
    bh = n * PITCH + 46 + 18
    # board
    add(f'<rect x="{bx}" y="{btop}" width="{BOARD_W}" height="{bh}" rx="6" fill="#1f5135" stroke="#163d28" stroke-width="1.2"/>')
    if b['usb'] == 'top':
        add(f'<rect x="{cx-16}" y="{btop-8}" width="32" height="14" rx="3" fill="#afb8c1" stroke="#6e7781"/>')
        add(f'<rect x="{bx+12}" y="{btop+12}" width="{BOARD_W-24}" height="26" rx="2" fill="#d0d7de" stroke="#8c959f"/>')
        text(cx, btop + 29, b['module'], 'mod', 'middle')
    else:
        add(f'<rect x="{bx+8}" y="{btop+6}" width="{BOARD_W-16}" height="34" rx="2" fill="#d0d7de" stroke="#8c959f"/>')
        add(f'<path d="M{bx+14},{btop+13} h8 v6 h8 v-6 h8 v6 h8 v-6 h8 v6 h8" fill="none" stroke="#8c959f" stroke-width="1.2"/>')
        text(cx, btop + 34, b['module'], 'mod', 'middle')
        add(f'<rect x="{cx-16}" y="{btop+bh-8}" width="32" height="14" rx="3" fill="#afb8c1" stroke="#6e7781"/>')
    # pins
    for side, pins in (('L', b['left']), ('R', b['right'])):
        for i, (name, role, tag) in enumerate(pins):
            y = y0 + i * PITCH
            x = bx + 7 if side == 'L' else bx + BOARD_W - 7
            add(f'<circle cx="{x}" cy="{y}" r="4" fill="#d4a017" stroke="#9a6700" stroke-width="1"/>')
            col = PWR.get(name)
            if side == 'L':
                text(bx - 8, y + 4, name, 'pn', 'end', col, '600' if col else None)
                if role: chip(bx - 76, y, tag, role, 'right')
            else:
                text(bx + BOARD_W + 8, y + 4, name, 'pn', 'start', col, '600' if col else None)
                if role: chip(bx + BOARD_W + 76, y, tag, role, 'left')
    ny = btop + bh + 22
    for k, note in enumerate(b['notes']):
        text(px + 16, ny + 15 * k, note, 'note')
    return ph

# layout
rows1 = max(len(XIAO['left']), len(C3['left']))
rows1 = 15
rows2 = 22
X1, X2 = 25, 490
Y1 = 64
body = []
h1 = TOP + rows1 * PITCH + 34 + 15 * 3
Y2 = Y1 + h1 + 16
h2 = TOP + rows2 * PITCH + 34 + 15 * 3
LEG_Y = Y2 + h2 + 18
H = LEG_Y + 58

add(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="system-ui, -apple-system, Segoe UI, Roboto, sans-serif" font-size="13">')
add('<title>Default pin maps of the supported boards</title>')
add('<desc>Header pinouts of the Seeed XIAO ESP32-C3, ESP32-C3-DevKitM-1, ESP32-DevKitC V4 and ESP32-S3-DevKitC-1, each pin tagged with its default role in this firmware: PWM channels, 1-Wire, button, LEDs, UART, and pins to avoid (strapping, flash, USB, input only, PSRAM).</desc>')
add('''<style>
  .panel{fill:#ffffff;stroke:#8c959f;stroke-width:1.2}
  .h{font-weight:600;font-size:15px;fill:#1f2328}
  .env{font-size:11px;fill:#0969da;font-family:ui-monospace, Consolas, monospace}
  .pn{font-size:11px;fill:#1f2328;font-family:ui-monospace, Consolas, monospace}
  .chip{font-size:10px;font-weight:600}
  .mod{font-size:8px;fill:#424a53;font-weight:600}
  .note{font-size:11px;fill:#57606a;font-style:italic}
  .s{font-size:11px;fill:#57606a}
</style>''')
add(f'<rect width="{W}" height="{H}" fill="#f6f8fa" rx="12"/>')
text(25, 38, 'Default pin maps', 'h', size=17)
text(190, 38, 'header order as printed on each board; tags show the firmware default for that pin (all editable in Settings)', 's')

# XIAO panel same height as C3 panel; centre its board vertically by using its own rows
panel(X1, Y1, XIAO, rows1)
panel(X2, Y1, C3, rows1)
panel(X1, Y2, DEVKITC, rows2)
panel(X2, Y2, S3, rows2)

# legend
add(f'<rect class="panel" x="25" y="{LEG_Y}" width="910" height="40" rx="8"/>')
lx = 44
for label, role in (('CH1–CH8  PWM channel', 'ch'), ('1-Wire  DS18B20', 'ow'), ('button · LED · UART', 'ui'),
                    ('suggested relay pin', 'relay'), ('avoid: strapping · flash · USB · input only', 'avoid')):
    fill, stroke, tc = ROLE[role]
    add(f'<rect x="{lx}" y="{LEG_Y+13}" width="22" height="14" rx="7" fill="{fill}" stroke="{stroke}"/>')
    text(lx + 28, LEG_Y + 24, label, 's')
    lx += 34 + 6.0 * len(label)
add('</svg>')

open(OUT, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
print('wrote', OUT, 'height', H)
