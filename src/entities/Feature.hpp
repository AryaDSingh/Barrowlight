#pragma once

#include "entities/Entity.hpp"

namespace engine {

// Non-actor, non-item world objects: a distinct Entity subtype for things
// like doors and stairs. The live game represents most of these as tiles,
// props and landmarks instead (world/), which proved simpler.
class Feature : public Entity {
public:
    using Entity::Entity;
};

} // namespace engine
