#pragma once
#include "entities/Player.hpp"
namespace engine {
inline bool specialized(const Player& p,const char* id) {
    for (const auto& t:p.trees()) if (t.id==id) return t.specialized;
    return false;
}
inline constexpr const char* kHiddenIds[]{"spellblade","animation","shadow_archer","blood_magic"};
inline constexpr const char* kHiddenNames[]{"Spellblade","Animation","Shadow Archer","Blood Magic"};
// Hidden trees can't be unlocked for now: their old discovery path (the
// Codex, relics and specialization combinations) was removed and a new
// unlock is still being designed. Every other tree is always available.
inline bool hiddenTreeAvailable(const Player&,const std::string& id) {
    for (const auto* hidden:kHiddenIds) if (id==hidden) return false;
    return true;
}
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
