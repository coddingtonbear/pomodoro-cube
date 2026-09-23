"""Tests for the enclosure model.

The things worth pinning are the ones a careless edit to a parameter would
break silently: that the parts still add up to an exact cube, that each is a
single solid a slicer will accept, that nothing inside collides with anything
else, and that the display seat and board retention still suit the board they
were measured from.
"""

from __future__ import annotations

import math

import pytest
from build123d import Axis, GeomType, Part

from cube import (
    BOARD_STACK_DEPTH,
    USB_C_CONNECTOR_WIDTH,
    DISPLAY_ACTIVE_DIAMETER,
    DISPLAY_BEZEL_DIAMETER,
    DISPLAY_MODULE_DIAMETER,
    PCB_DIAMETER,
    CubeSpec,
    Enclosure,
)

TOLERANCE = 1e-6


@pytest.fixture
def spec() -> CubeSpec:
    return CubeSpec()


@pytest.fixture
def enclosure(spec: CubeSpec) -> Enclosure:
    return Enclosure(spec)


class TestAssembly:
    def test_assembles_to_an_exact_cube(self, enclosure: Enclosure) -> None:
        size = enclosure.assembled().bounding_box().size
        for measured in (size.X, size.Y, size.Z):
            assert measured == pytest.approx(enclosure.spec.size, abs=1e-3)

    def test_heights_sum_to_the_cube(self, spec: CubeSpec) -> None:
        assert spec.band_height + 2 * spec.wall == pytest.approx(spec.size)

    def test_the_parts_are_band_plates_and_a_clamp_bar(
        self, enclosure: Enclosure
    ) -> None:
        assert set(enclosure.parts) == {
            "band",
            "top-plate",
            "back-plate",
            "clamp-bar",
        }

    def test_the_clamp_bar_is_printed_twice(self, enclosure: Enclosure) -> None:
        assert enclosure.print_quantities["clamp-bar"] == 2

    def test_nothing_collides_with_anything(self, enclosure: Enclosure) -> None:
        """Bosses and bars have to live inside the band's cavity."""
        placed = list(enclosure.assembled().children)
        for i, first in enumerate(placed):
            for second in placed[i + 1 :]:
                assert (first & second).volume == pytest.approx(0.0, abs=1e-3)


class TestParts:
    def test_every_part_is_one_closed_solid(self, enclosure: Enclosure) -> None:
        for name, part in enclosure.parts.items():
            assert isinstance(part, Part), name
            assert len(part.solids()) == 1, f"{name} is not a single solid"
            assert part.is_valid, f"{name} failed OCCT validation"
            assert part.volume > 0, name

    def test_band_footprint_is_the_full_cube(self, enclosure: Enclosure) -> None:
        size = enclosure.band.bounding_box().size
        assert size.X == pytest.approx(enclosure.spec.size, abs=1e-3)
        assert size.Y == pytest.approx(enclosure.spec.size, abs=1e-3)

    def test_band_walls_are_the_specified_thickness(self, enclosure: Enclosure) -> None:
        spec = enclosure.spec
        upright = [
            f
            for f in enclosure.band.faces().filter_by(GeomType.PLANE)
            if abs(f.normal_at(f.center()).Z) < TOLERANCE
        ]
        # Horizontal distance only: the face centres sit halfway up the band.
        offsets = {round(math.hypot(f.center().X, f.center().Y), 3) for f in upright}
        assert max(offsets) == pytest.approx(spec.size / 2, abs=1e-3)
        assert min(offsets) == pytest.approx(spec.cavity_size / 2, abs=1e-3)
        assert max(offsets) - min(offsets) == pytest.approx(spec.wall, abs=1e-3)

    def test_back_plate_is_wall_thick_and_unbroken(self, enclosure: Enclosure) -> None:
        """Nothing is cut into it yet — the internals come later, in Fusion."""
        spec = enclosure.spec
        plate = enclosure.back_plate
        assert plate.bounding_box().size.Z == pytest.approx(spec.wall, abs=1e-3)
        expected = spec.size**2 - (4 - math.pi) * spec.corner_radius**2
        # The inner face, which carries no chamfer.
        assert plate.faces().sort_by(Axis.Z)[-1].area == pytest.approx(
            expected, rel=1e-3
        )

    def test_top_plate_stands_wall_plus_bosses_tall(self, enclosure: Enclosure) -> None:
        spec = enclosure.spec
        height = enclosure.top_plate.bounding_box().size.Z
        assert height == pytest.approx(spec.wall + spec.boss_height, abs=1e-3)


