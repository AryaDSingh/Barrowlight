// Standalone sanity check for Prompt 34's support runes -- Rune.hpp's
// compatibility rules and effective-talent resolution, plus TalentSet's
// one-rune-per-talent attachment bookkeeping. Uses the real class kits
// (not hand-built talents) so a kit change that silently breaks a rune's
// intended target is caught here. Hand-computed expected results, no SFML.

#include <iostream>
#include <string>

#include "entities/PlayerClassFactory.hpp"
#include "entities/Rune.hpp"
#include "entities/TalentSet.hpp"

using namespace engine;

namespace {
bool g_allOk = true;

void check(bool condition, const std::string& description) {
    g_allOk &= condition;
    std::cout << (condition ? "[ok] " : "[FAIL] ") << description << '\n';
}

// A class's full kit through level 10, tags derived exactly as in play.
TalentSet fullKit(PlayerClass cls) {
    TalentSet talents = talentSetForClass(cls);
    for (int level = 1; level <= 10; ++level)
        if (auto unlocked = talentUnlockedAtLevel(cls, level)) talents.learnTalent(*unlocked);
    return talents;
}

const Talent* findKnown(const TalentSet& talents, const std::string& id, std::size_t* index = nullptr) {
    for (std::size_t i = 0; i < talents.knownTalents().size(); ++i)
        if (talents.knownTalents()[i].id == id) {
            if (index) *index = i;
            return &talents.knownTalents()[i];
        }
    return nullptr;
}

bool compatible(const TalentSet& talents, const std::string& talentId, const std::string& runeId) {
    const Talent* talent = findKnown(talents, talentId);
    return talent && runeUnavailableReason(*talent, runeId).empty();
}
} // namespace

