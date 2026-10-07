#pragma once
#include "entities/Player.hpp"
#include "entities/HiddenTrees.hpp"
#include "entities/RunProgression.hpp"
#include "entities/Ascendancy.hpp"

namespace engine {
inline const Player::TreeAccess* treeAccess(const Player& p, const std::string& id) {
    for (const auto& t:p.trees()) if (t.id==id) return &t;
    return nullptr;
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
inline int openTrees(const Player& p,bool utility) {
    int count=0;
    for (const auto& t:p.trees()) count+=utilityTree(t.id)==utility;
    return count;
}
inline std::string treePurchaseReason(const Player& p, PlayerClass cls, const TreeDefinition& t) {
    if (p.sandbox) {
        // Any tree, hidden ones included; it still costs a tree point (a utility tree doesn't).
        if (treeAccess(p,t.id)) return "Already open.";
        return utilityTree(t.id) || p.treePoints()>0 ? std::string{} : std::string("No tree points available.");
    }
    if (!hiddenTreeAvailable(p,t.id)) {
        const auto need=deepGate(t.id) ? deepRequirement(p,t.id) : hybridRequirement(p,t.id);
        return need.empty() ? "This tree is locked for now." : need;
    }
    if (treeAccess(p,t.id)) return "Already open.";
    if (utilityTree(t.id)) {
        if (openTrees(p,true)>=utilityTreeSlots(p.level())) {
            for (int level=p.level()+1;level<=kRunMaxLevel;++level)
                if (utilityTreeSlots(level)>openTrees(p,true)) return "Another utility tree opens at level "+std::to_string(level)+".";
            return "No more utility trees.";
        }
        return {};
    }
    if (p.treePoints()<=0) return "No tree points available.";
    if ((!openTrees(p,false) || p.level()<5) && !startingTreeAllowed(cls,t.tree)) return "Outside your starting class pool. Available at level 5.";
    return {};
}
inline bool purchaseTree(Player& p,PlayerClass cls,const TreeDefinition& t) {
    if (!treePurchaseReason(p,cls,t).empty()) return false;
    p.trees().push_back({t.id,false}); if (!utilityTree(t.id)) --p.treePoints(); synchronizeImbues(p); return true;
}
// The pool a node's ranks come from.
inline int& pointsFor(Player& p,const TalentDefinition& d) { return utilityTree(d.treeId) ? p.utilityPoints() : p.abilityPoints(); }
inline int pointsFor(const Player& p,const TalentDefinition& d) { return utilityTree(d.treeId) ? p.utilityPoints() : p.abilityPoints(); }
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
    if (pointsFor(p,d)<=0) return utilityTree(d.treeId) ? "No utility points available." : "No ability points available.";
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
    --pointsFor(p,d); synchronizeImbues(p);
    if (upgrading) p.talents().hotbar()=bindings;
    return true;
}
// Ascendancies go to what your build became: each asks for colours, not a
// class. Either every listed colour at its own count, or (Elementalist,
// Paragon) enough of a group at one count.
struct AscendancyNeed { std::vector<std::pair<Affinity,int>> all; std::vector<Affinity> anyOf; int anyCount=0, anyPoints=0; };
inline AscendancyNeed ascendancyNeed(const std::string& id) {
    if (id=="juggernaut") return {{{Affinity::Steel,8},{Affinity::Guard,6}},{},0,0};
    if (id=="elementalist") return {{},{Affinity::Flame,Affinity::Frost,Affinity::Storm},2,6};
    if (id=="trickster") return {{{Affinity::Guile,6},{Affinity::Motion,6}},{},0,0};
    if (id=="templar") return {{{Affinity::Light,6},{Affinity::Guard,6}},{},0,0};
    if (id=="shadowcaster") return {{{Affinity::Dark,6},{Affinity::Guile,6}},{},0,0};
    if (id=="duelist") return {{{Affinity::Steel,6},{Affinity::Motion,6}},{},0,0};
    if (id=="forgeknight") return {{{Affinity::Steel,6},{Affinity::Flame,6}},{},0,0};
    if (id=="beastwarden") return {{{Affinity::Hunt,6},{Affinity::Motion,6}},{},0,0};
    if (id=="plaguebringer") return {{{Affinity::Rot,6},{Affinity::Dark,6}},{},0,0};
    std::vector<Affinity> every;
    for (int a=1;a<=static_cast<int>(Affinity::Rot);++a) every.push_back(static_cast<Affinity>(a));
    return {{},every,5,3}; // the Paragon: any five colours
}
inline bool ascendancyQualified(const std::function<int(const std::string&)>& rankOf,const std::string& id) {
    const auto need=ascendancyNeed(id);
    for (const auto& [colour,points]:need.all) if (affinityPoints(rankOf,colour)<points) return false;
    int reached=0;
    for (const auto colour:need.anyOf) reached+=affinityPoints(rankOf,colour)>=need.anyPoints;
    return reached>=need.anyCount;
}
inline bool ascendancyQualified(const Player& p,const std::string& id) {
    return ascendancyQualified([&](const std::string& t){ return p.talents().rankOf(t); },id);
}
inline std::string ascendancyNeedText(const Player& p,const std::string& id) {
    const auto need=ascendancyNeed(id);
    std::string text;
    for (const auto& [colour,points]:need.all)
        text+=(text.empty()?"":", ")+std::string(affinityInfo(colour).name)+" "+std::to_string(std::min(affinityPoints(p,colour),points))+"/"+std::to_string(points);
    if (need.anyCount==5) {
        int reached=0;
        for (const auto colour:need.anyOf) reached+=affinityPoints(p,colour)>=need.anyPoints;
        text="Any five colours at 3: "+std::to_string(std::min(reached,5))+"/5";
    } else if (need.anyCount) {
        text="Two of ";
        for (std::size_t i=0;i<need.anyOf.size();++i) text+=(i?(i+1==need.anyOf.size()?" or ":", "):"")+std::string(affinityInfo(need.anyOf[i]).name);
        text+=" at "+std::to_string(need.anyPoints)+":";
        for (const auto colour:need.anyOf) text+=" "+std::string(affinityInfo(colour).name)+" "+std::to_string(std::min(affinityPoints(p,colour),need.anyPoints));
    }
    return text;
}
} // namespace engine