class TestDisplaySeat:
    """The seat is a cone, not a bore: the module wedges into it from behind."""

    def test_the_seat_narrows_towards_the_inside(self, spec: CubeSpec) -> None:
        assert spec.seat_mouth_diameter > spec.display_seat_diameter

    def test_the_mouth_is_wider_than_the_module(self, spec: CubeSpec) -> None:
        """Otherwise the module sits proud of the face instead of sinking in."""
        assert spec.seat_mouth_diameter > DISPLAY_MODULE_DIAMETER

    def test_the_throat_is_narrower_than_the_module(self, spec: CubeSpec) -> None:
        """Otherwise the module falls straight through."""
        assert spec.display_seat_diameter < DISPLAY_MODULE_DIAMETER

    def test_the_module_ends_up_below_the_outer_face(self, spec: CubeSpec) -> None:
        assert 0 < spec.module_seat_depth < spec.wall

    def test_nothing_masks_the_picture(self, spec: CubeSpec) -> None:
        assert spec.display_seat_diameter > DISPLAY_ACTIVE_DIAMETER

    def test_the_seat_covers_no_more_than_the_bezel(self, spec: CubeSpec) -> None:
        """The opening should show the bezel and none of the board behind."""
        assert spec.display_seat_diameter >= DISPLAY_BEZEL_DIAMETER

    def test_the_seat_opens_at_the_measured_diameters(
        self, enclosure: Enclosure
    ) -> None:
        """Mouth at the outer face, throat at the inner one, and nothing in
        between: an earlier chamfer bug widened the mouth by a millimetre."""
        spec = enclosure.spec
        seat = {
            round(edge.center().Z, 3): round(2 * edge.radius, 3)
            for edge in enclosure.top_plate.edges().filter_by(GeomType.CIRCLE)
            if edge.radius > spec.display_seat_diameter / 2 - TOLERANCE
        }
        assert seat == {
            0.0: pytest.approx(spec.seat_mouth_diameter, abs=1e-3),
            round(spec.wall, 3): pytest.approx(spec.display_seat_diameter, abs=1e-3),
        }

    def test_the_display_opening_is_not_chamfered(self, enclosure: Enclosure) -> None:
        """The cone is the bevel; a chamfer on top of it would move the seat."""
        spec = enclosure.spec
        outer = enclosure.top_plate.faces().sort_by(Axis.Z)[0]
        (mouth,) = outer.inner_wires()
        (edge,) = mouth.edges()
        assert 2 * edge.radius == pytest.approx(spec.seat_mouth_diameter, abs=1e-3)


