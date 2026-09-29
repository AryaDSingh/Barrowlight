#pragma once
#include "entities/Player.hpp"
namespace engine {
inline bool specialized(const Player& p,const char* id) {
    for (const auto& t:p.trees()) if (t.id==id) return t.specialized;
    return false;
}
inline bool hiddenTreeAvailable(const Player& p,const std::string& id) {
    const bool magic=specialized(p,"fire") || specialized(p,"ice") || specialized(p,"lightning") || specialized(p,"arcane");
    if (id=="spellblade") return magic && (specialized(p,"one_handed") || specialized(p,"two_handed") || specialized(p,"shield"));
    if (id=="animation") return p.animationRelic && specialized(p,"arcane");
    if (id=="blood_magic") return p.bloodRelic && magic;
    if (id=="shadow_archer") return specialized(p,"bow") && specialized(p,"stealth");
    return true;
}
inline constexpr const char* kHiddenIds[]{"spellblade","animation","shadow_archer","blood_magic"};
inline constexpr const char* kHiddenNames[]{"Spellblade","Animation","Shadow Archer","Blood Magic"};
inline constexpr const char* kHiddenConditions[]{
    "Specialize One-Handed, Two-Handed or Shield AND Fire, Ice, Lightning or Arcane.",
    "Find the Lich's Ossuary Seal in this run AND specialize Arcane.",
    "Specialize both Bow and Stealth.",
    "Find a Blood Testament in this run (Shamans or Warlord) AND specialize any magic tree."
};
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
        if (!p.talents().rankOf(ids[element])) p.talents().learnTalent(findTalentDefinition(ids[element])->ranks[rank-1]);
        for (std::size_t i=0;i<p.talents().knownTalents().size();++i) if (p.talents().knownTalents()[i].id==ids[element]) {
            p.talents().setRank(i,rank); p.talents().setCooldownRemaining(i,cooldown);
        }
    }
}
}
