#!/usr/bin/env python3
import csv
import sys
import os

try:
    import matplotlib.pyplot as plt
except Exception as e:
    print("matplotlib not available:", e, file=sys.stderr)
    sys.exit(2)

csv_file = sys.argv[1] if len(sys.argv) > 1 else 'benchmark.csv'
out_dir = 'docs/figures'
os.makedirs(out_dir, exist_ok=True)
out_png = os.path.join(out_dir, 'buffer_hitrate.png')

sizes = []
hit_rates = []
times = []

with open(csv_file, newline='') as f:
    reader = csv.DictReader(f)
    for row in reader:
        sizes.append(int(row['buffer_size']))
        hit_rates.append(float(row['hit_rate']))
        times.append(float(row['time_ms']))

plt.figure(figsize=(6,4))
plt.plot(sizes, hit_rates, marker='o')
plt.title('Buffer Manager Hit Rate vs Buffer Size')
plt.xlabel('Buffer Size')
plt.ylabel('Hit Rate')
plt.grid(True)
plt.savefig(out_png)
print('Wrote', out_png)

out_png2 = os.path.join(out_dir, 'buffer_time.png')
plt.figure(figsize=(6,4))
plt.plot(sizes, times, marker='o', color='orange')
plt.title('Benchmark Time vs Buffer Size')
plt.xlabel('Buffer Size')
plt.ylabel('Time (ms)')
plt.grid(True)
plt.savefig(out_png2)
print('Wrote', out_png2)
