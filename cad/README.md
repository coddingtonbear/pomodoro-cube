# Enclosure

A parametric 55 mm cube for the pomodoro cube, as three printable parts, meant
to be taken into Fusion 360 and fitted out there.

The model is [`cube.py`](cube.py). Running it writes STEP and STL for every
part into `build/`; the STEP files are committed, the STLs are not.

```sh
.venv/bin/python cube.py      # regenerate build/
.venv/bin/python -m pytest    # 34 tests
```

First time:

```sh
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
```

## What it is, and what it deliberately is not

Three parts, together making an exact 55 × 55 × 55 mm cube:

- **band** — the four side faces as one square tube
- **top-plate** — the face the display looks out of, carrying its aperture
- **back-plate** — a plain plate with nothing in it

There is no battery bay, no beeper hole, no face numerals and no joinery. Those
are for Fusion, which is why the parts are separate solids that merely stack:
adding a lip, screw bosses or magnets is easier on a clean plate than on
something already committed to a fixing scheme.

## The two split styles

The cube has six faces and there are three parts, so two faces have to come
from somewhere. Both readings are built, and they look different in the hand:

| | band | plates | seam reads as |
|---|---|---|---|
| `PLATES_AS_FACES` (default) | 55 × 55 × 49 | full 55 × 55 | a line around all four sides |
| `PLATES_INSET` | full 55 × 55 × 55 | 48.8 × 48.8, dropped in | an inset square on top and back |

![Both split styles, assembled and exploded](preview.png)

Regenerate that with `.venv/bin/python preview.py`.

`PLATES_AS_FACES` keeps the top face unbroken, which suits the display;
`PLATES_INSET` keeps the four side faces unbroken, which suits a cube you pick
up and turn over.

## Where the numbers came from

The original — [Timer cube by Robin](https://www.printables.com/model/1774785-timer-cube),
which the firmware here is forked from — is 55 × 55 × **62** mm, not a cube.
Its STLs were measured directly rather than eyeballed, and these carried over:

| | |
|---|---|
| Outer size | 55.00 mm (the original's footprint, now applied to all three axes) |
| Wall | 3.00 mm |
| Vertical corner radius | 6.00 mm outer, 3.00 mm inner |
| Bottom edge chamfer | 0.50 mm |
| LCD aperture | 37.81 mm |

These did not, being for internals this model leaves alone: a 38.5 × 10.4 × 28
battery bay, a Ø12.70 beeper hole, four M2 heat-set inserts on a 47.24 mm
square, and numerals standing 0.30 mm proud for a single-layer colour change.

The aperture figure is worth keeping: the
[Waveshare ESP32-S3-Touch-LCD-1.28](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28)
display module is Ø38.51 mm with a Ø33.40 mm active area, so 37.81 mm leaves a
0.35 mm lip to sit behind while masking none of the picture. Waveshare publish
a STEP of the whole board under Resources on that page, which is the thing to
measure against when the internals get designed.
