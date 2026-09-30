# Enclosure

A 44.9 mm cube, designed in Fusion 360. The design is the Fusion document
**Pomodoro Box**; the files here are exports of it, and are replaced wholesale
whenever it changes rather than edited.

| file | what it is |
|---|---|
| [`pomodoro-cube.f3d`](pomodoro-cube.f3d) | the Fusion 360 design itself, for opening and remixing |
| [`pomodoro-cube.step`](pomodoro-cube.step) | all four parts in their assembled positions, without the board |
| [`stl/`](stl/) | one STL per printed part, already oriented for printing |

## The parts

| part | size (mm) | print |
|---|---|---|
| [center](stl/pomodoro-cube-center.stl) | 44.9 × 44.9 × 38.9 | upright, as designed |
| [face](stl/pomodoro-cube-face.stl) | 44.9 × 44.9 × 11.4 | outer face down |
| [back](stl/pomodoro-cube-back.stl) | 44.9 × 44.9 × 3.0 | outer face down |
| [barrier](stl/pomodoro-cube-barrier.stl) | 38.5 × 38.5 × 1.0 | flat |

**Center** is the shell, with the display pocket, the internal ledges and the
openings in its side walls. **Face** is the display's front and
plugs into the top of it; **back** closes the bottom. **Barrier** is a thin
plate that sits inside the shell about 10 mm behind the display.

Each STL was checked watertight on export, and each part lies in whichever
axis-aligned orientation leaves the least overhang steeper than 45°. Face still
has some, from the display bevel and the edge rounds on the bed.

## Updating

Fusion runs on another machine, so exports are made there and copied in. When
the design changes, export every body as STL, the root component as STEP, and
the design as a Fusion archive, then replace the files here. Keep the file names
so links to them survive.

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
