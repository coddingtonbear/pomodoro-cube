"""Render the enclosure into preview.png.

Run from this directory after cube.py has written build/.
"""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import trimesh  # noqa: E402
from mpl_toolkits.mplot3d.art3d import Poly3DCollection  # noqa: E402

COLOURS = {
    "band": "#9fb4c7",
    "top-plate": "#d9a441",
    "back-plate": "#b5c9a7",
    "clamp-bar": "#c78f8f",
}

# Where each part sits in the finished cube, mirroring Enclosure.assembled().
SIZE, WALL, CLAMP, BAR_T, OFFSET = 55.0, 3.0, 7.0, 2.0, 20.0
PLACEMENT = [
    ("back-plate", (0, 0, 0), False),
    ("band", (0, 0, WALL), False),
    ("top-plate", (0, 0, SIZE), True),
    ("clamp-bar", (OFFSET, 0, SIZE - WALL - CLAMP - BAR_T), False),
    ("clamp-bar", (-OFFSET, 0, SIZE - WALL - CLAMP - BAR_T), False),
]


def load(name, translate, flip):
    mesh = trimesh.load(f"build/{name}.stl")
    if flip:
        mesh.apply_transform(trimesh.transformations.rotation_matrix(3.14159265, [1, 0, 0]))
    mesh.apply_translation(translate)
    return mesh


def draw(ax, meshes, colours, title):
    for mesh, colour in zip(meshes, colours):
        ax.add_collection3d(
            Poly3DCollection(mesh.vertices[mesh.faces], facecolor=colour, edgecolor="none")
        )
    ax.set_xlim(-32, 32)
    ax.set_ylim(-32, 32)
    ax.set_zlim(-4, 62)
    ax.set_box_aspect((1, 1, 1))
    ax.view_init(elev=20, azim=-54)
    ax.set_title(title, fontsize=10)
    ax.set_axis_off()


def retention_view(ax):
    """The top plate seen from inside, which is where the board is held."""
    plate = trimesh.load("build/top-plate.stl")
    bars = [
        trimesh.load("build/clamp-bar.stl").apply_translation(
            (x, 0, WALL + CLAMP)
        )
        for x in (OFFSET, -OFFSET)
    ]
    meshes = [plate] + bars
    colours = [COLOURS["top-plate"]] + [COLOURS["clamp-bar"]] * 2
    for mesh, colour in zip(meshes, colours):
        ax.add_collection3d(
            Poly3DCollection(mesh.vertices[mesh.faces], facecolor=colour, edgecolor="none")
        )
    ax.set_xlim(-32, 32)
    ax.set_ylim(-32, 32)
    ax.set_zlim(-26, 38)
    ax.set_box_aspect((1, 1, 1))
    ax.view_init(elev=32, azim=-54)
    ax.set_title("board retention: bosses and clamp bars", fontsize=10)
    ax.set_axis_off()


def main():
    fig = plt.figure(figsize=(16, 6))

    meshes = [load(n, t, f) for n, t, f in PLACEMENT]
    colours = [COLOURS[n] for n, _, _ in PLACEMENT]
    draw(fig.add_subplot(1, 3, 1, projection="3d"), meshes, colours, "assembled")

    # Pulled apart along z so the bosses and clamp bars are visible.
    gaps = {"back-plate": -16.0, "band": 0.0, "top-plate": 18.0, "clamp-bar": 9.0}
    exploded = [
        load(n, (t[0], t[1], t[2] + gaps[n]), f) for n, t, f in PLACEMENT
    ]
    draw(fig.add_subplot(1, 3, 2, projection="3d"), exploded, colours, "exploded")

    retention_view(fig.add_subplot(1, 3, 3, projection="3d"))

    plt.subplots_adjust(left=0.01, right=0.99, top=0.94, bottom=0.01, wspace=0.02)
    plt.savefig("preview.png", dpi=115)
    print("wrote preview.png")


if __name__ == "__main__":
    main()
