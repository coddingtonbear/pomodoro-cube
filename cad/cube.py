"""A 55 mm timer cube enclosure, as three printable parts.

A parametric riff on the original "Timer cube" by Robin
(https://www.printables.com/model/1774785-timer-cube), which is 55 x 55 x 62 mm
and carries its numerals, battery bay, beeper hole and M2 fixings in the model.
This one is a true cube and deliberately carries none of those: it is the shell
only, meant to be opened in Fusion 360 and have the internals added there.

Dimensions inherited from the original, measured off its meshes:

    outer size          55.00 mm        wall             3.00 mm
    corner radius        6.00 mm        inner radius     3.00 mm
    LCD aperture        37.81 mm        bottom chamfer   0.50 mm

The three parts are the four side faces as one tube, a top plate carrying the
LCD aperture, and a plain back plate. How the top plate meets the band is the
one thing the original does not settle for us, so it is a parameter: see
`SplitStyle`.

Run this module to write STEP and STL for every part into `build/`.
"""

from __future__ import annotations

import enum
from dataclasses import dataclass, field
from pathlib import Path

from build123d import (
    Axis,
    BuildPart,
    BuildSketch,
    Circle,
    Compound,
    Edge,
    Location,
    Mode,
    Part,
    RectangleRounded,
    ShapeList,
    chamfer,
    export_step,
    export_stl,
    extrude,
)


class SplitStyle(enum.Enum):
    """Where the seam between the band and the plates falls.

    The cube has six faces and the parts number three, so two faces have to
    come from somewhere. Either reading is defensible and they look different
    in the hand, so both are built.
    """

    #: The plates *are* the top and back faces, full 55 mm squares. The band is
    #: shorter by two plate thicknesses, and each side face shows a 3 mm seam
    #: at top and bottom.
    PLATES_AS_FACES = "plates-as-faces"

    #: The band is the full height of the cube with uninterrupted side faces,
    #: and the plates drop into its opening, flush with the top and back.
    #: The seam reads as an inset square on those two faces instead.
    PLATES_INSET = "plates-inset"


@dataclass(frozen=True)
class CubeSpec:
    """Every dimension the enclosure is built from."""

    #: Outer edge length of the finished cube.
    size: float = 55.0
    #: Side wall thickness, and the thickness of both plates.
    wall: float = 3.0
    #: Radius of the four vertical corners, on the outer surface.
    corner_radius: float = 6.0
    #: Aperture for the Waveshare ESP32-S3-Touch-LCD-1.28 display module.
    #: The module itself is 38.51 mm across, so it sits behind a 0.35 mm lip,
    #: exactly as the original does it.
    lcd_aperture_diameter: float = 37.81
    #: Chamfer on the cube's outermost top and bottom edges. Keeps the first
    #: layer off a knife edge and takes the sharpness off the finished cube.
    edge_chamfer: float = 0.5
    #: Total diametral slack where a plate sits inside the band. Only used by
    #: :attr:`SplitStyle.PLATES_INSET`.
    fit_clearance: float = 0.2
    #: Which of the two seam arrangements to build.
    split_style: SplitStyle = SplitStyle.PLATES_AS_FACES

    def __post_init__(self) -> None:
        if self.wall * 2 >= self.size:
            raise ValueError(f"wall {self.wall} too thick for a {self.size} mm cube")
        if self.corner_radius > self.size / 2:
            raise ValueError(
                f"corner radius {self.corner_radius} exceeds half of {self.size}"
            )
        if self.corner_radius < self.wall:
            raise ValueError(
                f"corner radius {self.corner_radius} is thinner than the "
                f"{self.wall} mm wall, which would leave a knife edge inside"
            )
        if self.lcd_aperture_diameter >= self.cavity_size:
            raise ValueError(
                f"LCD aperture {self.lcd_aperture_diameter} does not fit the "
                f"{self.cavity_size} mm cavity"
            )

    @property
    def cavity_size(self) -> float:
        """Edge length of the square opening through the band."""
        return self.size - 2 * self.wall

    @property
    def cavity_radius(self) -> float:
        """Corner radius of that opening, offset inwards from the outer one."""
        return self.corner_radius - self.wall

    @property
    def band_height(self) -> float:
        """Height of the four-sided band."""
        if self.split_style is SplitStyle.PLATES_AS_FACES:
            return self.size - 2 * self.wall
        return self.size

    @property
    def plate_size(self) -> float:
        """Edge length of the top and back plates."""
        if self.split_style is SplitStyle.PLATES_AS_FACES:
            return self.size
        return self.cavity_size - self.fit_clearance

    @property
    def plate_radius(self) -> float:
        """Corner radius of those plates."""
        if self.split_style is SplitStyle.PLATES_AS_FACES:
            return self.corner_radius
        return self.cavity_radius - self.fit_clearance / 2


