#include "core/Application.hpp"

#include <algorithm>
#include <cmath>

#include "core/PlayLayout.hpp"
#include "world/LineOfFire.hpp"

namespace engine {

// Combat effects. Everything here is cosmetic and time-based, like
// ApplicationAnimation.cpp: spells, arrows and blows leave short-lived
// effects in tile space; status effects (Burn, Shock, Chill, Conceal...)
// are drawn on whoever carries them every frame; telegraphed attacks get
// pulsing ground markings. Nothing here changes game state.

namespace {
constexpr float kTile = static_cast<float>(playLayout::tileSize);
constexpr float kPi = 3.14159265f;

// Element palette.
const sf::Color kFire(255, 140, 50), kIce(130, 210, 255), kLightning(175, 205, 255), kArcane(190, 120, 255),
    kArrow(235, 225, 200), kBlood(225, 45, 55), kPhysical(255, 240, 220), kShadow(150, 120, 210), kHoly(255, 215, 120),
    kHeal(120, 255, 150), kPoison(140, 225, 80);

unsigned mix(unsigned a, unsigned b) {
    unsigned h = a * 2654435761u ^ (b + 0x9e3779b9u + (a << 6) + (a >> 2));
    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
    return h;
}
float rand01(unsigned a, unsigned b) { return static_cast<float>(mix(a, b) % 10000) / 10000.f; }

sf::Color withAlpha(sf::Color c, float a) { c.a = static_cast<std::uint8_t>(std::clamp(a, 0.f, 1.f) * 255.f); return c; }

// A line of given width as two triangles.
void appendLine(sf::VertexArray& va, sf::Vector2f a, sf::Vector2f b, float width, sf::Color c) {
    const sf::Vector2f d = b - a;
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 0.01f) return;
    const sf::Vector2f n{-d.y / len * width / 2, d.x / len * width / 2};
    for (const auto& p : {a + n, b + n, b - n, a + n, b - n, a - n}) va.append(sf::Vertex{p, c});
}
void appendCircle(sf::VertexArray& va, sf::Vector2f c, float r, sf::Color inner, sf::Color outer, int segments = 14) {
    for (int i = 0; i < segments; ++i) {
        const float a0 = 2 * kPi * i / segments, a1 = 2 * kPi * (i + 1) / segments;
        va.append(sf::Vertex{c, inner});
        va.append(sf::Vertex{{c.x + std::cos(a0) * r, c.y + std::sin(a0) * r}, outer});
        va.append(sf::Vertex{{c.x + std::cos(a1) * r, c.y + std::sin(a1) * r}, outer});
    }
}
void appendRing(sf::VertexArray& va, sf::Vector2f c, float r, float width, sf::Color col, float from = 0, float to = 2 * kPi, int segments = 24) {
    for (int i = 0; i < segments; ++i) {
        const float a0 = from + (to - from) * i / segments, a1 = from + (to - from) * (i + 1) / segments;
        const sf::Vector2f o0{std::cos(a0), std::sin(a0)}, o1{std::cos(a1), std::sin(a1)};
        const sf::Vector2f p0 = c + o0 * (r - width / 2), p1 = c + o0 * (r + width / 2), p2 = c + o1 * (r + width / 2), p3 = c + o1 * (r - width / 2);
        for (const auto& p : {p0, p1, p2, p0, p2, p3}) va.append(sf::Vertex{p, col});
    }
}
// A jagged bolt from a to b; `seed` changes the shape.
void appendLightning(sf::VertexArray& va, sf::Vector2f a, sf::Vector2f b, unsigned seed, sf::Color core, sf::Color glow, float scale) {
    const sf::Vector2f d = b - a;
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1.f) return;
    const sf::Vector2f n{-d.y / len, d.x / len};
    const int segments = std::max(3, static_cast<int>(len / 9.f));
    sf::Vector2f prev = a;
    for (int i = 1; i <= segments; ++i) {
        const float t = static_cast<float>(i) / segments;
        const float jitter = i == segments ? 0.f : (rand01(seed, i) - .5f) * 14.f * scale;
        const sf::Vector2f p = a + d * t + n * jitter;
        appendLine(va, prev, p, 5.f * scale, glow);
        appendLine(va, prev, p, 1.8f * scale, core);
        prev = p;
    }
}
} // namespace

sf::Vector2f Application::tileToScreen(sf::Vector2f tile) const {
    const auto shift = cameraShift();
    return {static_cast<float>(playLayout::mapLeft) + (tile.x - cameraX_) * kTile + shift.x,
            static_cast<float>(playLayout::mapTop) + (tile.y - cameraY_) * kTile + shift.y};
}

void Application::spawnVfx(Vfx v, float delay) {
    const float now = animNow();
    v.start = std::max(now, vfxSlot_) + delay;
    if (!v.seed) v.seed = static_cast<unsigned>(vfx_.size() * 7919u + static_cast<unsigned>(now * 1000.f));
    vfx_.push_back(v);
}

// Successive attacks in one burst of turns play one after another.
void Application::nextVfxSlot(float gap) { vfxSlot_ = std::max(animNow(), vfxSlot_) + gap; }

