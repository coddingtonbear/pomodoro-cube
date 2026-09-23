"""Tests for the enclosure model.

The things worth pinning are the ones a careless edit to a parameter would
break silently: that the three parts still add up to an exact cube, that each
is a single watertight solid a slicer will accept, and that the guards in
:class:`CubeSpec` actually fire.
"""

from __future__ import annotations

import math

import pytest
from build123d import Axis, GeomType, Part

from cube import CubeSpec, Enclosure, SplitStyle

TOLERANCE = 1e-6


@pytest.fixture(params=list(SplitStyle), ids=lambda s: s.value)
def style(request) -> SplitStyle:
    return request.param


@pytest.fixture
def enclosure(style: SplitStyle) -> Enclosure:
    return Enclosure(CubeSpec(split_style=style))


def inward_face(part: Part, name: str):
    """The plate face that points into the cube.

    The outward face carries the chamfer, so measurements of the plate's true
    footprint have to be taken from the other side.
    """
    return part.faces().sort_by(Axis.Z)[0 if name == "top-plate" else -1]


class TestAssembly:
    def test_assembles_to_an_exact_cube(self, enclosure: Enclosure) -> None:
        size = enclosure.assembled().bounding_box().size
        for measured in (size.X, size.Y, size.Z):
            assert measured == pytest.approx(enclosure.spec.size, abs=1e-3)

    def test_heights_sum_to_the_cube(self, enclosure: Enclosure) -> None:
        spec = enclosure.spec
        if spec.split_style is SplitStyle.PLATES_AS_FACES:
            assert spec.band_height + 2 * spec.wall == pytest.approx(spec.size)
        else:
            assert spec.band_height == pytest.approx(spec.size)

    def test_there_are_exactly_three_parts(self, enclosure: Enclosure) -> None:
        assert set(enclosure.parts) == {"band", "top-plate", "back-plate"}


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
        """Measured between the flat faces, which the chamfers never touch."""
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

    def test_plates_are_wall_thick(self, enclosure: Enclosure) -> None:
        for name in ("top-plate", "back-plate"):
            height = enclosure.parts[name].bounding_box().size.Z
            assert height == pytest.approx(enclosure.spec.wall, abs=1e-3)

    def test_back_plate_is_unbroken(self, enclosure: Enclosure) -> None:
        """Nothing is cut into it yet — the internals come later, in Fusion."""
        spec = enclosure.spec
        expected = spec.plate_size**2 - (4 - math.pi) * spec.plate_radius**2
        footprint = inward_face(enclosure.back_plate, "back-plate").area
        assert footprint == pytest.approx(expected, rel=1e-3)


class TestLcdAperture:
    def test_top_plate_has_a_hole_of_the_right_size(self, enclosure: Enclosure) -> None:
        spec = enclosure.spec
        solid_area = spec.plate_size**2 - (4 - math.pi) * spec.plate_radius**2
        aperture_area = math.pi * (spec.lcd_aperture_diameter / 2) ** 2
        face = inward_face(enclosure.top_plate, "top-plate")
        assert face.area == pytest.approx(solid_area - aperture_area, rel=1e-3)

    def test_aperture_is_narrower_than_the_display_module(self) -> None:
        """The module is 38.51 mm across and must not fall through."""
        assert CubeSpec().lcd_aperture_diameter < 38.51

    def test_aperture_clears_the_active_area(self) -> None:
        """Nothing of the 33.40 mm visible area may be masked."""
        assert CubeSpec().lcd_aperture_diameter > 33.40


class TestSplitStyles:
    def test_plates_as_faces_gives_full_width_plates(self) -> None:
        spec = CubeSpec(split_style=SplitStyle.PLATES_AS_FACES)
        assert spec.plate_size == spec.size

    def test_inset_plates_clear_the_cavity(self) -> None:
        spec = CubeSpec(split_style=SplitStyle.PLATES_INSET)
        assert spec.plate_size < spec.cavity_size
        assert spec.cavity_size - spec.plate_size == pytest.approx(spec.fit_clearance)

    def test_inset_plates_keep_the_corner_concentric(self) -> None:
        spec = CubeSpec(split_style=SplitStyle.PLATES_INSET)
        gap = spec.cavity_radius - spec.plate_radius
        assert gap == pytest.approx(spec.fit_clearance / 2)


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

    def test_rejects_an_aperture_wider_than_the_cavity(self) -> None:
        with pytest.raises(ValueError, match="does not fit"):
            CubeSpec(size=40.0, lcd_aperture_diameter=37.81)

    def test_accepts_the_defaults(self) -> None:
        spec = CubeSpec()
        assert spec.cavity_size == pytest.approx(49.0)
        assert spec.cavity_radius == pytest.approx(3.0)


class TestResizing:
    """The point of the model is that these numbers move."""

    @pytest.mark.parametrize("size", [45.0, 55.0, 70.0])
    def test_any_size_still_assembles_to_a_cube(self, size: float) -> None:
        enclosure = Enclosure(CubeSpec(size=size))
        measured = enclosure.assembled().bounding_box().size
        assert measured.X == pytest.approx(size, abs=1e-3)
        assert measured.Z == pytest.approx(size, abs=1e-3)

    @pytest.mark.parametrize("wall", [2.0, 3.0, 4.0])
    def test_wall_thickness_drives_the_cavity(self, wall: float) -> None:
        spec = CubeSpec(wall=wall, corner_radius=max(6.0, wall))
        assert spec.cavity_size == pytest.approx(55.0 - 2 * wall)
