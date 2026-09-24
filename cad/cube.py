"""A 55 mm timer cube enclosure, as printable parts.

A parametric riff on the original "Timer cube" by Robin
(https://www.printables.com/model/1774785-timer-cube), which is 55 x 55 x 62 mm
and carries its numerals, battery bay, beeper hole and lid fixings in the model.
This one is a true cube and deliberately carries none of those: it is the shell
plus board retention, meant to be opened in Fusion 360 and fitted out there.

Dimensions inherited from the original, measured off its meshes:

    outer size          55.00 mm        wall             3.00 mm
    corner radius        6.00 mm        inner radius     3.00 mm
    bottom chamfer       0.50 mm        seat taper      28.4 degrees

Everything about the board itself — and so the seat depth, the clamp height and
the USB-C opening, which all derive from it — comes from :mod:`board`, measured
from the STEP Waveshare publish. See :class:`CubeSpec` for how the chain runs.

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

from board import WAVESHARE_ESP32_S3_TOUCH_LCD_1_28, BoardSpec


@dataclass(frozen=True)
class CubeSpec:
    """Every dimension the enclosure is built from.

    The chain runs from the board outwards. :attr:`display_recess` says how far
    the bezel should sit below the finished face; that fixes how deep the
    module's Ø38.51 shoulder has to seat, which fixes the seat's mouth and
    throat, where the PCB's back face lands, and where the USB-C socket lands
    in the side wall. Change the board and every one of those follows.
    """

    #: The board this is built around. Swap it for measurements off a real one.
    board: BoardSpec = WAVESHARE_ESP32_S3_TOUCH_LCD_1_28

    #: Outer edge length of the finished cube.
    size: float = 55.0
    #: Side wall thickness, and the thickness of both plates.
    wall: float = 3.0
    #: Radius of the four vertical corners, on the outer surface.
    corner_radius: float = 6.0
    #: Chamfer on the cube's outermost top and bottom edges. Keeps the first
    #: layer off a knife edge and takes the sharpness off the finished cube.
    edge_chamfer: float = 0.5

    #: How far the display bezel's face should sit below the cube's outer
    #: surface. This is the number that decides how the finished face reads,
    #: and everything about the board's depth follows from it.
    display_recess: float = 0.40
    #: Half-angle of the conical seat, from the plate's axis. Taken from the
    #: original, whose taper measures 28.4 degrees.
    display_seat_angle: float = 28.4

    #: Distance from the cube's axis to each boss centre, along the diagonals.
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

    #: Gap left around the USB-C socket's sides and underside.
    usb_cutout_clearance: float = 1.00
    #: Gap left above the socket. Kept smaller than the others because the
    #: socket sits close to the display and the opening's top edge runs out of
    #: band to sit in — see :attr:`usb_cutout_top_in_band`.
    usb_cutout_top_clearance: float = 0.50
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
        if self.module_seat_depth >= self.wall:
            raise ValueError(
                f"a {self.display_recess} mm recess seats the module "
                f"{self.module_seat_depth:.2f} mm down, through a "
                f"{self.wall} mm plate"
            )
        if self.seat_throat_diameter <= self.board.active_diameter:
            raise ValueError(
                f"throat {self.seat_throat_diameter:.2f} would mask the "
                f"{self.board.active_diameter} mm active area"
            )
        if self.seat_throat_diameter >= self.board.module_diameter:
            raise ValueError(
                f"throat {self.seat_throat_diameter:.2f} is wider than the "
                f"{self.board.module_diameter} mm module, which would fall through"
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
                f"is too large for a {self.usb_cutout_width:.2f} x "
                f"{self.usb_cutout_height:.2f} mm opening"
            )
        if self.usb_cutout_depth < self.wall:
            raise ValueError(
                f"USB cutout starts {self.usb_cutout_depth:.2f} mm in, inside "
                f"the {self.wall} mm top plate rather than the band"
            )
        if self.usb_cutout_top_in_band + self.usb_cutout_height > self.band_height:
            raise ValueError("USB cutout runs off the bottom of the band")
        if self.boss_clearance_to_board < 0:
            raise ValueError(
                f"bosses at {self.boss_offset} mm foul the "
                f"{self.board.pcb_diameter} mm board"
            )
        if self.clamp_bar_overlap <= 0:
            raise ValueError(
                f"clamp bars start {self.boss_offset - self.clamp_bar_width / 2} mm "
                f"out and never reach the {self.board.pcb_diameter} mm board"
            )
        if self.board.usb_socket_width / 2 >= self.clamp_bar_inner_edge:
            raise ValueError(
                f"clamp bars reach within {self.clamp_bar_inner_edge} mm of the "
                f"axis and would land on the USB-C socket"
            )

    # -- the shell ---------------------------------------------------------

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

    # -- the display seat --------------------------------------------------

    @property
    def module_seat_depth(self) -> float:
        """Depth of the module's Ø38.51 shoulder below the outer face.

        The bezel stands proud of that shoulder, so seating the shoulder flush
        would leave the bezel sticking out. Burying it by the bezel's own
        height plus the wanted recess is what puts the glass where it belongs.
        """
        return self.display_recess + self.board.bezel_proud_of_shoulder

    @property
    def seat_mouth_diameter(self) -> float:
        """Wide end of the display seat, at the plate's outer face.

        Sized so the cone has narrowed to the module's own diameter exactly at
        :attr:`module_seat_depth`, which is where the module comes to rest.
        """
        return self.board.module_diameter + 2 * self.module_seat_depth * math.tan(
            math.radians(self.display_seat_angle)
        )

    @property
    def seat_throat_diameter(self) -> float:
        """Narrow end of the display seat, at the plate's inner face."""
        return self.seat_mouth_diameter - 2 * self.wall * math.tan(
            math.radians(self.display_seat_angle)
        )

    # -- board retention ---------------------------------------------------

    @property
    def board_clamp_height(self) -> float:
        """Plate's inner face to the PCB's back face, which the bars bear on.

        Derived rather than given: once the seat depth is fixed, the board's
        own geometry says where its back face lands.
        """
        return self.module_seat_depth + self.board.pcb_back_behind_shoulder - self.wall

    @property
    def boss_height(self) -> float:
        """How far each boss stands proud of the plate's inner face."""
        return self.board_clamp_height

    @property
    def boss_clearance_to_board(self) -> float:
        """Gap between a boss's nearest edge and the board's rim."""
        radial = self.boss_offset * math.sqrt(2)
        return radial - self.boss_diameter / 2 - self.board.pcb_diameter / 2

    @property
    def clamp_bar_inner_edge(self) -> float:
        """Axis to a clamp bar's nearest edge."""
        return self.boss_offset - self.clamp_bar_width / 2

    @property
    def clamp_bar_overlap(self) -> float:
        """How far a bar reaches over the PCB's rim."""
        return self.board.pcb_diameter / 2 - self.clamp_bar_inner_edge

    @property
    def clamp_bar_bearing(self) -> float:
        """Which way the bars run, in degrees about the cube's axis.

        Always square to the socket. The USB-C socket hangs off the back of the
        PCB — the same face the bars bear on — so a bar crossing the board's tab
        would land on the socket rather than the board.
        """
        return self.usb_cutout_bearing + 90.0

    @property
    def clamp_bar_length(self) -> float:
        """End to end, with a half-width of material beyond each hole."""
        return 2 * self.boss_offset + self.clamp_bar_width

    # -- the USB-C opening -------------------------------------------------

    @property
    def usb_cutout_width(self) -> float:
        """Opening width, the socket plus clearance on both sides."""
        return self.board.usb_socket_width + 2 * self.usb_cutout_clearance

    @property
    def usb_cutout_height(self) -> float:
        """Opening height, the socket plus its two clearances."""
        return (
            self.board.usb_socket_height
            + self.usb_cutout_top_clearance
            + self.usb_cutout_clearance
        )

    @property
    def usb_cutout_depth(self) -> float:
        """How far in from the display's outer face the opening starts."""
        return (
            self.module_seat_depth
            + self.board.usb_socket_top_behind_shoulder
            - self.usb_cutout_top_clearance
        )

    @property
    def usb_cutout_top_in_band(self) -> float:
        """Distance from the band's top edge down to the opening's top.

        The band's top edge is the top plate's inner face, so this is the
        cutout's depth less the plate it sits behind. It is also the ligament
        of band left above the opening, which this board makes thin.
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
    def socket_to_wall_gap(self) -> float:
        """Gap between the socket's face and the wall's inner surface.

        A cable's plug has to cross this, plus the wall, before it engages.
        """
        return self.size / 2 - self.wall - self.board.usb_socket_reach


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
            top_radius=spec.seat_throat_diameter / 2,
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
        display looks up out of the top of the assembly. The bars are turned to
        :attr:`CubeSpec.clamp_bar_bearing`, square to the socket.
        """
        spec = self.spec
        clamp_z = spec.size - spec.wall - spec.boss_height - spec.clamp_bar_thickness
        children = [
            self.back_plate.moved(Location((0, 0, 0))),
            self.band.moved(Location((0, 0, spec.wall))),
            self.top_plate.rotate(Axis.X, 180).moved(Location((0, 0, spec.size))),
        ]
        for x in (spec.boss_offset, -spec.boss_offset):
            bar = self.clamp_bar.moved(Location((x, 0, clamp_z)))
            children.append(bar.rotate(Axis.Z, spec.clamp_bar_bearing))
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
    print(f"board {spec.board.name}")
    print(f"display seat {spec.seat_mouth_diameter:.2f} -> {spec.seat_throat_diameter:.2f} mm")
    print(f"module shoulder seats {spec.module_seat_depth:.2f} mm down, "
          f"bezel {spec.display_recess:.2f} mm below the face")
    print(f"clamp bears {spec.board_clamp_height:.2f} mm below the inner face, "
          f"overlapping the board by {spec.clamp_bar_overlap:.2f} mm")
    print(f"bars run at {spec.clamp_bar_bearing:.0f} deg, socket at "
          f"{spec.usb_cutout_bearing:.0f} deg")
    print(f"usb opening {spec.usb_cutout_width:.2f} x {spec.usb_cutout_height:.2f} mm, "
          f"{spec.usb_cutout_depth:.2f} mm in, leaving "
          f"{spec.usb_cutout_top_in_band:.2f} mm of band above it")
    print(f"socket face sits {spec.socket_to_wall_gap:.2f} mm inside the wall")
    print(f"boss clearance to board {spec.boss_clearance_to_board:.2f} mm")
    for name, count in enclosure.print_quantities.items():
        print(f"  print {count} x {name}")


if __name__ == "__main__":
    main()
