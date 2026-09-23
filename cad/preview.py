"""Render both split styles, assembled and exploded, into preview.png.

Run from this directory after cube.py has written build/.
"""

import trimesh, numpy as np, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

def draw(ax, meshes, colors, title):
    for m, c in zip(meshes, colors):
        tris = m.vertices[m.faces]
        pc = Poly3DCollection(tris, facecolor=c, edgecolor='none', alpha=1.0)
        pc.set_sort_zpos(0)
        ax.add_collection3d(pc)
    ax.set_xlim(-30,30); ax.set_ylim(-30,30); ax.set_zlim(0,60)
    ax.set_box_aspect((1,1,1)); ax.view_init(elev=22, azim=-52)
    ax.set_title(title, fontsize=10); ax.set_axis_off()

fig = plt.figure(figsize=(13,7))
for col, style in enumerate(("plates-as-faces","plates-inset")):
    parts = {n: trimesh.load(f"build/{n}-{style}.stl") for n in ("band","top-plate","back-plate")}
    z = {"back-plate":0.0, "band":3.0 if style=="plates-as-faces" else 0.0, "top-plate":52.0}
    # assembled
    asm = []
    for n,m in parts.items():
        c=m.copy(); c.apply_translation([0,0,z[n]]); asm.append(c)
    ax = fig.add_subplot(2,2,col+1, projection='3d')
    draw(ax, asm, ['#9fb4c7','#d9a441','#b5c9a7'], f"{style} — assembled")
    # exploded
    exp = []
    gap = {"back-plate":-14.0, "band":0.0, "top-plate":14.0}
    for n,m in parts.items():
        c=m.copy(); c.apply_translation([0,0,z[n]+gap[n]]); exp.append(c)
    ax = fig.add_subplot(2,2,col+3, projection='3d')
    draw(ax, exp, ['#9fb4c7','#d9a441','#b5c9a7'], f"{style} — exploded")
plt.tight_layout(); plt.savefig('preview.png', dpi=115)
print("ok")
