#pragma once

#include "entities/Entity.hpp"

namespace engine {

// Something that can sit on the ground or be carried in an Inventory.
// Minimal for now -- no item types, no stat bonuses, no usage logic.
// That's real design work for a later prompt, not a Prompt 3 concern.
class Item : public Entity {
public:
    using Entity::Entity;
};

} // namespace engine
