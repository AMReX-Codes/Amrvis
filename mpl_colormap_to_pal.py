#!/usr/bin/env python3

import argparse
import importlib

import matplotlib as mpl
import numpy as np

parser = argparse.ArgumentParser(
    description="Generate Amrvis palette files from matplotlib colormaps."
)
parser.add_argument("cmap", nargs="+", help="the name of a matplotlib colormap")
parser.add_argument(
    "--import",
    action="append",
    dest="extra_imports",
    metavar="MODULE",
    default=[],
    help="module to import for additional colormaps",
)
args = parser.parse_args()
print(args)

for name in args.extra_imports:
    importlib.import_module(name)

for cm_name in args.cmap:
    cmap = mpl.colormaps.get_cmap(cm_name)
    colors = cmap.resampled(256)(np.linspace(0, 1, 256))
    data = np.round(colors * (255, 255, 255, 100)).astype(np.uint8).T.reshape(-1)
    filename = f"{cmap.name}.pal"
    data.tofile(filename)
    print(f"wrote palette to {filename}")
