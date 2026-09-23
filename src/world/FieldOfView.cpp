#include "world/FieldOfView.hpp"

namespace engine {

namespace {

// One octant's worth of recursive shadowcasting. The map is swept as 8
// octants around origin; (dx, dy) in "octant-local" coordinates (dy is
// always <= 0, walking outward from the origin) is transformed into
// world space via the xx/xy/yx/yy multipliers below. That's the standard
// trick that lets one recursive function handle all 8 octants, rather
// than writing 8 near-identical cases by hand.
//
// startSlope/endSlope bound the current arc of unobstructed visibility
// within the octant (1.0 = the octant's outer edge, 0.0 = its center
// line). When an obstruction is found mid-row, the arc splits: the
// function recurses one row deeper for the still-open portion before
// continuing the current row past the obstruction.
void castLight(const Map& map, Position origin, int radius, int row,
               double startSlope, double endSlope, int xx, int xy, int yx,
               int yy, std::vector<Position>& visible) {
    if (startSlope < endSlope) {
        return;
    }

    double nextStartSlope = startSlope;
    for (int i = row; i <= radius; ++i) {
        const int dy = -i;
        bool blocked = false;

        for (int dx = -i; dx <= 0; ++dx) {
            const double leftSlope = (dx - 0.5) / (dy + 0.5);
            const double rightSlope = (dx + 0.5) / (dy - 0.5);

            if (startSlope < rightSlope) {
                continue; // this cell is past the start of our arc, skip it
            }
            if (endSlope > leftSlope) {
                break; // this cell (and the rest of the row) is past our arc's end
            }

            const int worldX = origin.x + dx * xx + dy * xy;
            const int worldY = origin.y + dx * yx + dy * yy;

            // Distance-squared check keeps the lit area roughly circular
            // rather than a diamond (Manhattan) or square (Chebyshev).
            if (dx * dx + dy * dy <= radius * radius && map.inBounds(worldX, worldY)) {
                visible.push_back(Position{worldX, worldY});
            }

            const bool tileBlocksSight =
                !map.inBounds(worldX, worldY) || !map.tileAt(worldX, worldY).transparent;

            if (blocked) {
                if (tileBlocksSight) {
                    // Still inside a blocked run -- narrow the arc for
                    // when we exit it and keep scanning this row.
                    nextStartSlope = rightSlope;
                    continue;
                }
                // Just exited a blocked run: resume an unobstructed arc.
                blocked = false;
                startSlope = nextStartSlope;
            } else if (tileBlocksSight && i < radius) {
                // Hit an obstruction: everything before it (already
                // scanned) continues outward one row deeper as its own
                // arc, then we keep scanning this row past the
                // obstruction with a narrowed arc.
                blocked = true;
                castLight(map, origin, radius, i + 1, startSlope, leftSlope, xx,
                          xy, yx, yy, visible);
                nextStartSlope = rightSlope;
            }
        }

        if (blocked) {
            break; // the whole rest of the arc was consumed by obstructions
        }
    }
}

} // namespace

std::vector<Position> computeFieldOfView(const Map& map, Position origin, int radius) {
    std::vector<Position> visible;

    if (map.inBounds(origin.x, origin.y)) {
        visible.push_back(origin);
    }

    // xx, xy, yx, yy per octant. Octant order/numbering doesn't matter,
    // only that all 8 are covered exactly once each.
    static constexpr int kOctantMultipliers[8][4] = {
        {1, 0, 0, 1},   {0, 1, 1, 0},   {0, -1, 1, 0},  {-1, 0, 0, 1},
        {-1, 0, 0, -1}, {0, -1, -1, 0}, {0, 1, -1, 0},  {1, 0, 0, -1},
    };

    for (const auto& m : kOctantMultipliers) {
        castLight(map, origin, radius, 1, 1.0, 0.0, m[0], m[1], m[2], m[3], visible);
    }

    return visible;
}

} // namespace engine