void Application::flashActor(const Actor& actor) { hitFlash_[&actor] = animNow(); }

namespace {
sf::Color talentColor(const Talent& t) {
    if (t.tree == TalentTree::Fire || (t.onHitEffect && t.onHitEffect->type == StatusEffectType::Burn)) return kFire;
    if (t.tree == TalentTree::Ice || (t.onHitEffect && t.onHitEffect->type == StatusEffectType::Chill)) return kIce;
    if (t.tree == TalentTree::Lightning) return kLightning;
    if (t.tree == TalentTree::Arcane || t.tree == TalentTree::Spellblade || t.tree == TalentTree::Cloth) return kArcane;
    if (t.tree == TalentTree::BloodMagic) return kBlood;
    if (t.tree == TalentTree::ShadowArcher || t.tree == TalentTree::Stealth || t.tree == TalentTree::Animation) return kShadow;
    if (t.tree == TalentTree::Bow) return kArrow;
    if (t.tree == TalentTree::Shadow) return sf::Color(110, 70, 170);
    if (t.tree == TalentTree::Earth || t.tree == TalentTree::Stonefist) return sf::Color(165, 125, 80);
    if (t.tree == TalentTree::Lamplighter) return sf::Color(255, 190, 90);
    if (t.tree == TalentTree::Stormlance) return sf::Color(150, 200, 255);
    if (t.tree == TalentTree::Hexblade) return sf::Color(170, 90, 210);
    if (t.tree == TalentTree::Tide) return sf::Color(70, 145, 225);
    if (t.tree == TalentTree::Hexes) return sf::Color(170, 90, 210);
    if (t.tree == TalentTree::Venom) return sf::Color(120, 210, 80);
    if (t.tree == TalentTree::Radiance) return kHoly;
    if (t.splashSurface == 7) return sf::Color(150, 220, 70);
    if (t.splashSurface == 1) return sf::Color(90, 70, 40);
    if (t.effectKind == TalentEffectKind::Heal || t.restoreHpPercent) return kHeal;
    return kPhysical;
}
sf::Color buffColor(StatusEffectType type) {
    switch (type) {
        case StatusEffectType::Guard: return kHoly;
        case StatusEffectType::Concealed: return sf::Color(120, 120, 140);
        case StatusEffectType::Empowered: case StatusEffectType::BattleRhythm: return sf::Color(255, 90, 60);
        case StatusEffectType::Evasion: return sf::Color(140, 230, 220);
        case StatusEffectType::FlameBlade: return kFire;
        case StatusEffectType::FrostBlade: return kIce;
        case StatusEffectType::StormBlade: return kLightning;
        case StatusEffectType::ArcaneBlade: return kArcane;
        case StatusEffectType::BloodPact: return kBlood;
        case StatusEffectType::Frenzy: return kBlood;
        case StatusEffectType::Slowed: return kIce;
        case StatusEffectType::Shaken: return sf::Color(200, 170, 120);
        case StatusEffectType::Steadfast: return sf::Color(214, 178, 110);
        default: return kHoly;
    }
}
} // namespace

