#include "core/Application.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>

#include "core/PlayLayout.hpp"

namespace engine {

// Character sheets are 10 frames wide with one row per animation:
// idle, gesture, walk, attack, death. Everything here is cosmetic and
// time-based: game state never waits for an animation to finish.
namespace {
constexpr int kIdleRow = 0, kWalkRow = 2, kAttackRow = 3, kDeathRow = 4, kFrames = 10;
constexpr float kStepSeconds = 0.11f;    // tile-to-tile glide; matches auto-explore pacing
constexpr float kWalkSeconds = 0.35f;    // walk cycle shown after a step
constexpr float kAttackSeconds = 0.45f;
constexpr float kDeathSeconds = 0.8f, kCorpseFadeSeconds = 1.2f;
constexpr float kTile = static_cast<float>(playLayout::tileSize);
}

float Application::animNow() const { return animationClock_.getElapsedTime().asSeconds(); }

sf::Vector2f Application::cameraShift() const {
    const float t = (animNow() - cameraShiftTime_) / kStepSeconds;
    return t >= 1.f ? sf::Vector2f{} : cameraShiftStart_ * (1.f - t);
}

// Called once per frame after the camera settles: a small camera move
// becomes a slide instead of a jump.
void Application::updateCameraShift() {
    if (previousCameraX_ != INT_MIN) {
        const int dx = cameraX_ - previousCameraX_, dy = cameraY_ - previousCameraY_;
        if ((dx || dy) && std::abs(dx) <= 2 && std::abs(dy) <= 2) {
            cameraShiftStart_ = cameraShift() + sf::Vector2f{dx * kTile, dy * kTile};
            cameraShiftTime_ = animNow();
        } else if (dx || dy) {
            cameraShiftStart_ = {};
        }
    }
    previousCameraX_ = cameraX_;
    previousCameraY_ = cameraY_;
}

void Application::notifyAttack(const Actor& actor, Position target) {
    auto& anim = actorAnims_[&actor];
    anim.attackStart = animNow();
    if (target.x != actor.position().x) anim.faceLeft = target.x < actor.position().x;
}

Application::ActorPose Application::actorPose(const Actor& actor) {
    auto& anim = actorAnims_[&actor];
    const float now = animNow();
    const Position tile = actor.position();
    if (!anim.initialised) {
        anim.initialised = true;
        anim.lastTile = anim.fromTile = tile;
        anim.moveStart = -10.f;
        anim.phase = static_cast<float>((reinterpret_cast<std::uintptr_t>(&actor) >> 4) % 97) * 0.13f;
    }
    if (tile.x != anim.lastTile.x || tile.y != anim.lastTile.y) {
        const int dx = tile.x - anim.lastTile.x, dy = tile.y - anim.lastTile.y;
        if (std::abs(dx) + std::abs(dy) <= 2) { anim.fromTile = anim.lastTile; anim.moveStart = now; }
        else anim.fromTile = tile; // teleports and blinks don't glide
        if (dx) anim.faceLeft = dx < 0;
        anim.lastTile = tile;
    }
    const float glide = std::clamp((now - anim.moveStart) / kStepSeconds, 0.f, 1.f);
    const sf::Vector2f world{anim.fromTile.x + (tile.x - anim.fromTile.x) * glide,
                             anim.fromTile.y + (tile.y - anim.fromTile.y) * glide};
    ActorPose pose;
    const auto shift = cameraShift();
    pose.screen = {static_cast<float>(playLayout::mapLeft) + (world.x - cameraX_) * kTile + shift.x,
                   static_cast<float>(playLayout::mapTop) + (world.y - cameraY_) * kTile + shift.y};
    pose.flip = anim.faceLeft;
    if (now - anim.attackStart < kAttackSeconds) {
        pose.row = kAttackRow;
        pose.frame = std::min(kFrames - 1, static_cast<int>((now - anim.attackStart) / kAttackSeconds * kFrames));
    } else if (now - anim.moveStart < kWalkSeconds) {
        pose.row = kWalkRow;
        pose.frame = static_cast<int>((now - anim.moveStart) * 24.f) % kFrames;
    } else {
        pose.row = kIdleRow;
        pose.frame = static_cast<int>((now + anim.phase) * 6.f) % kFrames;
    }
    return pose;
}

SpriteFrame Application::animatedFrame(const SpriteFrame& base, int row, int frame) {
    const int w = base.rect.size.x, h = base.rect.size.y;
    return {base.sheet, sf::IntRect({frame * w, base.rect.position.y + row * h}, {w, h})};
}

void Application::recordCorpse(const Monster& monster, const SpriteFrame& base, sf::Color tint, int deathRow, int deathFrames, float scale) {
    const auto p = monster.position();
    if (exploredMap_.at(p.x, p.y) != Visibility::Visible || monster.tactics.concealed) return;
    Corpse corpse{base, tint, p, animNow(), false, deathRow, deathFrames, scale};
    if (const auto it = actorAnims_.find(&monster); it != actorAnims_.end()) corpse.faceLeft = it->second.faceLeft;
    corpses_.push_back(corpse);
}

void Application::forgetActor(const Actor& actor) { actorAnims_.erase(&actor); }

// Monsters that just died play their death row, then fade where they fell.
void Application::renderCorpses() {
    const float now = animNow();
    corpses_.erase(std::remove_if(corpses_.begin(), corpses_.end(), [&](const Corpse& c) {
        return now - c.start > kDeathSeconds + kCorpseFadeSeconds;
    }), corpses_.end());
    for (const auto& c : corpses_) {
        const auto at = worldToScreen(c.tile.x, c.tile.y);
        if (!onMap(at)) continue;
        const float t = now - c.start;
        const int frame = std::min(c.deathFrames - 1, static_cast<int>(t / kDeathSeconds * c.deathFrames));
        sf::Color tint = c.tint;
        if (t > kDeathSeconds) tint.a = static_cast<std::uint8_t>(tint.a * std::max(0.f, 1.f - (t - kDeathSeconds) / kCorpseFadeSeconds));
        const float size = kTile * std::max(c.base.rect.size.x, c.base.rect.size.y) / 32.f * c.scale;
        sprites_.draw(window_, animatedFrame(c.base, c.deathRow, frame), {at.x + (kTile - size) / 2, at.y + kTile - size}, size,
                      tint, c.faceLeft);
    }
}

} // namespace engine
