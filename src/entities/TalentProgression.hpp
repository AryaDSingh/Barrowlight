#pragma once
#include "entities/Player.hpp"
#include "entities/HiddenTrees.hpp"

namespace engine {
inline const Player::TreeAccess* treeAccess(const Player& p, const std::string& id) {
    for (const auto& t:p.trees()) if (t.id==id) return &t;
    return nullptr;
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
    if (!hiddenTreeAvailable(p,t.id)) return "Hidden tree requirements have not been met by this character.";
    if (p.treePoints()<=0) return "No tree points available.";
    const auto* access=treeAccess(p,t.id);
    if (access) {
        if (access->specialized) return "Already specialized.";
        if (p.level()<5) return "Specialization requires level 5.";
        if (treeInvestment(p,t.id)<4) return "Specialization requires 4 ability points invested here.";
    } else if ((p.trees().empty() || p.level()<5) && !startingTreeAllowed(cls,t.tree)) return "Outside your starting class pool. Available at level 5.";
    return {};
}
inline bool purchaseTree(Player& p,PlayerClass cls,const TreeDefinition& t) {
    if (!treePurchaseReason(p,cls,t).empty()) return false;
    for (auto& access:p.trees()) if (access.id==t.id) { access.specialized=true; --p.treePoints(); synchronizeImbues(p); return true; }
    p.trees().push_back({t.id,false}); --p.treePoints(); synchronizeImbues(p); return true;
}
inline std::string abilityPurchaseReason(const Player& p,const TalentDefinition& d) {
    const auto* access=treeAccess(p,d.treeId);
    if (!access) return "Unlock this tree first.";
    const int rank=p.talents().rankOf(d.id);
    if (rank>=3) return "Maximum rank (3).";
    if (p.abilityPoints()<=0) return "No ability points available.";
    if (!rank) {
        constexpr int levels[]{1,1,4,5}, investments[]{0,1,3,4};
        if (p.level()<levels[d.tier]) return "Requires character level "+std::to_string(levels[d.tier])+".";
        if (treeInvestment(p,d.treeId)<investments[d.tier]) return "Requires "+std::to_string(investments[d.tier])+" ability points invested in this tree.";
        if (d.tier==3 && !access->specialized) return "Requires tree specialization.";
    }
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