// The player's ability, as it resolves.
void Application::spawnTalentVfx(const Talent& talent, Position from, Position cursor, const TalentTarget& target) {
    const sf::Vector2f origin{from.x + .5f, from.y + .5f};
    const sf::Color color = talentColor(talent);
    const auto centre = [](Position p) { return sf::Vector2f{p.x + .5f, p.y + .5f}; };
    const Position impact = !target.path.empty() ? target.path.back() : cursor;

    if (talent.shape == EffectShape::Movement || talent.boneSwap) {
        const sf::Color puff = talent.tree == TalentTree::Arcane ? kArcane : sf::Color(200, 190, 170);
        spawnVfx({Vfx::Kind::Puff, origin, origin, puff, 0, .45f, .6f});
        spawnVfx({Vfx::Kind::Puff, centre(target.destination), centre(target.destination), puff, 0, .45f, .6f}, .05f);
        if (talent.selfBuffEffect) spawnVfx({Vfx::Kind::Sparkle, centre(target.destination), centre(target.destination), buffColor(talent.selfBuffEffect->type), 0, .6f});
        nextVfxSlot(.1f);
        return;
    }
    if (talent.effectKind == TalentEffectKind::SelfBuff || talent.effectKind == TalentEffectKind::Heal) {
        if (talent.summonCount) {
            spawnVfx({Vfx::Kind::Pillar, origin, origin, kShadow, 0, .6f, .8f});
        } else if (talent.targeting == TargetingMode::RangedEnemyInSight) {
            // Curses and marks cast on another tile (Wither, Hunter's Mark).
            spawnVfx({Vfx::Kind::Bolt, origin, centre(cursor), color, 0, .18f});
            spawnVfx({Vfx::Kind::Ring, centre(cursor), centre(cursor), color, 0, .4f, .7f}, .18f);
        } else {
            const sf::Color c = talent.restoreHpPercent ? kHeal : talent.restoreMana ? kIce : talent.cleanse ? kHoly
                              : talent.selfBuffEffect ? buffColor(talent.selfBuffEffect->type) : color;
            if (talent.selfBuffEffect && talent.selfBuffEffect->type == StatusEffectType::Concealed)
                spawnVfx({Vfx::Kind::Smoke, origin, origin, c, 0, .8f, .9f});
            else {
                spawnVfx({Vfx::Kind::Ring, origin, origin, c, 0, .4f, .7f});
                spawnVfx({Vfx::Kind::Sparkle, origin, origin, c, 0, .7f});
            }
        }
        nextVfxSlot(.1f);
        return;
    }

    // Damage.
    float delay = 0;
    if (talent.shape == EffectShape::AreaAroundSelf) {
        spawnVfx({Vfx::Kind::Ring, origin, origin, color, 0, .35f, talent.areaRadius + .5f});
        spawnVfx({Vfx::Kind::Burst, origin, origin, color, 0, .3f, talent.areaRadius + .3f});
    } else if (talent.targeting == TargetingMode::AdjacentEnemy) {
        spawnVfx({Vfx::Kind::Slash, origin, centre(cursor), color, 0, .2f});
        delay = .06f;
    } else {
        const sf::Vector2f end = centre(impact);
        const float dx = end.x - origin.x, dy = end.y - origin.y;
        const float travel = std::clamp(std::sqrt(dx * dx + dy * dy) * .03f, .08f, .22f);
        if (talent.tree == TalentTree::Lightning) {
            spawnVfx({Vfx::Kind::Lightning, origin, end, kLightning, 0, .28f, 1.f});
            delay = .03f;
        } else if (talent.tree == TalentTree::Bow || talent.tree == TalentTree::ShadowArcher || talent.tree == TalentTree::Crossbow) {
            spawnVfx({Vfx::Kind::Arrow, origin, end, color, 0, travel});
            delay = travel;
        } else if (!talent.projectile && talent.tree != TalentTree::Alchemy) {
            // Direct spells (Mind Shatter, Drain Life) strike at once.
            spawnVfx({Vfx::Kind::Pillar, end, end, color, 0, .35f, .5f});
            if (talent.drainPercent) spawnVfx({Vfx::Kind::Bolt, end, origin, kBlood, 0, .25f}, .15f);
            delay = .02f;
        } else {
            spawnVfx({Vfx::Kind::Bolt, origin, end, color, 0, travel});
            delay = travel;
        }
        if (talent.shape == EffectShape::AreaAroundTarget) {
            spawnVfx({Vfx::Kind::Burst, end, end, color, 0, .38f, talent.areaRadius + .4f}, delay);
            spawnVfx({Vfx::Kind::Ring, end, end, color, 0, .4f, talent.areaRadius + .6f}, delay);
        } else {
            spawnVfx({Vfx::Kind::Burst, end, end, color, 0, .22f, .45f}, delay);
        }
        if (target.chainedTarget) {
            const auto p = target.chainedTarget->position();
            spawnVfx({Vfx::Kind::Lightning, end, centre(p), kLightning, 0, .25f, .8f}, delay + .05f);
        }
    }
    nextVfxSlot(delay + .08f);
}

// A monster's blow or shot (or a minion's). `magic` bolts glow violet.
void Application::spawnAttackVfx(const Actor& attacker, const Actor& target, bool magic, bool dodged) {
    if (suppressAttackVfx_) return;
    const auto a = attacker.position(), b = target.position();
    const bool seen = exploredMap_.at(a.x, a.y) == Visibility::Visible || exploredMap_.at(b.x, b.y) == Visibility::Visible;
    if (!seen) return;
    const sf::Vector2f from{a.x + .5f, a.y + .5f}, to{b.x + .5f, b.y + .5f};
    const float dx = to.x - from.x, dy = to.y - from.y, dist = std::sqrt(dx * dx + dy * dy);
    float delay = 0;
    if (dist > 1.6f) {
        const float travel = std::clamp(dist * .03f, .08f, .22f);
        spawnVfx({magic ? Vfx::Kind::Bolt : Vfx::Kind::Arrow, from, to, magic ? kArcane : kArrow, 0, travel});
        delay = travel;
        spawnVfx({Vfx::Kind::Burst, to, to, magic ? kArcane : kArrow, 0, .2f, .4f}, delay);
    } else {
        spawnVfx({Vfx::Kind::Slash, from, to, magic ? kArcane : sf::Color(255, 200, 180), 0, .2f});
        delay = .06f;
    }
    if (dodged) spawnVfx({Vfx::Kind::Puff, to, to, sf::Color(200, 200, 200), 0, .3f, .35f}, delay);
    nextVfxSlot(delay + .1f);
}

// A telegraphed attack going off over its whole area.
void Application::spawnReleaseVfx(const EnemyIntent& intent) {
    const sf::Vector2f at{intent.target.x + .5f, intent.target.y + .5f};
    const sf::Color c = intent.kind == IntentKind::Summon ? kArcane : intent.kind == IntentKind::MagicStrike ? kFire
                      : intent.kind == IntentKind::StunStrike ? kHoly : sf::Color(255, 90, 60);
    if (intent.kind == IntentKind::Summon) {
        spawnVfx({Vfx::Kind::Pillar, at, at, c, 0, .7f, .9f});
    } else {
        spawnVfx({Vfx::Kind::Burst, at, at, c, 0, .45f, intent.radius + .6f});
        spawnVfx({Vfx::Kind::Ring, at, at, c, 0, .5f, intent.radius + .9f});
    }
    nextVfxSlot(.15f);
}

