#pragma once

namespace engine {

// Grid coordinates. A foundational, shared type -- both entities/ and
// world/ need it, so it lives in core/ rather than being owned by either.
// (Originally defined inside Entity.hpp; moved out here once world/ also
// needed it, see ARCHITECTURE_DECISIONS.md.)
struct Position {
    int x = 0;
    int y = 0;
};

} // namespace engine
