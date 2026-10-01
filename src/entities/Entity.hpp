#pragma once

#include <string>

#include "core/Position.hpp"

namespace engine {

// Anything that exists in the world at a grid position: actors, items,
// features. Deliberately minimal -- no rendering, no SFML, no behavior.
// Whatever eventually draws entities (Prompt 5) will read this public
// state; Entity itself doesn't know or care how it's drawn.
class Entity {
public:
    Entity(std::string name, char glyph, Position position)
        : name_(std::move(name)), glyph_(glyph), position_(position) {}

    virtual ~Entity() = default;

    const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    // Placeholder visual identity (roguelike-style ASCII glyph) until a
    // real tile/sprite system exists. Deliberately not a texture ID or
    // sprite handle yet -- that's a Prompt 5 decision, not this one.
    char glyph() const { return glyph_; }

    const Position& position() const { return position_; }
    void setPosition(Position position) { position_ = position; }

private:
    std::string name_;
    char glyph_;
    Position position_;
};

} // namespace engine
