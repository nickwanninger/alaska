#!/usr/bin/env python3

import os
import csv
import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns
import sys

if len(sys.argv) != 3:
    print('usage: bench.py <speedup.csv> <output.pdf>')
    exit()

csv = sys.argv[1]
output = sys.argv[2]

df = pd.read_csv(csv)
# df = df.set_index('benchmark')
# del df['speedup']

fig, ax = plt.subplots(figsize=(10,4))

print(df)
plt.grid('y')
g = sns.barplot(df, x='benchmark', y='speedup', ax=ax, palette=("Blues_d"))
g.set_xticklabels(g.get_xticklabels(), rotation=45, horizontalalignment='right')

plt.tight_layout()

plt.savefig(output)