class TestBoardRetention:
    """The board has no mounting holes, so it is clamped, not screwed through."""

    def test_there_are_four_bosses(self, enclosure: Enclosure) -> None:
        spec = enclosure.spec
        bores = [
            f
            for f in enclosure.top_plate.faces().filter_by(GeomType.CYLINDER)
            if abs(f.radius - spec.insert_bore_diameter / 2) < 1e-3
        ]
        assert len(bores) == 4

    def test_bosses_take_an_m2_heat_set_insert(self, spec: CubeSpec) -> None:
        assert spec.insert_bore_diameter == pytest.approx(3.20)

    def test_bosses_have_wall_left_around_the_bore(self, spec: CubeSpec) -> None:
        wall = (spec.boss_diameter - spec.insert_bore_diameter) / 2
        assert wall >= 1.2, f"only {wall:.2f} mm of boss around the insert"

    def test_bosses_clear_the_board(self, spec: CubeSpec) -> None:
        assert spec.boss_clearance_to_board > 1.0

    def test_the_bore_does_not_break_through_the_face(
        self, enclosure: Enclosure
    ) -> None:
        """A bore into the outer face would show on the finished cube."""
        outer = enclosure.top_plate.faces().sort_by(Axis.Z)[0]
        # Only the display seat opens onto the outside.
        assert len(outer.inner_wires()) == 1

    def test_clamp_bar_reaches_over_the_board_rim(self, spec: CubeSpec) -> None:
        overlap = PCB_DIAMETER / 2 - (spec.boss_offset - spec.clamp_bar_width / 2)
        assert overlap > 2.0, f"only {overlap:.2f} mm of bar lands on the board"

    def test_clamp_bar_has_a_hole_over_each_boss(self, enclosure: Enclosure) -> None:
        spec = enclosure.spec
        holes = [
            f
            for f in enclosure.clamp_bar.faces().filter_by(GeomType.CYLINDER)
            if abs(f.radius - spec.screw_clearance_diameter / 2) < 1e-3
        ]
        assert len(holes) == 2

    def test_screws_clear_their_holes_but_heads_do_not(self, spec: CubeSpec) -> None:
        assert spec.screw_clearance_diameter > 2.0  # M2 shank
        assert spec.screw_clearance_diameter < spec.clamp_bar_width

    def test_the_clamp_plane_is_inside_the_board_stack(self, spec: CubeSpec) -> None:
        """The bars bear on the board's rim, which sits in front of its rearmost
        components, so the clamp height must fall short of the whole stack."""
        assert spec.board_clamp_height < BOARD_STACK_DEPTH


class TestUsbCutout:
    """The socket points out of the board's edge, so it leaves through a wall."""

    def test_the_band_actually_has_an_opening(self, enclosure: Enclosure) -> None:
        """Measured as material removed, which a misplaced cutter would not do:
        the first attempt sketched the opening on swapped axes and cut nothing."""
        spec = enclosure.spec
        outer = spec.size**2 - (4 - math.pi) * spec.corner_radius**2
        inner = spec.cavity_size**2 - (4 - math.pi) * spec.cavity_radius**2
        unbroken = (outer - inner) * spec.band_height
        opening = (
            spec.usb_cutout_width * spec.usb_cutout_height
            - (4 - math.pi) * spec.usb_cutout_corner_radius**2
        )
        assert unbroken - enclosure.band.volume == pytest.approx(
            opening * spec.wall, rel=1e-3
        )

    def test_the_opening_clears_the_connector(self, spec: CubeSpec) -> None:
        clearance = spec.usb_cutout_width - USB_C_CONNECTOR_WIDTH
        assert clearance > 1.5, f"only {clearance:.2f} mm around the socket"

    def test_the_opening_sits_where_the_socket_does(self, spec: CubeSpec) -> None:
        """Depth below the display's outer face, which is what the board hangs
        off. The original's socket opening runs 6.00 to 12.25 mm in."""
        near = spec.usb_cutout_depth
        far = spec.usb_cutout_depth + spec.usb_cutout_height
        assert near == pytest.approx(6.00, abs=0.01)
        assert far == pytest.approx(12.25, abs=0.01)

    def test_the_opening_falls_wholly_within_the_band(self, spec: CubeSpec) -> None:
        top = spec.usb_cutout_top_in_band
        assert top > 0, "opening would break into the top plate"
        assert top + spec.usb_cutout_height < spec.band_height

    def test_the_opening_is_centred_on_its_wall(self, enclosure: Enclosure) -> None:
        spec = enclosure.spec
        wall = spec.size / 2
        cut_faces = [
            f
            for f in enclosure.band.faces()
            if f.center().X > wall - spec.wall - TOLERANCE
            and abs(f.center().Y) < spec.usb_cutout_width
            and f.center().Z > spec.band_height / 2
        ]
        assert cut_faces, "found no faces belonging to the opening"
        assert min(f.center().Y for f in cut_faces) < 0
        assert max(f.center().Y for f in cut_faces) > 0


