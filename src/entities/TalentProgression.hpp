#pragma once
#include "entities/Player.hpp"
#include "entities/HiddenTrees.hpp"

namespace engine {
inline const Player::TreeAccess* treeAccess(const Player& p, const std::string& id) {
    for (const auto& t:p.trees()) if (t.id==id) return &t;
    return nullptr;
}
// Points of a colour: one per rank bought in a node of that colour.
inline int affinityPoints(const std::function<int(const std::string&)>& rankOf,Affinity a) {
    int points=0;
    for (const auto& d:talentCatalog()) if (d.affinity==a) points+=rankOf(d.id);
    return points;
}
inline int affinityPoints(const Player& p,Affinity a) {
    return affinityPoints([&](const std::string& id){ return p.talents().rankOf(id); },a);
}
inline const std::vector<const TalentDefinition*>& resonances() { return treeNodes("resonance"); }
// Awake: both colours held deeply enough to learn it. Glimpsed: you hold some of one.
inline bool resonanceAwake(const Player& p,const TalentDefinition& d) {
    return affinityPoints(p,d.resonance[0])>=kResonancePoints && affinityPoints(p,d.resonance[1])>=kResonancePoints;
}
inline bool resonanceGlimpsed(const Player& p,const TalentDefinition& d) {
    return affinityPoints(p,d.resonance[0])>0 || affinityPoints(p,d.resonance[1])>0;
}
inline int treeInvestment(const Player& p,const std::string& tree) {
    int count=0;
    for (std::size_t i=0;i<p.talents().knownTalents().size();++i) {
        const auto* d=findTalentDefinition(p.talents().knownTalents()[i].id);
        if (d && !isImbueVariant(d->id) && d->treeId==tree) count+=p.talents().rank(i);
    }
    return count;
}
inline std::string treePurchaseReason(const Player& p, PlayerClass cls, const TreeDefinition& t) {
    if (!hiddenTreeAvailable(p,t.id)) { const auto need=hybridRequirement(t.id); return need.empty() ? "This tree is locked for now." : need; }
    if (p.treePoints()<=0) return "No tree points available.";
    if (treeAccess(p,t.id)) return "Already open.";
    if ((p.trees().empty() || p.level()<5) && !startingTreeAllowed(cls,t.tree)) return "Outside your starting class pool. Available at level 5.";
    return {};
}
inline bool purchaseTree(Player& p,PlayerClass cls,const TreeDefinition& t) {
    if (!treePurchaseReason(p,cls,t).empty()) return false;
    p.trees().push_back({t.id,false}); --p.treePoints(); synchronizeImbues(p); return true;
}
inline std::string abilityPurchaseReason(const Player& p,const TalentDefinition& d) {
    if (d.treeId=="resonance") {
        if (p.talents().rankOf(d.id)) return "Maximum rank (1).";
        if (!resonanceAwake(p,d)) return "Not yet.";
        if (p.abilityPoints()<=0) return "No ability points available.";
        return {};
    }
    const auto* access=treeAccess(p,d.treeId);
    if (!access) return "Unlock this tree first.";
    const int rank=p.talents().rankOf(d.id);
    if (rank>=d.maxRank()) return "Maximum rank ("+std::to_string(d.maxRank())+").";
    if (p.abilityPoints()<=0) return "No ability points available.";
    if (!rank) return nodeRequirementReason(d,p.level(),[&](const std::string& id){ return p.talents().rankOf(id); });
    return {};
}
inline bool purchaseAbility(Player& p,const TalentDefinition& d) {
    if (!abilityPurchaseReason(p,d).empty()) return false;
    const bool upgrading=p.talents().rankOf(d.id)>0;
    const auto bindings=p.talents().hotbar();
    if (!upgrading) p.talents().learnTalent(d.ranks[0]);
    else for (std::size_t i=0;i<p.talents().knownTalents().size();++i)
        if (p.talents().knownTalents()[i].id==d.id) p.talents().setRank(i,p.talents().rank(i)+1);
    --p.abilityPoints(); synchronizeImbues(p);
    if (upgrading) p.talents().hotbar()=bindings;
    return true;
}
} // namespace engine
