#pragma once
#include "entities/Player.hpp"
#include <initializer_list>
#include <string>
namespace engine {
inline bool specialized(const Player& p,const char* id) {
    for (const auto& t:p.trees()) if (t.id==id) return t.specialized;
    return false;
}
inline constexpr const char* kHiddenIds[]{"spellblade","animation","shadow_archer","blood_magic"};
inline constexpr const char* kHiddenNames[]{"Spellblade","Animation","Shadow Archer","Blood Magic"};
// Ranks put into a tree (each rank of each ability counts one).
inline int ranksInvested(const Player& p,const std::string& tree) {
    int count=0;
    for (std::size_t i=0;i<p.talents().knownTalents().size();++i) {
        const auto* d=findTalentDefinition(p.talents().knownTalents()[i].id);
        if (d && !isImbueVariant(d->id) && d->treeId==tree) count+=p.talents().rank(i);
    }
    return count;
}
// The hybrid trees open once you have this many ranks in each parent.
inline constexpr int kHybridParentRanks=5;
inline bool investedInAny(const Player& p,std::initializer_list<const char*> trees,const char* except="") {
    for (const auto* t:trees) if (std::string(t)!=except && ranksInvested(p,t)>=kHybridParentRanks) return true;
    return false;
}
#define ENGINE_MELEE_TREES {"one_handed","two_handed","brawling","whip"}
#define ENGINE_MAGIC_TREES {"fire","ice","lightning","arcane","shadow","radiance"}
// What a hybrid tree asks for, shown on the talent screen; empty for other trees.
inline std::string hybridRequirement(const std::string& id) {
    if (id=="spellblade") return "Opens with 5 ranks in a melee tree (One-Handed, Two-Handed, Brawling or Whip) and 5 in a magic tree.";
    if (id=="shadow_archer") return "Opens with 5 ranks in Bow and 5 in Stealth.";
    if (id=="animation") return "Opens with 5 ranks in Shadow and 5 in another magic tree.";
    if (id=="lamplighter") return "Opens with 5 ranks in Radiance and 5 in Fire.";
    if (id=="stormlance") return "Opens with 5 ranks in Spear and 5 in Lightning.";
    if (id=="hexblade") return "Opens with 5 ranks in One-Handed and 5 in Hexes.";
    if (id=="saboteur") return "Opens with 5 ranks in Stealth and 5 in Alchemy.";
    if (id=="stonefist") return "Opens with 5 ranks in Brawling and 5 in Earth.";
    return {};
}
// Hidden trees open by special means. Blood Magic: an offering at the Blood
// Altar. Spellblade, Shadow Archer and Animation: ranks in both parent trees.
// Every other tree is always available.
inline bool hiddenTreeAvailable(const Player& p,const std::string& id) {
    if (id=="blood_magic") return p.bloodMagicUnlocked;
    if (id=="spellblade") return investedInAny(p,ENGINE_MELEE_TREES) && investedInAny(p,ENGINE_MAGIC_TREES);
    if (id=="shadow_archer") return ranksInvested(p,"bow")>=kHybridParentRanks && ranksInvested(p,"stealth")>=kHybridParentRanks;
    if (id=="animation") return ranksInvested(p,"shadow")>=kHybridParentRanks && investedInAny(p,ENGINE_MAGIC_TREES,"shadow");
    const auto both=[&](const char* a,const char* b){ return ranksInvested(p,a)>=kHybridParentRanks && ranksInvested(p,b)>=kHybridParentRanks; };
    if (id=="lamplighter") return both("radiance","fire");
    if (id=="stormlance") return both("spear","lightning");
    if (id=="hexblade") return both("one_handed","hexes");
    if (id=="saboteur") return both("stealth","alchemy");
    if (id=="stonefist") return both("brawling","earth");
    for (const auto* hidden:kHiddenIds) if (id==hidden) return false;
    return true;
}
#undef ENGINE_MELEE_TREES
#undef ENGINE_MAGIC_TREES
inline bool hiddenTree(const std::string& id) {
    for (const auto* hidden:kHiddenIds) if (id==hidden) return true;
    return false;
}
inline void synchronizeImbues(Player& p) {
    const int rank=p.talents().rankOf("spellblade.imbue");
    if (!rank) return;
    const char* parents[]{"fire","ice","lightning","arcane"};
    const char* ids[]{"spellblade.flame","spellblade.frost","spellblade.storm","spellblade.arcane"};
    int cooldown=0;
    for (std::size_t i=0;i<p.talents().knownTalents().size();++i)
        if (p.talents().knownTalents()[i].id=="spellblade.imbue") cooldown=p.talents().cooldownRemaining(i);
    for (auto& id:p.talents().hotbar()) if (id=="spellblade.imbue") id.clear();
    for (int element=0;element<4;++element) {
        bool owned=false; for (const auto& access:p.trees()) if (access.id==parents[element]) owned=true;
        if (!owned) continue;
        if (!p.talents().rankOf(ids[element])) p.talents().learnTalent(findTalentDefinition(ids[element])->atRank(rank));
        for (std::size_t i=0;i<p.talents().knownTalents().size();++i) if (p.talents().knownTalents()[i].id==ids[element]) {
            p.talents().setRank(i,rank); p.talents().setCooldownRemaining(i,cooldown);
        }
    }
}
}