void Application::addVfxLights(std::vector<std::pair<sf::Vector2f, sf::Color>>& lights) {
    const float now = animNow();
    for (const auto& v : vfx_) {
        const float t = (now - v.start) / v.duration;
        if (t < 0 || t > 1) continue;
        sf::Vector2f p = v.to;
        if (v.kind == Vfx::Kind::Bolt || v.kind == Vfx::Kind::Arrow) p = v.from + (v.to - v.from) * t;
        if (v.kind == Vfx::Kind::Slash || v.kind == Vfx::Kind::Arrow) continue;
        // A slightly cooler colour so these never take the torches' flicker.
        lights.push_back({tileToScreen(p), sf::Color(v.color.r * 4 / 5, std::min(199, static_cast<int>(v.color.g)), v.color.b)});
    }
    // Rare and unique drops shed their colour on the floor around them.
    for (const auto& item : groundItems_) {
        const auto p = item->position();
        if (item->rarity() < ItemRarity::Rare || exploredMap_.at(p.x, p.y) != Visibility::Visible) continue;
        lights.push_back({tileToScreen({p.x + .5f, p.y + .5f}),
                          item->rarity() == ItemRarity::Unique ? sf::Color(230, 150, 80) : sf::Color(230, 199, 110)});
    }
    // (Burning creatures, torches, braziers and wisps light through computeLight's sources.)
}

void Application::renderVfx() {
    const float now = animNow();
    vfx_.erase(std::remove_if(vfx_.begin(), vfx_.end(), [&](const Vfx& v) { return now - v.start > v.duration; }), vfx_.end());
    if (vfxSlot_ < now) vfxSlot_ = now;
    sf::VertexArray glow(sf::PrimitiveType::Triangles), smoke(sf::PrimitiveType::Triangles);
    for (const auto& v : vfx_) {
        const float t = (now - v.start) / v.duration;
        if (t < 0) continue;
        const sf::Vector2f a = tileToScreen(v.from), b = tileToScreen(v.to);
        const float fade = 1.f - t;
        switch (v.kind) {
            case Vfx::Kind::Bolt: {
                const sf::Vector2f head = a + (b - a) * t;
                for (int i = 4; i >= 0; --i) {
                    const float back = std::max(0.f, t - i * .06f);
                    const sf::Vector2f p = a + (b - a) * back;
                    appendCircle(glow, p, 7.f - i, withAlpha(v.color, .55f - i * .1f), withAlpha(v.color, 0));
                }
                appendCircle(glow, head, 4.f, withAlpha(sf::Color::White, .9f), withAlpha(v.color, .4f));
                break;
            }
            case Vfx::Kind::Arrow: {
                const sf::Vector2f d = b - a;
                const float len = std::max(1.f, std::sqrt(d.x * d.x + d.y * d.y));
                const sf::Vector2f head = a + d * t, tail = head - d / len * (kTile * .7f);
                appendLine(glow, tail, head, 2.f, withAlpha(v.color, .9f));
                appendLine(glow, tail - d / len * 8.f, tail, 3.f, withAlpha(v.color, .25f));
                break;
            }
            case Vfx::Kind::Lightning: {
                // Re-forks every 50ms and fades out.
                const unsigned frame = static_cast<unsigned>((now - v.start) / .05f);
                appendLightning(glow, a, b, mix(v.seed, frame), withAlpha(sf::Color::White, fade), withAlpha(v.color, .5f * fade), v.radius);
                if (t < .5f) appendLightning(glow, a, b, mix(v.seed + 99, frame), withAlpha(v.color, .6f * fade), withAlpha(v.color, .2f * fade), v.radius * .6f);
                appendCircle(glow, b, 10.f * v.radius, withAlpha(v.color, .5f * fade), withAlpha(v.color, 0));
                break;
            }
            case Vfx::Kind::Slash: {
                // A crescent swept across the target, facing away from the attacker.
                const float base = std::atan2(b.y - a.y, b.x - a.x);
                const float sweep = 2.2f, startA = base - sweep / 2;
                const float head = startA + sweep * std::min(1.f, t * 1.6f);
                const sf::Vector2f c = b - (b - a) * .18f + sf::Vector2f{kTile / 2, kTile / 2} * 0.f;
                appendRing(glow, c, kTile * .42f, 4.f * fade + 1.f, withAlpha(v.color, .85f * fade), startA, head, 10);
                appendRing(glow, c, kTile * .42f, 9.f * fade + 2.f, withAlpha(v.color, .25f * fade), startA, head, 10);
                break;
            }
            case Vfx::Kind::Burst: {
                const float r = v.radius * kTile * (.35f + .65f * std::sqrt(t));
                appendCircle(glow, b, r, withAlpha(sf::Color::White, .5f * fade * fade), withAlpha(v.color, .45f * fade), 22);
                for (int i = 0; i < 10; ++i) {
                    const float ang = rand01(v.seed, i) * 2 * kPi, dist = r * (.5f + .7f * rand01(v.seed, i + 50)) * t;
                    appendCircle(glow, {b.x + std::cos(ang) * dist, b.y + std::sin(ang) * dist}, 2.5f, withAlpha(v.color, fade), withAlpha(v.color, 0), 6);
                }
                break;
            }
            case Vfx::Kind::Ring: {
                const float r = v.radius * kTile * (.2f + .8f * t);
                appendRing(glow, b, r, 3.f + 4.f * fade, withAlpha(v.color, .8f * fade), 0, 2 * kPi, 32);
                break;
            }
            case Vfx::Kind::Puff: {
                for (int i = 0; i < 7; ++i) {
                    const float ang = rand01(v.seed, i) * 2 * kPi, dist = v.radius * kTile * .6f * t * (.5f + rand01(v.seed, i + 9));
                    appendCircle(glow, {b.x + std::cos(ang) * dist, b.y + std::sin(ang) * dist - 6 * t}, 3.f + 4.f * t,
                                 withAlpha(v.color, .5f * fade), withAlpha(v.color, 0), 8);
                }
                break;
            }
            case Vfx::Kind::Smoke: {
                // Dark, drawn normally rather than glowing: concealment.
                for (int i = 0; i < 10; ++i) {
                    const float ang = rand01(v.seed, i) * 2 * kPi, dist = v.radius * kTile * .5f * (.3f + t) * rand01(v.seed, i + 3);
                    appendCircle(smoke, {b.x + std::cos(ang) * dist, b.y + std::sin(ang) * dist - 10 * t}, 5.f + 6.f * t,
                                 sf::Color(150, 150, 170, static_cast<std::uint8_t>(150 * fade)), sf::Color(150, 150, 170, 0), 10);
                }
                break;
            }
            case Vfx::Kind::Pillar: {
                const float w = kTile * v.radius * (1.f - .5f * t);
                const sf::Vector2f top{b.x, b.y - kTile * 2.2f};
                appendLine(glow, top, b, w, withAlpha(v.color, .35f * fade));
                appendLine(glow, top, b, w * .35f, withAlpha(sf::Color::White, .6f * fade));
                appendCircle(glow, b, w, withAlpha(v.color, .5f * fade), withAlpha(v.color, 0), 16);
                break;
            }
            case Vfx::Kind::Sparkle: {
                for (int i = 0; i < 9; ++i) {
                    const float x = (rand01(v.seed, i) - .5f) * kTile * .9f;
                    const float rise = kTile * (.2f + .9f * t) * (.6f + .6f * rand01(v.seed, i + 20));
                    appendCircle(glow, {b.x + x, b.y + kTile * .3f - rise}, 2.2f, withAlpha(sf::Color::White, fade), withAlpha(v.color, 0), 6);
                }
                break;
            }
        }
    }
    window_.draw(smoke);
    window_.draw(glow, sf::BlendAdd);
}

