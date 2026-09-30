# Enclosure

A 44.9 mm cube, designed in Fusion 360. The design is the Fusion document
**Pomodoro Box**; the files here are exports of it, and are replaced wholesale
whenever it changes rather than edited.

It's published on Printables as
[Pomodoro Cube](https://www.printables.com/model/1860676-pomodoro-cube).

| file | what it is |
|---|---|
| [`pomodoro-cube.f3d`](pomodoro-cube.f3d) | the Fusion 360 design itself, for opening and remixing |
| [`pomodoro-cube.step`](pomodoro-cube.step) | the assembly: one component per part, in their assembled positions, without the board |
| [`pomodoro-cube.3mf`](pomodoro-cube.3mf) | all four parts laid out on one plate, ready to slice |
| [`step/`](step/) | one STEP per part, oriented for printing like the STLs |
| [`stl/`](stl/) | one STL per printed part, already oriented for printing |

To print, open the 3MF, or the per-part STEPs or STLs. The assembly STEP is for
CAD; a slicer would load its parts nested inside each other.

## The parts

| part | size (mm) | print | files |
|---|---|---|---|
| center | 44.9 × 44.9 × 38.9 | upright, as designed | [STL](stl/pomodoro-cube-center.stl), [STEP](step/pomodoro-cube-center.step) |
| face | 44.9 × 44.9 × 11.4 | outer face down | [STL](stl/pomodoro-cube-face.stl), [STEP](step/pomodoro-cube-face.step) |
| back | 44.9 × 44.9 × 3.0 | outer face down | [STL](stl/pomodoro-cube-back.stl), [STEP](step/pomodoro-cube-back.step) |
| barrier | 38.5 × 38.5 × 1.0 | flat | [STL](stl/pomodoro-cube-barrier.stl), [STEP](step/pomodoro-cube-barrier.step) |

**Center** is the shell, with the display pocket, the internal ledges and the
openings in its side walls. **Face** is the display's front and
plugs into the top of it; **back** closes the bottom. **Barrier** is a thin
plate that sits inside the shell about 10 mm behind the display.

Besides the board, it holds a 102535 Li-Po battery and a vibration motor, and
four M2.6 × 6 mm self-tapping screws hold it together: two inside fix the
display to center, and two outside fix back to center. See
[Parts](../README.md#parts) for links.

Each STL was checked watertight on export, and each part lies in whichever
axis-aligned orientation leaves the least overhang steeper than 45°. Face still
has some, from the display bevel and the edge rounds on the bed.

## Updating

Fusion runs on another machine, so exports are made there and copied in. When
the design changes, replace every file here, keeping the names so links to them
survive:

- **Fusion archive**: the design as-is.
- **STL**: every body, in its print orientation.
- **STEP**: the design keeps all four parts as bodies of one component, so
  exporting the root gives a single part that slicers load as one lump. Instead,
  copy the bodies into a scratch design with one component each: one set in
  place, exported together as the assembly, and one set rotated to print
  orientation (face and back turn 180° about X), each exported alone.
- **3MF**: all four parts, in print orientation, as separate objects on one
  plate. The current one was built from the STLs, in a 2 × 2 grid with 10 mm
  gaps centred at (100, 100) so it fits a 180 mm bed.

## History

Until September 2026 the enclosure was a parametric build123d model here: a
55 mm cube in a band, two plates and two clamp bars, with every dimension
derived from the board's published STEP. It was taken into Fusion and then
redesigned there over many iterations, and was removed once it no longer
described the part that is printed. It is in the git history before this
directory was replaced.

The shell descends from Robin's
[Timer cube](https://www.printables.com/model/1774785-timer-cube), the
enclosure for [fly-robin-fly/coffee_timer](https://github.com/fly-robin-fly/coffee_timer),
which the firmware here is forked from.