class TestSpecValidation:
    def test_rejects_walls_that_meet_in_the_middle(self) -> None:
        with pytest.raises(ValueError, match="too thick"):
            CubeSpec(size=10.0, wall=5.0)

    def test_rejects_a_corner_radius_larger_than_the_cube(self) -> None:
        with pytest.raises(ValueError, match="exceeds half"):
            CubeSpec(size=20.0, corner_radius=15.0)

    def test_rejects_a_corner_thinner_than_the_wall(self) -> None:
        with pytest.raises(ValueError, match="knife edge"):
            CubeSpec(wall=8.0, corner_radius=6.0)

    def test_rejects_a_seat_that_masks_the_picture(self) -> None:
        with pytest.raises(ValueError, match="active area"):
            CubeSpec(display_seat_diameter=32.0)

    def test_rejects_a_seat_the_module_would_fall_through(self) -> None:
        with pytest.raises(ValueError, match="fall through"):
            CubeSpec(display_seat_diameter=39.0)

    def test_rejects_a_seat_mouth_wider_than_the_cavity(self) -> None:
        with pytest.raises(ValueError, match="does not fit"):
            CubeSpec(size=42.0, display_seat_angle=40.0)

    def test_rejects_bosses_that_foul_the_board(self) -> None:
        with pytest.raises(ValueError, match="foul"):
            CubeSpec(boss_offset=14.0)

    def test_rejects_a_usb_cutout_inside_the_top_plate(self) -> None:
        with pytest.raises(ValueError, match="top plate"):
            CubeSpec(usb_cutout_depth=2.0)

    def test_rejects_a_usb_cutout_that_runs_off_the_band(self) -> None:
        with pytest.raises(ValueError, match="runs off"):
            CubeSpec(usb_cutout_height=50.0)

    def test_rejects_a_usb_corner_radius_that_swallows_the_opening(self) -> None:
        with pytest.raises(ValueError, match="too large"):
            CubeSpec(usb_cutout_corner_radius=4.0)

    def test_rejects_a_clamp_bar_with_no_flank(self) -> None:
        with pytest.raises(ValueError, match="no flank"):
            CubeSpec(clamp_bar_width=6.0, clamp_bar_corner_radius=3.0)

    def test_accepts_the_defaults(self, spec: CubeSpec) -> None:
        assert spec.cavity_size == pytest.approx(49.0)
        assert spec.cavity_radius == pytest.approx(3.0)
        assert spec.seat_mouth_diameter == pytest.approx(38.94, abs=0.02)


class TestResizing:
    """The point of the model is that these numbers move."""

    @pytest.mark.parametrize("size", [50.0, 55.0, 70.0])
    def test_any_size_still_assembles_to_a_cube(self, size: float) -> None:
        measured = Enclosure(CubeSpec(size=size)).assembled().bounding_box().size
        assert measured.X == pytest.approx(size, abs=1e-3)
        assert measured.Z == pytest.approx(size, abs=1e-3)

    @pytest.mark.parametrize("wall", [2.0, 3.0, 4.0])
    def test_wall_thickness_drives_the_cavity(self, wall: float) -> None:
        spec = CubeSpec(wall=wall, corner_radius=max(6.0, wall))
        assert spec.cavity_size == pytest.approx(55.0 - 2 * wall)

    def test_a_thicker_wall_opens_the_seat_mouth(self) -> None:
        """The taper is an angle, so the mouth follows the wall."""
        assert CubeSpec(wall=4.0).seat_mouth_diameter > CubeSpec(
            wall=2.0
        ).seat_mouth_diameter