// Status effects worn by actors, drawn every frame after the lighting.
void Application::renderStatusVfx() {
    const float now = animNow();
    sf::VertexArray glow(sf::PrimitiveType::Triangles), shade(sf::PrimitiveType::Triangles);
    const auto draw = [&](const Actor& actor, sf::Vector2f topLeft, unsigned salt) {
        const auto& fx = actor.statusEffects();
        const sf::Vector2f feet{topLeft.x + kTile / 2, topLeft.y + kTile * .9f}, chest{topLeft.x + kTile / 2, topLeft.y + kTile * .55f},
            head{topLeft.x + kTile / 2, topLeft.y + kTile * .05f};
        if (fx.has(StatusEffectType::Guard)) {
            const float pulse = .6f + .4f * std::sin(now * 4.f + salt);
            appendRing(glow, chest, kTile * .55f, 2.f, withAlpha(kHoly, .45f * pulse), kPi * .1f, kPi * .9f, 12);
            appendRing(glow, chest, kTile * .55f, 2.f, withAlpha(kHoly, .45f * pulse), kPi * 1.1f, kPi * 1.9f, 12);
        }
        for (const auto type : {StatusEffectType::Empowered, StatusEffectType::BattleRhythm, StatusEffectType::FlameBlade, StatusEffectType::FrostBlade,
                                StatusEffectType::StormBlade, StatusEffectType::ArcaneBlade, StatusEffectType::BloodPact, StatusEffectType::Evasion})
            if (fx.has(type)) {
                const float pulse = .5f + .5f * std::sin(now * 5.f + salt);
                appendCircle(glow, feet, kTile * .5f, withAlpha(buffColor(type), .25f + .15f * pulse), withAlpha(buffColor(type), 0), 16);
                break;
            }
        if (fx.has(StatusEffectType::Burn)) {
            // Real flames licking at the feet, and sparks rising off the body.
            for (int i = 0; i < 2; ++i) {
                const int flame = (static_cast<int>(now * 9.f) + i + static_cast<int>(salt % 3)) % 3;
                sprites_.draw(window_, {"calciumtrice/tiles/dungeon_tileset_calciumtrice.png", sf::IntRect({224 + flame * 16, 512}, {16, 16})},
                              {topLeft.x + kTile * (i ? .45f : .05f), topLeft.y + kTile * .42f}, kTile * .55f);
            }
            for (int i = 0; i < 6; ++i) {
                const float phase = std::fmod(now * 1.6f + rand01(salt, i), 1.f);
                const float x = (rand01(salt, i + 7) - .5f) * kTile * .7f;
                const sf::Vector2f p{chest.x + x, feet.y - phase * kTile * .9f};
                appendCircle(glow, p, 4.f * (1.f - phase) + 1.f, withAlpha(sf::Color(255, 220, 120), .9f * (1.f - phase)),
                             withAlpha(kFire, 0), 7);
            }
            appendCircle(glow, feet, kTile * .45f, withAlpha(kFire, .3f), withAlpha(kFire, 0), 14);
        }
        if (fx.has(StatusEffectType::Shock)) {
            // Crackling arcs that jump around the body a few times a second.
            const unsigned frame = static_cast<unsigned>(now / .07f);
            if (mix(salt, frame) % 3 != 0)
                for (int i = 0; i < 2; ++i) {
                    const float a0 = rand01(mix(salt, frame), i) * 2 * kPi, a1 = a0 + 1.2f + rand01(mix(salt, frame), i + 5);
                    const sf::Vector2f p0{chest.x + std::cos(a0) * kTile * .45f, chest.y + std::sin(a0) * kTile * .45f};
                    const sf::Vector2f p1{chest.x + std::cos(a1) * kTile * .4f, chest.y + std::sin(a1) * kTile * .4f};
                    appendLightning(glow, p0, p1, mix(salt + i, frame), withAlpha(sf::Color::White, .9f), withAlpha(kLightning, .5f), .45f);
                }
        }
        if (fx.has(StatusEffectType::Chill)) {
            appendCircle(glow, chest, kTile * .55f, withAlpha(kIce, .22f), withAlpha(kIce, 0), 14);
            for (int i = 0; i < 4; ++i) {
                const float phase = std::fmod(now * .7f + rand01(salt, i + 30), 1.f);
                const float x = (rand01(salt, i + 40) - .5f) * kTile * .8f;
                appendCircle(glow, {chest.x + x, head.y + phase * kTile}, 1.6f, withAlpha(sf::Color::White, .8f * (1 - phase)), withAlpha(kIce, 0), 5);
            }
        }
        if (fx.has(StatusEffectType::Poison)) {
            for (int i = 0; i < 4; ++i) {
                const float phase = std::fmod(now * .9f + rand01(salt, i + 60), 1.f);
                const float x = (rand01(salt, i + 70) - .5f) * kTile * .6f;
                appendRing(glow, {chest.x + x, chest.y - phase * kTile * .6f}, 2.5f, 1.2f, withAlpha(kPoison, .8f * (1 - phase)), 0, 2 * kPi, 8);
            }
        }
        if (fx.has(StatusEffectType::Stun)) {
            for (int i = 0; i < 3; ++i) {
                const float ang = now * 4.f + i * 2 * kPi / 3;
                appendCircle(glow, {head.x + std::cos(ang) * kTile * .32f, head.y + std::sin(ang) * kTile * .1f}, 2.5f,
                             withAlpha(sf::Color(255, 240, 140), .95f), withAlpha(kHoly, 0), 6);
            }
        }
        if (fx.has(StatusEffectType::Marked) || fx.has(StatusEffectType::HuntersMark)) {
            const float spin = now * 1.5f;
            for (int i = 0; i < 4; ++i) {
                const float ang = spin + i * kPi / 2;
                const sf::Vector2f dir{std::cos(ang), std::sin(ang)};
                appendLine(glow, chest + dir * (kTile * .38f), chest + dir * (kTile * .56f), 2.f, withAlpha(sf::Color(255, 70, 60), .85f));
            }
            appendRing(glow, chest, kTile * .47f, 1.2f, withAlpha(sf::Color(255, 70, 60), .5f), 0, 2 * kPi, 20);
        }
        if (fx.has(StatusEffectType::Doom) || fx.has(StatusEffectType::ManaDrain) || fx.has(StatusEffectType::Wither)) {
            for (int i = 0; i < 3; ++i) {
                const float ang = now * 2.f + i * 2 * kPi / 3 + salt;
                appendCircle(glow, {chest.x + std::cos(ang) * kTile * .45f, chest.y + std::sin(ang * 1.3f) * kTile * .3f}, 3.f,
                             withAlpha(sf::Color(200, 90, 255), .7f), withAlpha(kArcane, 0), 7);
            }
        }
        if (fx.has(StatusEffectType::Concealed)) {
            // Wisps of smoke curling off whoever is hidden.
            for (int i = 0; i < 5; ++i) {
                const float phase = std::fmod(now * .5f + rand01(salt, i + 80), 1.f);
                const float x = (rand01(salt, i + 90) - .5f) * kTile * .9f + std::sin(now * 2 + i) * 3.f;
                appendCircle(shade, {chest.x + x, feet.y - phase * kTile * 1.1f}, 4.f + 5.f * phase,
                             sf::Color(150, 150, 170, static_cast<std::uint8_t>(110 * (1 - phase))), sf::Color(150, 150, 170, 0), 9);
            }
        }
    };
    const auto pose = [&](const Actor& a) { return actorPose(a).screen; };
    // Torchbearers' torches burn in their hands.
    for (const auto& m : monsters_) {
        const auto p = m->position();
        if (m->type() != MonsterType::Torchbearer || m->stats().hp <= 0 || exploredMap_.at(p.x, p.y) != Visibility::Visible) continue;
        const auto at = pose(*m);
        const int frame = (static_cast<int>(now * 8.f) + p.x) % 3;
        sprites_.draw(window_, {"calciumtrice/tiles/dungeon_tileset_calciumtrice.png", sf::IntRect({176 + frame * 16, 304}, {16, 16})},
                      {at.x + kTile * .45f, at.y - kTile * .15f}, kTile * .6f);
    }
    draw(player_, pose(player_), 1);
    for (const auto& m : monsters_) {
        const auto p = m->position();
        if (m->stats().hp <= 0 || m->tactics.concealed || exploredMap_.at(p.x, p.y) != Visibility::Visible) continue;
        draw(*m, pose(*m), static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(m.get()) >> 4));
    }
    window_.draw(shade);
    window_.draw(glow, sf::BlendAdd);
}

