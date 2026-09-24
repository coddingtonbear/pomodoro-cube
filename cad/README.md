# Enclosure

A parametric 55 mm cube for the pomodoro cube, meant to be taken into Fusion 360
and fitted out there.

The model is [`cube.py`](cube.py); the board it is built around is
[`board.py`](board.py). Running `cube.py` writes STEP and STL for every part
into `build/`; the STEP files are committed, the STLs are not.

```sh
.venv/bin/python cube.py      # regenerate build/
.venv/bin/python preview.py   # regenerate preview.png
.venv/bin/python -m pytest    # 69 tests
```

First time:

```sh
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
```

## The parts

![Assembled, exploded, the board retention, and the USB-C opening](preview.png)

| part | qty | what it is |
|---|---|---|
| band | 1 | the four side faces as one square tube, 55 × 55 × 49, with the USB-C opening |
| top-plate | 1 | the display's face: conical seat, plus the four bosses that hold the board |
| back-plate | 1 | a plain plate with nothing in it |
| clamp-bar | 2 | screws down over two bosses and traps the board's rim |

Together they make an exact 55 × 55 × 55 mm cube. Each part is modelled in its
print orientation, lying on the bed at z=0, so nothing needs support and the
visible faces get the smooth side.

There is still no battery bay, no beeper hole, no face numerals and no joinery
between the band and the plates. Those are for Fusion.

## Everything derives from the board

`board.py` holds every dimension of the Waveshare module, measured from the STEP
Waveshare publish by loading it with build123d rather than reading the dimension
drawing. `CubeSpec` then derives the seat depth, the clamp height and the
USB-C opening from those figures, so a board that measures differently moves all
of them together. The one number you choose is `display_recess` — how far the
bezel should sit below the finished face.

The datum is the **Ø38.51 shoulder, not the glass.** The module's front 0.50 mm
is a Ø35.67 bezel standing proud of a Ø38.51 body, and it is that body the
conical seat grips. Seating it flush would leave the bezel sticking out, so the
seat is cut deep enough to bury the bezel's own height plus the wanted recess:

```
module_seat_depth = display_recess + bezel_proud_of_shoulder   = 0.90 mm
seat_mouth        = module_diameter + 2·depth·tan(28.4°)       = 39.48 mm
seat_throat       = seat_mouth − 2·wall·tan(28.4°)             = 36.24 mm
board_clamp_height= depth + pcb_back_behind_shoulder − wall     =  2.40 mm
usb_cutout_depth  = depth + usb_socket_top_behind_shoulder − clearance = 3.99 mm
```

## How the board is held

The Waveshare board **has no mounting holes** — its only through-holes are
0.50 and 0.76 mm vias. So it cannot be screwed down, and both the original and
this model clamp it instead.

**The conical seat.** The display opening is not a bore. It narrows from
39.48 mm at the outer face to 36.24 mm at the inner one, a 28.4° taper. The
module's Ø38.51 body cannot pass through, and it cannot sit proud either: it
settles into the cone until the walls close to its own diameter, 0.90 mm below
the outer surface, which leaves the bezel face 0.40 mm recessed.

**Four bosses and two bars.** The bosses stand 2.40 mm off the plate's inner
face on the diagonals, 20 mm out from the axis, each bored 3.20 mm for an M2
heat-set insert. They clear the 37.54 mm board by 6.52 mm. The two bars screw
down onto them and reach 2.27 mm over the board's rim.

**The bars run square to the socket, and that is not cosmetic.** The USB-C
socket hangs 3.25 mm off the *back* of the PCB — the same face the bars bear on
— so a bar crossing the board's tab lands on the socket rather than the board.
`clamp_bar_bearing` is therefore always `usb_cutout_bearing + 90°`, and a spec
that would put a bar on the socket is rejected.

**Nothing yet stops the board rotating.** The seat is a cone against a round
module and the bars press on a round rim: both are rotationally symmetric, so
the board is held by friction at any angle. The board's trapezoidal USB tab is
the obvious key — a matching notch in a raised outline on the plate would fix
the angle to about ±1° — but that is not modelled yet.

## The USB-C opening

The socket points radially out of the board's tab rather than backwards, so it
has to leave through a side wall — there is no arrangement that brings it out of
the back plate. The opening is **11.58 × 5.66 mm**, running 3.99 to 9.65 mm in
from the display's outer face, derived from the socket's own 9.58 × 4.16 mm body
plus clearance.

`usb_cutout_bearing` turns it to another wall if the board is to sit the other
way round; the clamp bars turn with it.

Two consequences worth knowing before printing:

- **The ligament above the opening is thin.** The socket sits close to the
  display, so only **0.99 mm** of band is left above the opening. A plate
  thicker than about 3.99 mm makes it negative, and the spec rejects that.
  Letting the opening run through the band/plate seam as a notch in both would
  be the sturdier answer.
- **A plug has to reach.** The socket's face sits 2.72 mm inside the wall's
  inner surface, so a cable must insert 5.72 mm past the cube's outer surface
  before it engages.

## Where the numbers came from

The shell is the original — [Timer cube by Robin](https://www.printables.com/model/1774785-timer-cube),
which the firmware here is forked from — measured off its STLs rather than
eyeballed: 55 mm footprint, 3.00 mm wall, 6.00 mm outer vertical corner radius
(3.00 mm inner), 0.50 mm bottom chamfer, and a 28.4° seat taper. The original is
55 × 55 × **62** mm, not a cube; ours is a true cube, which is 7 mm less height
for the same internals.

Everything about the board comes from the STEP under Resources on the
[ESP32-S3-Touch-LCD-1.28 wiki page](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28),
read with build123d. Three of those measurements contradict figures the model
previously carried over from the original or from the dimension drawing:

| | quoted | measured |
|---|---|---|
| PCB diameter | 39.53 mm | **37.54 mm** — 39.53 is the span including the USB tab |
| board outline | circle with a chord flat | **circle with a trapezoidal tab**, 45° flanks to a 9.92 mm flat |
| USB socket width | 9.92 mm | **9.58 mm** — 9.92 is the tab it sits on |

The original's own internals did not carry over, being for features this model
leaves alone: a 38.5 × 10.4 × 28 battery bay, a Ø12.70 beeper hole, four more M2
inserts on a 47.24 mm square for the lid, and numerals standing 0.30 mm proud
for a single-layer colour change.