def _perimeter_edges(part: Part, axis_end: int) -> ShapeList[Edge]:
    """The outer boundary edges of the part's top (1) or bottom (-1) face.

    Picked out by distance from the vertical axis rather than by geometry type,
    because the rounded corners are arcs and so is the LCD aperture; only their
    distance from the middle tells them apart.
    """
    face = part.faces().sort_by(Axis.Z)[axis_end]
    threshold = max(e.center().length for e in face.edges()) * 0.5
    return ShapeList(e for e in face.edges() if e.center().length > threshold)


def build_band(spec: CubeSpec) -> Part:
    """The four side faces, as a single square tube open top and bottom."""
    with BuildPart() as band:
        with BuildSketch():
            RectangleRounded(spec.size, spec.size, spec.corner_radius)
            RectangleRounded(
                spec.cavity_size, spec.cavity_size, spec.cavity_radius,
                mode=Mode.SUBTRACT,
            )
        extrude(amount=spec.band_height)
        if spec.split_style is SplitStyle.PLATES_INSET:
            # Here the band owns the cube's top and bottom edges, so it is the
            # part that gets chamfered.
            chamfer(_perimeter_edges(band.part, -1), length=spec.edge_chamfer)
            chamfer(_perimeter_edges(band.part, 0), length=spec.edge_chamfer)
    return band.part


def build_top_plate(spec: CubeSpec) -> Part:
    """The face the display looks out of."""
    with BuildPart() as plate:
        with BuildSketch():
            RectangleRounded(spec.plate_size, spec.plate_size, spec.plate_radius)
            Circle(spec.lcd_aperture_diameter / 2, mode=Mode.SUBTRACT)
        extrude(amount=spec.wall)
        if spec.split_style is SplitStyle.PLATES_AS_FACES:
            chamfer(_perimeter_edges(plate.part, -1), length=spec.edge_chamfer)
    return plate.part


def build_back_plate(spec: CubeSpec) -> Part:
    """The opposite face: a plain plate, with nothing in it yet."""
    with BuildPart() as plate:
        with BuildSketch():
            RectangleRounded(spec.plate_size, spec.plate_size, spec.plate_radius)
        extrude(amount=spec.wall)
        if spec.split_style is SplitStyle.PLATES_AS_FACES:
            chamfer(_perimeter_edges(plate.part, 0), length=spec.edge_chamfer)
    return plate.part


@dataclass
class Enclosure:
    """The three parts, each at its own origin, plus the assembled cube."""

    spec: CubeSpec
    band: Part = field(init=False)
    top_plate: Part = field(init=False)
    back_plate: Part = field(init=False)

    def __post_init__(self) -> None:
        self.band = build_band(self.spec)
        self.top_plate = build_top_plate(self.spec)
        self.back_plate = build_back_plate(self.spec)

    @property
    def parts(self) -> dict[str, Part]:
        return {
            "band": self.band,
            "top-plate": self.top_plate,
            "back-plate": self.back_plate,
        }

    def assembled(self) -> Compound:
        """The three parts moved into their places in the finished cube."""
        spec = self.spec
        if spec.split_style is SplitStyle.PLATES_AS_FACES:
            back_z, band_z, top_z = 0.0, spec.wall, spec.size - spec.wall
        else:
            back_z, band_z, top_z = 0.0, 0.0, spec.size - spec.wall
        return Compound(
            children=[
                self.back_plate.moved(Location((0, 0, back_z))),
                self.band.moved(Location((0, 0, band_z))),
                self.top_plate.moved(Location((0, 0, top_z))),
            ]
        )

    def export(self, directory: Path) -> list[Path]:
        """Write STEP and STL for each part; returns what was written."""
        directory.mkdir(parents=True, exist_ok=True)
        written: list[Path] = []
        for name, part in self.parts.items():
            stem = f"{name}-{self.spec.split_style.value}"
            step = directory / f"{stem}.step"
            stl = directory / f"{stem}.stl"
            export_step(part, str(step))
            export_stl(part, str(stl))
            written += [step, stl]
        return written


def main() -> None:
    out = Path(__file__).parent / "build"
    for style in SplitStyle:
        spec = CubeSpec(split_style=style)
        enclosure = Enclosure(spec)
        written = enclosure.export(out)
        box = enclosure.assembled().bounding_box()
        print(f"{style.value}: assembled {box.size.X:.2f} x {box.size.Y:.2f} x {box.size.Z:.2f} mm")
        for path in written:
            print(f"  {path.relative_to(out.parent)}")


if __name__ == "__main__":
    main()