// Loot beams: a column of light over magic and better drops, taller and
// brighter the rarer the item, so a good drop is spotted across a room.
void Application::renderLootBeams() {
    const float now = animNow();
    sf::VertexArray glow(sf::PrimitiveType::Triangles);
    for (const auto& item : groundItems_) {
        const auto rarity = item->rarity();
        if (rarity == ItemRarity::Normal) continue;
        const auto p = item->position();
        if (exploredMap_.at(p.x, p.y) != Visibility::Visible) continue;
        const sf::Color c = rarity == ItemRarity::Unique ? sf::Color(240, 136, 52) : rarity == ItemRarity::Rare ? sf::Color(255, 214, 96)
                                                                                  : sf::Color(120, 160, 255);
        const float height = kTile * (rarity == ItemRarity::Unique ? 4.f : rarity == ItemRarity::Rare ? 3.f : 1.6f);
        const float strength = rarity == ItemRarity::Magic ? .55f : 1.f;
        const float pulse = .8f + .2f * std::sin(now * 3.f + p.x * 1.7f + p.y);
        const sf::Vector2f base = tileToScreen({p.x + .5f, p.y + .75f}), top{base.x, base.y - height};
        // Soft outer column and a bright core, both fading upward.
        const auto column = [&](float width, float alpha) {
            const sf::Color bottom = withAlpha(c, alpha * strength * pulse), fade = withAlpha(c, 0);
            const sf::Vector2f l{width / 2, 0};
            for (const auto& v : {sf::Vertex{base - l, bottom}, sf::Vertex{base + l, bottom}, sf::Vertex{top + l, fade},
                                  sf::Vertex{base - l, bottom}, sf::Vertex{top + l, fade}, sf::Vertex{top - l, fade}}) glow.append(v);
        };
        column(kTile * .75f, .22f);
        column(kTile * .28f, .5f);
        column(3.f, .9f);
        appendCircle(glow, base, kTile * .55f, withAlpha(c, .45f * strength * pulse), withAlpha(c, 0), 18);
        // Motes drifting up the beam.
        for (int i = 0; i < (rarity == ItemRarity::Magic ? 2 : 4); ++i) {
            const float phase = std::fmod(now * .6f + rand01(static_cast<unsigned>(p.x * 31 + p.y), i), 1.f);
            const float x = (rand01(static_cast<unsigned>(p.y * 17 + p.x), i + 7) - .5f) * kTile * .35f;
            appendCircle(glow, {base.x + x, base.y - phase * height}, 1.8f, withAlpha(sf::Color::White, (1 - phase) * strength),
                         withAlpha(c, 0), 6);
        }
        if (rarity == ItemRarity::Unique)
            appendRing(glow, base, kTile * (.35f + .1f * std::sin(now * 2.f)), 2.f, withAlpha(c, .8f), now, now + 4.5f, 20);
    }
    window_.draw(glow, sf::BlendAdd);
}

