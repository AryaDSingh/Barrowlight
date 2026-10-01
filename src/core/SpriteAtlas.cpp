#include "core/SpriteAtlas.hpp"

#include <algorithm>
#include <iostream>

namespace engine {

namespace {
// Same "relative to launch directory" convention as kFontPath and the
// sound paths.
constexpr const char* kSpriteRoot = "assets/sprites/";
} // namespace

const sf::Texture* SpriteAtlas::texture(const char* sheet) {
    if (const auto it = byPointer_.find(sheet); it != byPointer_.end()) return it->second;
    if (const auto it = textures_.find(sheet); it != textures_.end()) return byPointer_[sheet] = &it->second;
    if (missing_.count(sheet)) return nullptr;

    sf::Texture loaded;
    const std::string path = std::string(kSpriteRoot) + sheet;
    if (!loaded.loadFromFile(path)) {
        std::cout << "Warning: failed to load sprite sheet " << path
                  << " -- falling back to flat colors for it.\n";
        missing_[sheet] = true;
        return nullptr;
    }
    // Pixel art: nearest-neighbor sampling keeps edges hard when the
    // 16/32px source frames are scaled to the 28px tile.
    loaded.setSmooth(false);
    return byPointer_[sheet] = &textures_.emplace(sheet, std::move(loaded)).first->second;
}

bool SpriteAtlas::draw(sf::RenderTarget& target, const SpriteFrame& frame, sf::Vector2f topLeft,
                       float size, sf::Color tint) {
    const sf::Texture* tex = texture(frame.sheet);
    if (!tex) return false;

    const float w = static_cast<float>(frame.rect.size.x);
    const float h = static_cast<float>(frame.rect.size.y);
    const float scale = size / std::max(w, h);

    sf::Sprite sprite(*tex, frame.rect);
    sprite.setScale({scale, scale});
    sprite.setPosition({topLeft.x + (size - w * scale) / 2.f, topLeft.y + (size - h * scale)});
    sprite.setColor(tint);
    target.draw(sprite);
    return true;
}

} // namespace engine
