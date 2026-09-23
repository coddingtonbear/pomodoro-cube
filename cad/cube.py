"""A 55 mm timer cube enclosure, as printable parts.

A parametric riff on the original "Timer cube" by Robin
(https://www.printables.com/model/1774785-timer-cube), which is 55 x 55 x 62 mm
and carries its numerals, battery bay, beeper hole and lid fixings in the model.
This one is a true cube and deliberately carries none of those: it is the shell
plus board retention, meant to be opened in Fusion 360 and fitted out there.

Dimensions inherited from the original, measured off its meshes:

    outer size          55.00 mm        wall             3.00 mm
    corner radius        6.00 mm        inner radius     3.00 mm
    bottom chamfer       0.50 mm        clamp height     7.00 mm

The display seat is a cone, not a bore: it narrows from 38.97 mm at the outer
face to 35.70 mm at the inner one, so the module drops in from behind and wedges
rather than passing through. See :class:`CubeSpec` for why that matters.

Run this module to write STEP and STL for every part into `build/`.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from pathlib import Path

from build123d import (
    Align,
    Axis,
    BuildPart,
    BuildSketch,
    Circle,
    Compound,
    Cone,
    Edge,
    Location,
    Locations,
    Mode,
    Part,
    Plane,
    RectangleRounded,
    ShapeList,
    chamfer,
    export_step,
    export_stl,
    extrude,
)

# The Waveshare ESP32-S3-Touch-LCD-1.28, measured off the STEP model Waveshare
# publish under Resources at
# https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28
DISPLAY_MODULE_DIAMETER = 38.51
"""Outside diameter of the round display module. It must not fall through."""

DISPLAY_BEZEL_DIAMETER = 35.67
"""The module's black border. The seat may cover this but no more."""

DISPLAY_ACTIVE_DIAMETER = 33.40
"""Visible picture. Nothing may mask any of it."""

PCB_DIAMETER = 39.53
"""Widest extent of the board itself, which the clamp bars bear on.

The board has no mounting holes — only 0.50 and 0.76 mm vias — so clamping is
the only way to hold it. This is why the original uses bars and bosses rather
than screwing through the board, and why this model does the same.
"""

USB_C_CONNECTOR_WIDTH = 9.92
"""Width of the board's USB-C socket, from Waveshare's dimension drawing.

It sits on a 25.28 mm flat at the bottom of the otherwise round PCB and points
radially outward, so it has to leave through a side face rather than the back.
"""

BOARD_STACK_DEPTH = 8.40
"""Front of the glass to the back of the rearmost component.

Recorded for reference. The clamp bars bear on the board's bare rim, not on its
rear components, so this is not the height they sit at — see
:attr:`CubeSpec.board_clamp_height`.
"""


