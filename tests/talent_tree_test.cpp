// The talent catalogue and its bookkeeping, checked without a window:
// learning talents, Piercing Shot's critical mechanic, the shape of every
// forked tree, which sound each hit makes, how flat passives grow with their
// attribute, and fork/prerequisite rules on a made-up tree.

#include <iostream>
#include <memory>
#include <string>

#include "ai/NullAIBehavior.hpp"
#include "fixtures/MageTalents.hpp"
#include "entities/TalentCatalog.hpp"
#include "core/CombatSounds.hpp"
#include <map>
#include "entities/Monster.hpp"
#include "entities/PlayerClassFactory.hpp"
#include "fixtures/ClassKits.hpp"
#include "entities/TalentEffects.hpp"
#include "fixtures/ThiefTalents.hpp"
#include "fixtures/WarriorTalents.hpp"

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

    // --- Piercing Shot: a talent with its own critical chance and critical damage.
    check(thiefTalentUnlockedAtLevel(4).has_value() &&
              thiefTalentUnlockedAtLevel(4)->name == "Piercing Shot",
          "Thief unlocks Piercing Shot at level 4");
    check(thiefTalentUnlockedAtLevel(4)->bonusCritChance > 0.199f &&
              thiefTalentUnlockedAtLevel(4)->bonusCritChance < 0.201f &&
              thiefTalentUnlockedAtLevel(4)->bonusCritDamageMultiplier > 0.499f &&
              thiefTalentUnlockedAtLevel(4)->bonusCritDamageMultiplier < 0.501f,
          "Piercing Shot has its own +20% critical chance and +50% critical damage");
    check(thiefTalentUnlockedAtLevel(4)->cooldownTurns == 7,
          "Piercing Shot's cooldown is 7, the Signature tier");

    // The damage it really deals, not just its data. The attribute bonus is
    // scaled by cooldown tier: 2 Strength / 5 truncates to 0 at the Filler,
    // Core and Power tiers, but Signature (cooldown 7) multiplies by 2.5,
    // so 2/5 * 2.5 = 1.0 and the bonus is a real +1.
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

    // --- The node model: the forked pilot trees, and every other tree as before.
    {
        bool othersUnchanged=true, pilotsForked=true;
        for (std::size_t tree=0;tree<kTalentTrees.size();++tree) {
            const std::string id=kTalentTrees[tree].id;
            const auto& nodes=treeNodes(tree);
            if (id=="fire" || id=="one_handed" || id=="arcane" || id=="ice" || id=="lightning" || id=="two_handed" || id=="shadow" || id=="radiance" || id=="shield" || id=="bow" || id=="stealth" || id=="daggers" || id=="earth" || id=="tide" || id=="venom" || id=="spear" || id=="mace" || id=="crossbow" || id=="brawling" || id=="whip" || id=="skirmish" || id=="alchemy" || id=="traps" || id=="hexes" || id=="acrobatics" || id=="cloth" || id=="light_armour" || id=="heavy_armour" || id=="spellblade" || id=="animation" || id=="blood_magic" || id=="shadow_archer" || id=="lamplighter" || id=="stormlance" || id=="hexblade" || id=="saboteur" || id=="stonefist" || id=="warbanner" || id=="forgeborn" || id=="slagcaller" || id=="tempest" || id=="bonewright" || id=="rimeheart" || id=="briarheart" || id=="packmaster" || id=="wintermarch" || id=="gravecold") {
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
    // --- Combat sounds: each hit sounds like what dealt it.
    {
        const auto talent=[](const char* id){ return findTalentDefinition(id)->ranks[0]; };
        check(talentSound(talent("one_handed.quick_strike"),WeaponKind::OneHanded)==HitSound::Slash,"A sword strike slashes");
        check(talentSound(talent("mace.crush"),WeaponKind::Mace)==HitSound::Blunt,"A mace crushes");
        check(talentSound(talent("bow.quick_shot"),WeaponKind::Bow)==HitSound::Pierce,"An arrow pierces");
        check(talentSound(talent("fire.fireball"),WeaponKind::Staff)==HitSound::Fire && talentSound(talent("ice.shard"),WeaponKind::Staff)==HitSound::Frost,
              "Spells sound like their element");
        check(talentSound(basicAttack(),WeaponKind::None)==HitSound::Blunt,"Bare fists thump");
        check(std::string(critFamily(HitSound::Slash,true))=="crit_gore" && std::string(critFamily(HitSound::Slash,false))=="crit_bone",
              "A slashing crit sprays gore from the living and cracks bone from the dead");
        check(std::string(critFamily(HitSound::Fire,true))=="crit_fire" && std::string(critFamily(HitSound::Blunt,true))=="crit_bone",
              "Other crits take their own heavier layer");
        check(monsterSound(MonsterType::Archer,false)==HitSound::Pierce && monsterSound(MonsterType::Ogre,false)==HitSound::Blunt,
              "Monsters sound like what they are");
    }
    // --- Flat passives grow with their attribute; percentages don't.
    {
        TalentSet kit;
        kit.learnTalent(findTalentDefinition("fire.kindling")->ranks[0]);        // +3, Intelligence, 1 per 5
        kit.learnTalent(findTalentDefinition("acrobatics.footwork")->ranks[0]);  // a dodge percentage
        Stats low; low.intelligence=4; low.dexterity=4;
        Stats high; high.intelligence=40; high.dexterity=40;
        check(kit.passiveValue(PassiveKind::Kindling,low)==3 && kit.passiveValue(PassiveKind::Kindling,high)==11,
              "Kindling grows by 1 for every 5 Intelligence");
        check(kit.passiveValue(PassiveKind::Footwork,low)==kit.passiveValue(PassiveKind::Footwork,high),"Percentages don't grow");
        TalentSet resonance;
        resonance.learnTalent(findTalentDefinition("resonance.hallowed_guard")->ranks[0]);
        Stats brawny; brawny.strength=30;
        check(resonance.passiveValue(PassiveKind::HallowedGuard,brawny)==3+6,"Resonances grow with your highest attribute");
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
              << (g_allOk ? "All talent tree checks passed." : "Some talent tree checks FAILED.")
              << '\n';
    return g_allOk ? 0 : 1;
}
