#pragma once

#include "entities/Entity.hpp"

namespace engine {

// Non-actor, non-item world objects: doors, stairs, traps. Minimal for
// now -- no open/close state, no trigger logic. Just a distinct Entity
// subtype so the Map (Prompt 5) has somewhere to put these.
class Feature : public Entity {
public:
    using Entity::Entity;
};

} // namespace engine
