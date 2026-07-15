#!/usr/bin/env python3
import csv, os, sys

csv_file = sys.argv[1] if len(sys.argv) > 1 else 'benchmark.csv'
out_dir = 'docs/figures'
os.makedirs(out_dir, exist_ok=True)

sizes = []
hit_rates = []
times = []

with open(csv_file, newline='') as f:
    # Skip any preamble lines until we find the header that contains 'buffer_size'
    header_line = None
    for line in f:
        if 'buffer_size' in line:
            header_line = line
            break
    if not header_line:
        print('CSV header not found in', csv_file, file=sys.stderr)
        sys.exit(1)
    # build a new reader starting from the header line
    cols = [c.strip() for c in header_line.strip().split(',')]
    reader = csv.DictReader(f, fieldnames=cols)
    for row in reader:
        if not row or 'buffer_size' not in row or row['buffer_size'] is None:
            continue
        sizes.append(int(row['buffer_size']))
        hit_rates.append(float(row['hit_rate']))
        times.append(float(row['time_ms']))

def make_svg(x, y, filename, y_label):
    w, h = 800, 400
    margin = 60
    minx, maxx = min(x), max(x)
    miny, maxy = min(y), max(y)
    if miny == maxy:
        miny -= 0.1
        maxy += 0.1
    def sx(v):
        return margin + (v - minx) / (maxx - minx) * (w - 2*margin)
    def sy(v):
        return h - margin - (v - miny) / (maxy - miny) * (h - 2*margin)

    pts = " ".join([f"{sx(xi)},{sy(yi)}" for xi, yi in zip(x, y)])
    svg = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}">')
    svg.append(f'<rect width="100%" height="100%" fill="white"/>')
    # axes
    svg.append(f'<line x1="{margin}" y1="{h-margin}" x2="{w-margin}" y2="{h-margin}" stroke="black"/>')
    svg.append(f'<line x1="{margin}" y1="{margin}" x2="{margin}" y2="{h-margin}" stroke="black"/>')
    # labels
    svg.append(f'<text x="{w/2}" y="20" font-size="16" text-anchor="middle">{y_label} vs Buffer Size</text>')
    # ticks and x labels
    for xi in x:
        svg.append(f'<line x1="{sx(xi)}" y1="{h-margin}" x2="{sx(xi)}" y2="{h-margin+6}" stroke="black"/>')
        svg.append(f'<text x="{sx(xi)}" y="{h-margin+20}" font-size="12" text-anchor="middle">{xi}</text>')
    # y labels (3 ticks)
    for t in range(4):
        vy = miny + (maxy-miny)*t/3
        svg.append(f'<text x="{margin-10}" y="{sy(vy)+4}" font-size="12" text-anchor="end">{round(vy,3)}</text>')
        svg.append(f'<line x1="{margin-4}" y1="{sy(vy)}" x2="{margin}" y2="{sy(vy)}" stroke="black"/>')
    # polyline
    svg.append(f'<polyline fill="none" stroke="#1f77b4" stroke-width="2" points="{pts}" />')
    # points
    for xi, yi in zip(x, y):
        svg.append(f'<circle cx="{sx(xi)}" cy="{sy(yi)}" r="4" fill="#1f77b4"/>')
    svg.append('</svg>')
    with open(filename, 'w') as f:
        f.write('\n'.join(svg))

make_svg(sizes, hit_rates, os.path.join(out_dir, 'buffer_hitrate.svg'), 'Hit Rate')
print('Wrote', os.path.join(out_dir, 'buffer_hitrate.svg'))
make_svg(sizes, times, os.path.join(out_dir, 'buffer_time.svg'), 'Time (ms)')
print('Wrote', os.path.join(out_dir, 'buffer_time.svg'))