int main() {
    const TalentSet warrior = fullKit(PlayerClass::Warrior);
    const TalentSet mage = fullKit(PlayerClass::Mage);
    const TalentSet thief = fullKit(PlayerClass::Thief);

    // --- Compatibility against the real kits.
    check(compatible(mage, "mage.arcane_bolt", "chain"), "Chain fits Arcane Bolt (damaging single-target projectile)");
    check(compatible(thief, "thief.quick_shot", "chain"), "Chain fits Quick Shot");
    check(!compatible(thief, "thief.volley", "chain"), "Chain rejects Volley (projectile, but an area)");
    check(!compatible(warrior, "warrior.slam", "chain"), "Chain rejects Slam (melee, not a projectile)");

    check(compatible(warrior, "warrior.cleave", "widen"), "Widen fits Cleave (damaging area)");
    check(compatible(mage, "mage.arcane_storm", "widen"), "Widen fits Arcane Storm");
    check(!compatible(mage, "mage.arcane_bolt", "widen"), "Widen rejects Arcane Bolt (single target)");
    check(!compatible(warrior, "warrior.rallying_cry", "widen"), "Widen rejects Rallying Cry (not damaging)");

    check(compatible(warrior, "warrior.slam", "venom"), "Venom fits Slam (damaging, no on-hit effect)");
    check(!compatible(mage, "mage.mind_shatter", "venom"), "Venom rejects Mind Shatter (already has an on-hit effect)");
    check(!compatible(mage, "mage.arcane_focus", "venom"), "Venom rejects Arcane Focus (not damaging)");

    check(compatible(mage, "mage.blink", "swift_passage"), "Swift Passage fits Blink (pure movement)");
    check(!compatible(thief, "thief.vault_kick", "swift_passage"),
          "Swift Passage rejects Vault Kick (an attack with a retreat is not pure movement)");

    check(!runeUnavailableReason(*findKnown(mage, "mage.arcane_bolt"), "no_such_rune").empty(),
          "an unknown rune ID is rejected");
    check(!runeUnavailableReason(Talent{"Unnamed"}, "venom").empty(),
          "a talent without a stable ID has no support slot");

    // --- Effective talent values, hand-computed from the base kit numbers.
    {
        const Talent& cleave = *findKnown(warrior, "warrior.cleave"); // mana 4, radius 1
        const Talent widened = resolveEffectiveTalent(cleave, "widen");
        check(widened.areaRadius == 2, "Widen: Cleave radius 1 -> 2");
        check(widened.manaCost == 6, "Widen: Cleave mana 4 -> 6 (+50%, rounded up)");

        const Talent& storm = *findKnown(mage, "mage.arcane_storm"); // mana 7, radius 2
        const Talent widenedStorm = resolveEffectiveTalent(storm, "widen");
        check(widenedStorm.areaRadius == 3 && widenedStorm.manaCost == 11,
              "Widen: Arcane Storm radius 2 -> 3, mana 7 -> 11 (7 + ceil(3.5))");

        Talent cheap = cleave;
        cheap.manaCost = 0;
        check(resolveEffectiveTalent(cheap, "widen").manaCost == 1, "Widen always adds at least 1 mana");

        const Talent venomSlam = resolveEffectiveTalent(*findKnown(warrior, "warrior.slam"), "venom");
        check(venomSlam.damagePercent == 80, "Venom: direct damage drops to 80%");
        check(venomSlam.onHitEffect && venomSlam.onHitEffect->type == StatusEffectType::Poison &&
                  venomSlam.onHitEffect->turnsRemaining == 3 && venomSlam.onHitEffect->magnitude == 2 &&
                  venomSlam.onHitChance == 1.f,
              "Venom: guaranteed poison, 2 damage for 3 turns");

        const Talent& blink = *findKnown(mage, "mage.blink"); // move 3, cooldown 4
        const Talent swift = resolveEffectiveTalent(blink, "swift_passage");
        check(swift.moveDistance == 5 && swift.cooldownTurns == 6, "Swift Passage: Blink 3 -> 5 tiles, cooldown 4 -> 6");
        check(swift.scalingCooldown == 4, "Swift Passage keeps Blink's original damage-scaling cooldown tier");

        check(resolveEffectiveTalent(*findKnown(mage, "mage.arcane_bolt"), "chain").chain,
              "Chain sets the bounce flag on Arcane Bolt");

        const Talent unchanged = resolveEffectiveTalent(*findKnown(mage, "mage.arcane_bolt"), "widen");
        check(unchanged.areaRadius == 0 && unchanged.manaCost == 2,
              "an incompatible rune resolves to the unmodified talent");
    }

    // --- TalentSet attachment bookkeeping.
    {
        TalentSet talents = fullKit(PlayerClass::Mage);
        std::size_t bolt = 0, storm = 0, blink = 0;
        findKnown(talents, "mage.arcane_bolt", &bolt);
        findKnown(talents, "mage.arcane_storm", &storm);
        findKnown(talents, "mage.blink", &blink);
        talents.runes() = {{"chain", 10, ""}, {"widen", 11, ""}, {"venom", 12, ""}};

        check(talents.attachRune(0, bolt), "Chain attaches to Arcane Bolt");
        check(talents.attachedRune(bolt) && talents.attachedRune(bolt)->instanceId == 10,
              "Arcane Bolt reports its attached rune");
        check(talents.effectiveTalent(bolt).chain, "effectiveTalent() applies the attached rune");
        check(!talents.knownTalents()[bolt].chain, "the base talent itself is never mutated");

        check(!talents.attachRune(1, bolt), "Widen cannot attach to Arcane Bolt");
        check(talents.attachedRune(bolt)->instanceId == 10, "a rejected attach leaves the existing rune in place");
        check(!talents.attachRune(0, bolt), "re-attaching the same rune to the same talent is a no-op failure");

        check(talents.attachRune(2, bolt), "Venom replaces Chain on Arcane Bolt");
        check(talents.attachedRune(bolt)->instanceId == 12, "Arcane Bolt now carries Venom");
        check(talents.runes()[0].talentId.empty() && talents.runes().size() == 3,
              "the displaced Chain rune stays owned, back in the rune bag");

        check(talents.attachRune(2, storm), "moving Venom to Arcane Storm succeeds");
        check(!talents.attachedRune(bolt), "moving a rune detaches it from its previous talent");
        check(talents.attachedRune(storm)->instanceId == 12, "Arcane Storm carries Venom");

        check(!talents.removeRune(blink), "removing from a talent with no rune reports failure");
        check(talents.removeRune(storm) && !talents.attachedRune(storm), "removeRune() detaches Venom");
        check(!talents.attachRune(99, bolt) && !talents.attachRune(0, 99), "out-of-range indices are rejected");
    }

    std::cout << "\n" << (g_allOk ? "All rune checks passed." : "Some checks FAILED.") << '\n';
    return g_allOk ? 0 : 1;
}
