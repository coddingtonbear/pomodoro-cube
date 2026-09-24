"""The board the enclosure is built around, measured from its own STEP model.

Every figure here came from the STEP Waveshare publish under Resources on the
`ESP32-S3-Touch-LCD-1.28 wiki page
<https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28>`_, read with
``import_step`` rather than taken from the dimension drawing. Swap the values
for measurements off a real board when one is in hand; everything in
:mod:`cube` derives from them.

The datum is the **Ø38.51 shoulder**, not the glass. The display module's front
0.50 mm is a Ø35.67 bezel standing proud of a Ø38.51 body, and it is that body
the conical seat grips, so the shoulder is the surface whose depth the
enclosure actually controls. Depths below are measured back from it.
"""

from __future__ import annotations

import math
from dataclasses import dataclass


@dataclass(frozen=True)
class BoardSpec:
    """Every board dimension the enclosure depends on."""

    name: str = "Waveshare ESP32-S3-Touch-LCD-1.28"

    #: Diameter of the round part of the PCB.
    #:
    #: Note this is *not* the board's overall extent: the 39.53 mm often quoted
    #: for this board is its span including the USB-C tab, and the circle it is
    #: measured across is a millimetre smaller.
    pcb_diameter: float = 37.537
    #: Thickness of the bare PCB.
    pcb_thickness: float = 1.000

    #: The PCB is not a circle with a chord flat. A trapezoidal tab carries the
    #: USB-C socket: two 45 degree flanks leave the circle and run down to a
    #: flat this wide.
    tab_width: float = 9.916
    #: Axis to the tab's flat end.
    tab_reach: float = 20.755
    #: Angle of the tab's flanks, measured from the tab's own axis.
    tab_flank_angle: float = 45.0
    #: Radius easing the tab's two outer corners.
    tab_corner_radius: float = 0.394

    #: Outside diameter of the display module's body — the surface the conical
    #: seat grips, and the widest thing on the board.
    module_diameter: float = 38.510
    #: The module's black border, standing proud of that body.
    bezel_diameter: float = 35.670
    #: Visible picture. Nothing may mask any of it.
    active_diameter: float = 33.400
    #: How far the bezel's face stands in front of the Ø38.51 shoulder. This is
    #: why seating the shoulder flush leaves the bezel proud.
    bezel_proud_of_shoulder: float = 0.500

    #: PCB's front face, behind the shoulder.
    pcb_front_behind_shoulder: float = 3.500
    #: PCB's back face, behind the shoulder. This is the plane the clamp bears
    #: on, and what sets the boss height.
    pcb_back_behind_shoulder: float = 4.500
    #: Back of the rearmost component, behind the shoulder. Nothing may sit
    #: closer to the back plate than this.
    rearmost_behind_shoulder: float = 7.900

    #: Width of the USB-C socket's body. Narrower than the tab it sits on.
    usb_socket_width: float = 9.583
    #: Height of the socket's body.
    usb_socket_height: float = 4.160
    #: Top of the socket, behind the shoulder.
    usb_socket_top_behind_shoulder: float = 3.590
    #: Axis to the socket's outer face. It overhangs the tab a little.
    usb_socket_reach: float = 21.781

    def __post_init__(self) -> None:
        if self.module_diameter <= self.pcb_diameter:
            raise ValueError(
                f"module {self.module_diameter} should overhang the "
                f"{self.pcb_diameter} mm PCB"
            )
        if not self.active_diameter < self.bezel_diameter < self.module_diameter:
            raise ValueError("active area, bezel and module are not nested")
        if self.pcb_back_behind_shoulder <= self.pcb_front_behind_shoulder:
            raise ValueError("PCB's back face is not behind its front face")
        if self.rearmost_behind_shoulder < self.pcb_back_behind_shoulder:
            raise ValueError("rearmost component is in front of the PCB's back")
        if self.usb_socket_width > self.tab_width:
            raise ValueError(
                f"socket {self.usb_socket_width} is wider than the "
                f"{self.tab_width} mm tab it sits on"
            )

    @property
    def stack_depth(self) -> float:
        """Glass front to the back of the rearmost component."""
        return self.bezel_proud_of_shoulder + self.rearmost_behind_shoulder

    @property
    def usb_socket_behind_pcb(self) -> float:
        """How far the socket hangs off the back of the PCB.

        The clamp bears on that same face, so anything crossing the tab will
        meet the socket rather than the board.
        """
        return (
            self.usb_socket_top_behind_shoulder
            + self.usb_socket_height
            - self.pcb_back_behind_shoulder
        )

    @property
    def tab_half_angle(self) -> float:
        """Half the angle the tab and its flanks subtend, from the axis."""
        return math.degrees(math.asin(min(1.0, (self.tab_width / 2) / (self.pcb_diameter / 2))))


WAVESHARE_ESP32_S3_TOUCH_LCD_1_28 = BoardSpec()