@dataclass(frozen=True)
class CubeSpec:
    """Every dimension the enclosure is built from."""

    #: Outer edge length of the finished cube.
    size: float = 55.0
    #: Side wall thickness, and the thickness of both plates.
    wall: float = 3.0
    #: Radius of the four vertical corners, on the outer surface.
    corner_radius: float = 6.0
    #: Chamfer on the cube's outermost top and bottom edges. Keeps the first
    #: layer off a knife edge and takes the sharpness off the finished cube.
    edge_chamfer: float = 0.5

    #: Narrow end of the display seat, at the plate's inner face. Sized just
    #: over the module's 35.67 mm bezel, so the opening shows the whole bezel
    #: and none of the board behind it.
    display_seat_diameter: float = 35.70
    #: Half-angle of the conical seat, from the plate's axis. The original's
    #: taper measures 28.4 degrees, which over a 3 mm wall opens the outer face
    #: to 38.94 mm — wider than the module's 38.51 mm face, so the module
    #: settles into the cone and comes to rest a little under the outer
    #: surface rather than sitting proud or dropping through.
    display_seat_angle: float = 28.4

    #: Distance from the top plate's inner face to the face the clamp bars bear
    #: on, which sets the boss height.
    #:
    #: 7.00 mm is the original's figure, and it is the one dimension here that
    #: was verified against a physical build of this exact board rather than
    #: derived. Check it with a board in hand before printing: the bars should
    #: meet the board's rim, not stand off it or crush it.
    board_clamp_height: float = 7.00
    #: Distance from the cube's axis to each boss centre, along the diagonals.
    #: Far enough out to clear the 39.53 mm board, near enough in to stay
    #: inside the cavity.
    boss_offset: float = 20.0
    #: Outside diameter of the four bosses.
    boss_diameter: float = 6.0
    #: Bore for an M2 heat-set insert.
    insert_bore_diameter: float = 3.20
    #: Clearance hole for the M2 screw through a clamp bar.
    screw_clearance_diameter: float = 2.40
    #: Thickness of a clamp bar.
    clamp_bar_thickness: float = 2.0
    #: Width of a clamp bar, wide enough to seat an M2 screw head.
    clamp_bar_width: float = 7.0
    #: Corner radius of a clamp bar. Short of half its width, which would
    #: make the ends semicircular and leave no straight flank.
    clamp_bar_corner_radius: float = 3.0

    #: Opening in the side wall for the board's USB-C socket, which points
    #: radially out of the board's edge and so has to leave through a side
    #: face rather than the back. Sized from the original's 12.00 x 6.25 mm
    #: cutout, which clears the 9.92 mm connector generously enough for a
    #: cable's overmoulding.
    usb_cutout_width: float = 12.00
    usb_cutout_height: float = 6.25
    #: How far in from the display's outer face the opening starts. The board
    #: hangs off that face, so this is what keeps the cutout lined up with the
    #: socket when other dimensions move.
    usb_cutout_depth: float = 6.00
    #: Eases the opening's corners. The original leaves them square; a radius
    #: prints better and takes the stress riser out of the wall.
    usb_cutout_corner_radius: float = 1.0
    #: Which way the socket faces, in degrees about the cube's axis. Zero puts
    #: it in the +X wall; the board turns with it.
    usb_cutout_bearing: float = 0.0

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
        if self.display_seat_diameter <= DISPLAY_ACTIVE_DIAMETER:
            raise ValueError(
                f"seat {self.display_seat_diameter} would mask the "
                f"{DISPLAY_ACTIVE_DIAMETER} mm active area"
            )
        if self.display_seat_diameter >= DISPLAY_MODULE_DIAMETER:
            raise ValueError(
                f"seat {self.display_seat_diameter} is wider than the "
                f"{DISPLAY_MODULE_DIAMETER} mm module, which would fall through"
            )
        if self.seat_mouth_diameter >= self.cavity_size:
            raise ValueError(
                f"seat mouth {self.seat_mouth_diameter:.2f} does not fit the "
                f"{self.cavity_size} mm cavity"
            )
        if self.clamp_bar_corner_radius * 2 >= self.clamp_bar_width:
            raise ValueError(
                f"clamp bar corner radius {self.clamp_bar_corner_radius} "
                f"leaves no flank on a {self.clamp_bar_width} mm bar"
            )
        if self.usb_cutout_corner_radius * 2 >= min(
            self.usb_cutout_width, self.usb_cutout_height
        ):
            raise ValueError(
                f"USB cutout corner radius {self.usb_cutout_corner_radius} "
                f"is too large for a {self.usb_cutout_width} x "
                f"{self.usb_cutout_height} mm opening"
            )
        if self.usb_cutout_depth < self.wall:
            raise ValueError(
                f"USB cutout starts {self.usb_cutout_depth} mm in, inside the "
                f"{self.wall} mm top plate rather than the band"
            )
        if self.usb_cutout_top_in_band + self.usb_cutout_height > self.band_height:
            raise ValueError("USB cutout runs off the bottom of the band")
        if self.boss_clearance_to_board < 0:
            raise ValueError(
                f"bosses at {self.boss_offset} mm foul the "
                f"{PCB_DIAMETER} mm board"
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
        """Height of the four-sided band, the cube less its two plates."""
        return self.size - 2 * self.wall

    @property
    def seat_mouth_diameter(self) -> float:
        """Wide end of the display seat, at the plate's outer face."""
        return self.display_seat_diameter + 2 * self.wall * math.tan(
            math.radians(self.display_seat_angle)
        )

    @property
    def module_seat_depth(self) -> float:
        """How far below the outer surface the module's face comes to rest.

        Where the cone has narrowed to the module's own diameter.
        """
        rise = (self.seat_mouth_diameter - DISPLAY_MODULE_DIAMETER) / 2
        return rise / math.tan(math.radians(self.display_seat_angle))

    @property
    def boss_height(self) -> float:
        """How far each boss stands proud of the plate's inner face."""
        return self.board_clamp_height

    @property
    def boss_clearance_to_board(self) -> float:
        """Gap between a boss's nearest edge and the board's rim."""
        radial = self.boss_offset * math.sqrt(2)
        return radial - self.boss_diameter / 2 - PCB_DIAMETER / 2

    @property
    def usb_cutout_top_in_band(self) -> float:
        """Distance from the band's top edge down to the opening's top.

        The band's top edge is the top plate's inner face, so this is the
        cutout's depth less the plate it sits behind.
        """
        return self.usb_cutout_depth - self.wall

    @property
    def usb_cutout_centre_height(self) -> float:
        """Height of the opening's middle above the band's own base."""
        return (
            self.band_height
            - self.usb_cutout_top_in_band
            - self.usb_cutout_height / 2
        )

    @property
    def clamp_bar_length(self) -> float:
        """End to end, with a half-width of material beyond each hole."""
        return 2 * self.boss_offset + self.clamp_bar_width


def _perimeter_edges(part: Part, axis_end: int) -> ShapeList[Edge]:
    """The outer boundary edges of the part's top (-1) or bottom (0) face.

    Taken from the face's outer wire, so holes in the face are left alone. A
    distance test would not do: `center()` on a full circle returns a point on
    the curve rather than the axis, which puts the display seat's mouth as far
    from the middle as the perimeter is and gets it chamfered too.
    """
    face = part.faces().sort_by(Axis.Z)[axis_end]
    return ShapeList(face.outer_wire().edges())


def build_band(spec: CubeSpec) -> Part:
    """The four side faces, as a single square tube open top and bottom.

    Both of its ends are seams against a plate, so neither is chamfered.
    """
    with BuildPart() as band:
        with BuildSketch():
            RectangleRounded(spec.size, spec.size, spec.corner_radius)
            RectangleRounded(
                spec.cavity_size,
                spec.cavity_size,
                spec.cavity_radius,
                mode=Mode.SUBTRACT,
            )
        extrude(amount=spec.band_height)

        # The USB-C opening, cut clean through one wall. Sketched on the
        # cutting plane and extruded both ways so the wall's thickness and
        # corner rounding never have to be reasoned about.
        plane = Plane.YZ.rotated((0, 0, spec.usb_cutout_bearing))
        # On Plane.YZ the sketch's own x runs along global Y and its y along
        # global Z, so the opening's width comes first and its height second.
        with BuildSketch(plane.offset(spec.size / 2)) as opening:
            with Locations((0, spec.usb_cutout_centre_height)):
                RectangleRounded(
                    spec.usb_cutout_width,
                    spec.usb_cutout_height,
                    spec.usb_cutout_corner_radius,
                )
        extrude(to_extrude=opening.sketch, amount=spec.wall * 2,
                both=True, mode=Mode.SUBTRACT)
    return band.part


def build_top_plate(spec: CubeSpec) -> Part:
    """The face the display looks out of, and what holds the board to it.

    Modelled in its print orientation: outer face on the bed at z=0, bosses
    rising from the inner face, so nothing needs support and the visible face
    gets the smooth side.
    """
    with BuildPart() as plate:
        with BuildSketch():
            RectangleRounded(spec.size, spec.size, spec.corner_radius)
        extrude(amount=spec.wall)

        # The seat is cut as a cone, widest at the outer face. Built upside
        # down and flipped, because Cone's bottom radius is its larger one.
        Cone(
            bottom_radius=spec.seat_mouth_diameter / 2,
            top_radius=spec.display_seat_diameter / 2,
            height=spec.wall,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
            mode=Mode.SUBTRACT,
        )

        # Four bosses on the diagonals, clear of the board, each bored for an
        # M2 heat-set insert. The bore stops at the inner face so it never
        # breaks through to the outside.
        with BuildSketch(Plane.XY.offset(spec.wall)):
            with Locations(*_boss_positions(spec)):
                Circle(spec.boss_diameter / 2)
        extrude(amount=spec.boss_height)
        with BuildSketch(Plane.XY.offset(spec.wall + spec.boss_height)):
            with Locations(*_boss_positions(spec)):
                Circle(spec.insert_bore_diameter / 2)
        extrude(amount=-spec.boss_height, mode=Mode.SUBTRACT)

        chamfer(_perimeter_edges(plate.part, 0), length=spec.edge_chamfer)
    return plate.part


def build_back_plate(spec: CubeSpec) -> Part:
    """The opposite face: a plain plate, with nothing in it yet."""
    with BuildPart() as plate:
        with BuildSketch():
            RectangleRounded(spec.size, spec.size, spec.corner_radius)
        extrude(amount=spec.wall)
        chamfer(_perimeter_edges(plate.part, 0), length=spec.edge_chamfer)
    return plate.part


def build_clamp_bar(spec: CubeSpec) -> Part:
    """A bar that screws down over two bosses and traps the board's rim.

    Two of these are needed, and they are identical. The board has no mounting
    holes, so this is what positively attaches it: the module wedges into the
    conical seat from behind, and the bars stop it coming back out.
    """
    with BuildPart() as bar:
        with BuildSketch():
            RectangleRounded(
                spec.clamp_bar_width,
                spec.clamp_bar_length,
                spec.clamp_bar_corner_radius,
            )
            with Locations((0, spec.boss_offset), (0, -spec.boss_offset)):
                Circle(spec.screw_clearance_diameter / 2, mode=Mode.SUBTRACT)
        extrude(amount=spec.clamp_bar_thickness)
    return bar.part


def _boss_positions(spec: CubeSpec) -> list[tuple[float, float]]:
    d = spec.boss_offset
    return [(d, d), (d, -d), (-d, d), (-d, -d)]


@dataclass
class Enclosure:
    """The parts, each in its print orientation, plus the assembled cube."""

    spec: CubeSpec
    band: Part = field(init=False)
    top_plate: Part = field(init=False)
    back_plate: Part = field(init=False)
    clamp_bar: Part = field(init=False)

    def __post_init__(self) -> None:
        self.band = build_band(self.spec)
        self.top_plate = build_top_plate(self.spec)
        self.back_plate = build_back_plate(self.spec)
        self.clamp_bar = build_clamp_bar(self.spec)

    @property
    def parts(self) -> dict[str, Part]:
        return {
            "band": self.band,
            "top-plate": self.top_plate,
            "back-plate": self.back_plate,
            "clamp-bar": self.clamp_bar,
        }

    @property
    def print_quantities(self) -> dict[str, int]:
        return {"band": 1, "top-plate": 1, "back-plate": 1, "clamp-bar": 2}

    def assembled(self) -> Compound:
        """Every part moved into its place in the finished cube.

        The back plate lies on the bed and the top plate is turned over, so the
        display looks up out of the top of the assembly.
        """
        spec = self.spec
        clamp_z = spec.size - spec.wall - spec.boss_height - spec.clamp_bar_thickness
        children = [
            self.back_plate.moved(Location((0, 0, 0))),
            self.band.moved(Location((0, 0, spec.wall))),
            self.top_plate.rotate(Axis.X, 180).moved(Location((0, 0, spec.size))),
        ]
        for x in (spec.boss_offset, -spec.boss_offset):
            children.append(self.clamp_bar.moved(Location((x, 0, clamp_z))))
        return Compound(children=children)

    def export(self, directory: Path) -> list[Path]:
        """Write STEP and STL for each part; returns what was written."""
        directory.mkdir(parents=True, exist_ok=True)
        written: list[Path] = []
        for name, part in self.parts.items():
            step = directory / f"{name}.step"
            stl = directory / f"{name}.stl"
            export_step(part, str(step))
            export_stl(part, str(stl))
            written += [step, stl]
        return written


def main() -> None:
    spec = CubeSpec()
    enclosure = Enclosure(spec)
    written = enclosure.export(Path(__file__).parent / "build")
    box = enclosure.assembled().bounding_box().size
    print(f"assembled {box.X:.2f} x {box.Y:.2f} x {box.Z:.2f} mm")
    print(f"display seat {spec.seat_mouth_diameter:.2f} -> {spec.display_seat_diameter:.2f} mm")
    print(f"module rests {spec.module_seat_depth:.2f} mm below the outer face")
    print(f"boss clearance to board {spec.boss_clearance_to_board:.2f} mm")
    for name, count in enclosure.print_quantities.items():
        print(f"  print {count} x {name}")


if __name__ == "__main__":
    main()
