#!/usr/bin/env python3
"""Force tabular (fixed-width) digits in an lv_font_conv-generated font.

A countdown ticks once a second, and most proportional fonts draw a much
narrower "1" than an "0". On a centred label that makes the whole time jump
sideways whenever a 1 appears or leaves -- Open Gorton swings by ~80px out of a
240px panel between "50:00" and "11:11". Fonts designed for this (JetBrains
Mono, DSEG7) already have equal digit advances; this makes any other font
behave the same way.

It rewrites the generated glyph descriptors rather than the source TTF, because
the advance and the glyph's horizontal offset are plain numbers there -- no
need to touch glyf or CFF outlines. Each digit is given the widest digit's
advance and re-centred within it.

    tools/tabular-digits.py <generated font .c>
"""

import re
import sys

# lv_font_conv writes advances in 1/16 px fixed point.
ADV_FRACTION = 16

GLYPH_DSC_RE = re.compile(
    r"\{\.bitmap_index = (?P<bitmap>\d+), \.adv_w = (?P<adv>\d+), "
    r"\.box_w = (?P<box_w>\d+), \.box_h = (?P<box_h>\d+), "
    r"\.ofs_x = (?P<ofs_x>-?\d+), \.ofs_y = (?P<ofs_y>-?\d+)\}"
)
CMAP_RE = re.compile(
    r"\.range_start = (?P<start>\d+), \.range_length = (?P<length>\d+), "
    r"\.glyph_id_start = (?P<gid>\d+)"
)

DIGIT_FIRST = ord("0")
DIGIT_LAST = ord("9")


def digit_glyph_ids(source):
    """Glyph ids for '0'-'9', read from the font's own cmap subtables."""
    ids = {}
    for match in CMAP_RE.finditer(source):
        start = int(match.group("start"))
        length = int(match.group("length"))
        gid = int(match.group("gid"))
        for offset in range(length):
            codepoint = start + offset
            if DIGIT_FIRST <= codepoint <= DIGIT_LAST:
                ids[codepoint] = gid + offset
    return ids


def main():
    if len(sys.argv) != 2:
        print(__doc__.strip(), file=sys.stderr)
        return 1

    path = sys.argv[1]
    with open(path) as handle:
        source = handle.read()

    ids = digit_glyph_ids(source)
    missing = [chr(c) for c in range(DIGIT_FIRST, DIGIT_LAST + 1) if c not in ids]
    if missing:
        print(
            "%s: font is missing digit(s) %s; not making digits tabular"
            % (path, "".join(missing)),
            file=sys.stderr,
        )
        return 1

    glyphs = list(GLYPH_DSC_RE.finditer(source))
    widest = max(int(glyphs[gid].group("adv")) for gid in ids.values())

    replacements = {}
    for gid in ids.values():
        glyph = glyphs[gid]
        advance = int(glyph.group("adv"))
        if advance == widest:
            continue
        # Re-centre the glyph inside its now-wider cell.
        shift = round((widest - advance) / ADV_FRACTION / 2)
        replacements[glyph.start()] = (
            "{.bitmap_index = %s, .adv_w = %d, .box_w = %s, .box_h = %s, "
            ".ofs_x = %d, .ofs_y = %s}"
            % (
                glyph.group("bitmap"),
                widest,
                glyph.group("box_w"),
                glyph.group("box_h"),
                int(glyph.group("ofs_x")) + shift,
                glyph.group("ofs_y"),
            )
        )

    if not replacements:
        print("%s: digits were already tabular" % path)
        return 0

    out = []
    cursor = 0
    for glyph in glyphs:
        if glyph.start() not in replacements:
            continue
        out.append(source[cursor:glyph.start()])
        out.append(replacements[glyph.start()])
        cursor = glyph.end()
    out.append(source[cursor:])

    with open(path, "w") as handle:
        handle.write("".join(out))

    print(
        "%s: widened %d digit(s) to %.2f px for tabular spacing"
        % (path, len(replacements), widest / ADV_FRACTION)
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