// A white flash over a sprite that was just hit.
void Application::drawHitFlash(const Actor& actor, const SpriteFrame& frame, sf::Vector2f topLeft, float size, bool flip) {
    const auto it = hitFlash_.find(&actor);
    if (it == hitFlash_.end()) return;
    const float t = (animNow() - it->second) / .16f;
    if (t < 0 || t > 1) return;
    const auto* tex = sprites_.texture(frame.sheet);
    if (!tex) return;
    sf::VertexArray quad(sf::PrimitiveType::Triangles);
    SpriteAtlas::append(quad, frame, topLeft, size, sf::Color(255, 255, 255, static_cast<std::uint8_t>(230 * (1 - t))));
    if (flip) for (std::size_t i = 0; i < quad.getVertexCount(); ++i) quad[i].position.x = 2 * topLeft.x + size - quad[i].position.x;
    sf::RenderStates states(sf::BlendAdd);
    states.texture = tex;
    window_.draw(quad, states);
}

// Telegraphed attacks: pulsing, edged tiles with a countdown badge.
void Application::renderTelegraphs(int viewStartX, int viewStartY, int viewEndX, int viewEndY) {
    const float now = animNow();
    sf::VertexArray fill(sf::PrimitiveType::Triangles), glow(sf::PrimitiveType::Triangles);
    struct Badge { sf::Vector2f at; int turns; };
    std::vector<Badge> badges;
    for (const auto& m : monsters_) {
        if (m->stats().hp <= 0 || !m->intent()) continue;
        const auto& intent = *m->intent();
        const sf::Color c = intent.kind == IntentKind::Summon ? kArcane : intent.radius ? kFire : sf::Color(255, 60, 50);
        const float urgency = intent.playerActionsRemaining <= 1 ? 9.f : 4.f;
        const float pulse = .5f + .5f * std::sin(now * urgency);
        bool anyTile = false;
        for (int y = intent.target.y - intent.radius; y <= intent.target.y + intent.radius; ++y)
            for (int x = intent.target.x - intent.radius; x <= intent.target.x + intent.radius; ++x) {
                if (!intent.contains({x, y}) || !map_.isWalkable(x, y) || !hasLineOfFire(map_, intent.target, {x, y}) ||
                    exploredMap_.at(x, y) != Visibility::Visible || x < viewStartX || x >= viewEndX || y < viewStartY || y >= viewEndY) continue;
                anyTile = true;
                const auto at = worldToScreen(x, y);
                const sf::Color body = withAlpha(c, .22f + .2f * pulse);
                for (const auto& p : {at, sf::Vector2f{at.x + kTile, at.y}, sf::Vector2f{at.x + kTile, at.y + kTile}, at,
                                      sf::Vector2f{at.x + kTile, at.y + kTile}, sf::Vector2f{at.x, at.y + kTile}}) fill.append(sf::Vertex{p, body});
                // Bright edges only where the danger zone ends.
                const auto edge = [&](int nx, int ny) {
                    return !intent.contains({nx, ny}) || !map_.isWalkable(nx, ny) || !hasLineOfFire(map_, intent.target, {nx, ny});
                };
                const sf::Color line = withAlpha(c, .6f + .4f * pulse);
                if (edge(x, y - 1)) appendLine(glow, at, {at.x + kTile, at.y}, 2.f, line);
                if (edge(x, y + 1)) appendLine(glow, {at.x, at.y + kTile}, {at.x + kTile, at.y + kTile}, 2.f, line);
                if (edge(x - 1, y)) appendLine(glow, at, {at.x, at.y + kTile}, 2.f, line);
                if (edge(x + 1, y)) appendLine(glow, {at.x + kTile, at.y}, {at.x + kTile, at.y + kTile}, 2.f, line);
            }
        if (!anyTile) continue;
        const sf::Vector2f centre = tileToScreen({intent.target.x + .5f, intent.target.y + .5f});
        if (intent.kind == IntentKind::Summon) {
            // A slowly turning rune circle where the dead will rise.
            const float spin = now * .8f;
            appendRing(glow, centre, kTile * .45f, 2.f, withAlpha(c, .9f), 0, 2 * kPi, 24);
            for (int i = 0; i < 5; ++i) {
                const float a0 = spin + i * 4 * kPi / 5, a1 = spin + (i + 1) * 4 * kPi / 5;
                appendLine(glow, centre + sf::Vector2f{std::cos(a0), std::sin(a0)} * (kTile * .42f),
                           centre + sf::Vector2f{std::cos(a1), std::sin(a1)} * (kTile * .42f), 1.5f, withAlpha(c, .8f));
            }
        } else {
            // Shockwaves ripple out from the point of impact.
            const float ripple = std::fmod(now * 1.2f, 1.f);
            appendRing(glow, centre, kTile * (.2f + (intent.radius + .5f) * ripple), 2.f, withAlpha(c, .7f * (1 - ripple)), 0, 2 * kPi, 32);
        }
        badges.push_back({worldToScreen(intent.target.x, intent.target.y), intent.playerActionsRemaining});
    }
    window_.draw(fill, sf::BlendAdd);
    window_.draw(glow, sf::BlendAdd);
    for (const auto& b : badges) {
        sf::CircleShape back(7.f);
        back.setPosition({b.at.x + kTile - 15.f, b.at.y + 1.f});
        back.setFillColor(sf::Color(20, 10, 10, 220));
        back.setOutlineThickness(1.f);
        back.setOutlineColor(sf::Color(255, 210, 120));
        window_.draw(back);
        drawText(std::to_string(b.turns), b.at.x + kTile - 11.f, b.at.y + 1.f, 11, sf::Color::White);
    }
}

} // namespace engine
