// Standalone sanity check for the Prompt 23 talent-unlock system --
// TalentSet::learnTalent() and PlayerClassFactory::talentUnlockedAtLevel().
// Hand-computed expected results, no SFML, no window, no Application.

#include <iostream>
#include <memory>
#include <string>

#include "ai/NullAIBehavior.hpp"
#include "entities/MageTalents.hpp"
#include "entities/TalentCatalog.hpp"
#include <map>
#include "entities/Monster.hpp"
#include "entities/PlayerClassFactory.hpp"
#include "entities/TalentEffects.hpp"
#include "entities/ThiefTalents.hpp"
#include "entities/WarriorTalents.hpp"

using namespace engine;

namespace {
bool g_allOk = true;

void check(bool condition, const std::string& description) {
    g_allOk &= condition;
    std::cout << (condition ? "[ok] " : "[FAIL] ") << description << '\n';
}

// A damage result is valid if it matches either the normal hit or the
// crit hit -- crit is global (a flat base chance, always active), so an
// exact `==` would be flaky. `critMultiplier` is passed explicitly since
// Piercing Shot's own bonusCritDamageMultiplier makes its crit 2.0x, not
// the usual 1.5x.
bool matchesNormalOrCrit(int actualDamage, int normalDamage, float critMultiplier) {
    const int critDamage = static_cast<int>(static_cast<float>(normalDamage) * critMultiplier);
    return actualDamage == normalDamage || actualDamage == critDamage;
}
} // namespace

int main() {
    // --- TalentSet::learnTalent(): appends without disturbing existing
    // entries or their cooldown tracking.
    {
        TalentSet talents = talentSetForClass(PlayerClass::Warrior);
        check(talents.knownTalents().size() == 4, "Fighter starts with 4 known talents");

        talents.startCooldown(1); // Cleave, index 1
        check(!talents.isReady(1), "Cleave is on cooldown after starting it (sanity check)");

        talents.learnTalent(Talent{/*name=*/"Test Talent"});
        check(talents.knownTalents().size() == 5, "learnTalent() grows the known-talent count to 5");
        check(talents.knownTalents()[4].name == "Test Talent",
              "the newly learned talent lands at the new index (4), not inserted earlier");
        check(!talents.isReady(1), "Cleave's cooldown (index 1) is undisturbed by the new talent");
        check(talents.isReady(4), "the newly learned talent starts with 0 cooldown -- usable immediately");
    }

    // --- Fighter's unlock levels: 4 and 7 only.
    check(!warriorTalentUnlockedAtLevel(1).has_value(), "Warrior has no unlock at level 1");
    check(!warriorTalentUnlockedAtLevel(3).has_value(), "Warrior has no unlock at level 3");
    check(warriorTalentUnlockedAtLevel(4).has_value() &&
              warriorTalentUnlockedAtLevel(4)->name == "Whirlwind",
          "Fighter unlocks Whirlwind at level 4");
    check(!warriorTalentUnlockedAtLevel(5).has_value(), "Warrior has no unlock at level 5");
    check(warriorTalentUnlockedAtLevel(7).has_value() &&
              warriorTalentUnlockedAtLevel(7)->name == "Undying Rage",
          "Fighter unlocks Undying Rage at level 7");
    check(!warriorTalentUnlockedAtLevel(10).has_value(), "Warrior has no unlock at level 10");

    // --- Sorcerer's unlock levels.
    check(mageTalentUnlockedAtLevel(4).has_value() &&
              mageTalentUnlockedAtLevel(4)->name == "Meteor",
          "Sorcerer unlocks Meteor at level 4");
    check(mageTalentUnlockedAtLevel(7).has_value() &&
              mageTalentUnlockedAtLevel(7)->name == "Overload",
          "Sorcerer unlocks Overload at level 7");
    check(!mageTalentUnlockedAtLevel(6).has_value(), "Mage has no unlock at level 6");

    // --- Thief's unlock levels, including Piercing Shot's crit-bonus mechanic.
    check(thiefTalentUnlockedAtLevel(4).has_value() &&
              thiefTalentUnlockedAtLevel(4)->name == "Piercing Shot",
          "Thief unlocks Piercing Shot at level 4");
    check(thiefTalentUnlockedAtLevel(4)->bonusCritChance > 0.199f &&
              thiefTalentUnlockedAtLevel(4)->bonusCritChance < 0.201f &&
              thiefTalentUnlockedAtLevel(4)->bonusCritDamageMultiplier > 0.499f &&
              thiefTalentUnlockedAtLevel(4)->bonusCritDamageMultiplier < 0.501f,
          "Piercing Shot has an inherent +20% crit chance and +50% increased crit damage "
          "-- reworked from its original conditional-multiplier mechanic during the "
          "attribute-system redesign");
    check(thiefTalentUnlockedAtLevel(4)->cooldownTurns == 7,
          "Piercing Shot's cooldown was raised to 7 (from the original 4) to match its "
          "new, stronger always-on mechanic");

    // A real damage-application check, not just the data fields above --
    // this is exactly the kind of discrepancy live testing caught once
    // already (an inline comment claiming the strength bonus was 0 at
    // "every tier," which turned out to be wrong specifically for
    // Piercing Shot's own Signature-tier cooldown of 7): 2/5 truncates
    // to 0 at Filler/Core/Power, but 2/5 * 2.5 (Signature) == 1.0,
    // truncating to a real +1, not 0.
    {
        Stats thiefStats = statsForClass(PlayerClass::Thief);
        Monster attacker(MonsterType::Goblin, "ThiefAttacker", '@', Position{0, 0}, thiefStats,
                          std::make_unique<NullAIBehavior>());
        Stats targetStats;
        targetStats.hp = 90;
        targetStats.maxHp = 90; // dexterity defaults to 0 -- guaranteed to land
        Monster target(MonsterType::Goblin, "PiercingShotTarget", 'p', Position{0, 0},
                        targetStats, std::make_unique<NullAIBehavior>());
        applyTalentDamage(*thiefTalentUnlockedAtLevel(4), attacker, target);
        check(matchesNormalOrCrit(90 - target.stats().hp, 11, 2.0f),
              "Piercing Shot deals base 10 + strength bonus 1 == 11 damage (or 22 on a "
              "crit, using its own 2.0x bonus multiplier, not the global 1.5x) -- the "
              "exact total confirmed live during verification, not just calculated");
    }

    check(thiefTalentUnlockedAtLevel(7).has_value() &&
              thiefTalentUnlockedAtLevel(7)->name == "Adrenaline",
          "Thief unlocks Adrenaline at level 7");

    // --- PlayerClassFactory::talentUnlockedAtLevel() dispatches correctly.
    check(talentUnlockedAtLevel(PlayerClass::Warrior, 4)->name == "Whirlwind",
          "talentUnlockedAtLevel dispatches Fighter correctly");
    check(talentUnlockedAtLevel(PlayerClass::Mage, 4)->name == "Meteor",
          "talentUnlockedAtLevel dispatches Sorcerer correctly");
    check(talentUnlockedAtLevel(PlayerClass::Thief, 4)->name == "Piercing Shot",
          "talentUnlockedAtLevel dispatches Thief correctly");

    // --- Spellblade never unlocks talents this way -- reserved for its
    // own separate mechanism (MetaProgress), not level-gated like the
    // three base classes.
    check(!talentUnlockedAtLevel(PlayerClass::Spellblade, 4).has_value(),
          "Spellblade has no talentUnlockedAtLevel unlock at level 4");
    check(!talentUnlockedAtLevel(PlayerClass::Spellblade, 7).has_value(),
          "Spellblade has no talentUnlockedAtLevel unlock at level 7");

    // --- The node model: the forked pilot trees, and every other tree as before.
    {
        bool othersUnchanged=true, pilotsForked=true;
        for (std::size_t tree=0;tree<kTalentTrees.size();++tree) {
            const std::string id=kTalentTrees[tree].id;
            const auto& nodes=treeNodes(tree);
            if (id=="fire" || id=="one_handed" || id=="arcane" || id=="ice" || id=="lightning" || id=="two_handed" || id=="shadow" || id=="radiance" || id=="shield" || id=="bow" || id=="stealth" || id=="daggers") {
                // root, two actives, two passives, two capstones
                pilotsForked&=nodes.size()==7;
                for (const auto* d:nodes) pilotsForked&=d->maxRank()==(d->ranks[0].passive?1:3) && d->prerequisites.empty()==(d->tier==0 || d->tier==3);
                int forks=0; for (const auto* d:nodes) forks+=!d->fork.empty();
                pilotsForked&=forks==4;
                continue;
            }
            othersUnchanged&=nodes.size()==4;
            for (std::size_t i=0;i<nodes.size();++i) othersUnchanged&=nodes[i]->maxRank()==kMaxTalentRank && nodes[i]->tier==static_cast<int>(i) &&
                nodes[i]->prerequisites.empty() && nodes[i]->fork.empty();
        }
        check(othersUnchanged,"Trees outside the pilot keep four five-rank nodes, one per tier");
        check(pilotsForked,"The forked trees: seven nodes, 3-rank actives, 1-rank passives, two forks");
        check(findTalentDefinition("juggernaut.iron_skin")->maxRank()==1,"Ascendancy nodes have a single rank");
        check(findTalentDefinition("juggernaut.iron_skin")->atRank(3).name=="Iron Skin","Asking past a node's last rank gives its last rank");
    }
    // --- Forks and prerequisites, on a made-up tree.
    {
        const auto node=[](const char* id,int tier,int ranks,const char* fork,std::vector<std::string> needs) {
            Talent t; t.id=id; t.name=id;
            TalentDefinition d; d.id=id; d.treeId="test"; d.tier=tier; d.ranks=std::vector<Talent>(static_cast<std::size_t>(ranks),t);
            d.fork=fork; d.prerequisites=std::move(needs); return d;
        };
        const auto root=node("Root",0,3,"",{}), wall=node("Wall",1,3,"path",{"Root"}), ball=node("Ball",1,3,"path",{"Root"}),
                   haze=node("Haze",2,1,"",{"Wall"}), kindling=node("Kindling",2,1,"",{"Ball"});
        const std::vector<const TalentDefinition*> tree{&root,&wall,&ball,&haze,&kindling};
        std::map<std::string,int> held;
        const auto rankOf=[&](const std::string& id){ const auto it=held.find(id); return it==held.end()?0:it->second; };
        const auto reason=[&](const TalentDefinition& d){ return nodeRequirementReason(d,tree,10,rankOf); };
        check(reason(root).empty() && !reason(wall).empty(),"A fork waits for its root");
        held["Root"]=1;
        check(reason(wall).empty() && reason(ball).empty(),"With the root learned, both sides of a fork are open");
        held["Wall"]=1; held["Root"]=2; // tier 2 also needs 3 points spent below it
        check(reason(ball)=="You took Wall instead.","Taking one side of a fork closes the other");
        check(reason(haze).empty() && reason(kindling)=="Requires Ball.","A passive follows the side you took");
        check(root.maxRank()==3 && haze.maxRank()==1,"Nodes carry their own rank counts");
    }

    std::cout << "\n"
              << (g_allOk ? "All talent-unlock checks passed." : "Some talent-unlock checks FAILED.")
              << '\n';
    return g_allOk ? 0 : 1;
}
