#pragma once
#include "entities/Player.hpp"
#include <functional>
#include <initializer_list>
#include <optional>
#include <vector>
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
// Points of a colour: one per rank bought in a node of that colour.
inline int affinityPoints(const std::function<int(const std::string&)>& rankOf,Affinity a) {
    int points=0;
    for (const auto& d:talentCatalog()) if (d.affinity==a) points+=rankOf(d.id);
    return points;
}
inline int affinityPoints(const Player& p,Affinity a) {
    return affinityPoints([&](const std::string& id){ return p.talents().rankOf(id); },a);
}
// The hybrids open on colours held, from any tree: the same measure that
// wakes a resonance, set higher. Animation asks for Dark and any other
// school of magic; Saboteur's parents are both Guile.
inline constexpr int kHybridColourPoints=6;
struct HybridGate { Affinity first, second; int points; std::vector<Affinity> anyOf; };
inline std::optional<HybridGate> hybridGate(const std::string& id) {
    constexpr int n=kHybridColourPoints;
    if (id=="spellblade") return HybridGate{Affinity::Steel,Affinity::Arcane,n,{}};
    if (id=="shadow_archer") return HybridGate{Affinity::Hunt,Affinity::Guile,n,{}};
    if (id=="animation") return HybridGate{Affinity::Dark,Affinity::None,n,{Affinity::Flame,Affinity::Frost,Affinity::Storm,Affinity::Arcane,Affinity::Light}};
    if (id=="lamplighter") return HybridGate{Affinity::Light,Affinity::Flame,n,{}};
    if (id=="stormlance") return HybridGate{Affinity::Steel,Affinity::Storm,n,{}};
    if (id=="hexblade") return HybridGate{Affinity::Steel,Affinity::Dark,n,{}};
    if (id=="saboteur") return HybridGate{Affinity::Guile,Affinity::None,n*2,{}};
    if (id=="stonefist") return HybridGate{Affinity::Motion,Affinity::Earth,n,{}};
    return std::nullopt;
}
// The strongest of a gate's "any other school" colours.
inline Affinity bestOf(const Player& p,const std::vector<Affinity>& colours) {
    Affinity best=colours.empty()?Affinity::None:colours.front();
    for (const auto c:colours) if (affinityPoints(p,c)>affinityPoints(p,best)) best=c;
    return best;
}
inline Affinity gateSecond(const Player& p,const HybridGate& g) { return g.anyOf.empty()?g.second:bestOf(p,g.anyOf); }
inline bool hybridGateMet(const Player& p,const HybridGate& g) {
    if (affinityPoints(p,g.first)<g.points) return false;
    const Affinity second=gateSecond(p,g);
    return second==Affinity::None || affinityPoints(p,second)>=g.points;
}
// Seen as a silhouette once you hold either colour; named once you hold both.
inline bool hybridGlimpsed(const Player& p,const std::string& id) {
    const auto g=hybridGate(id);
    return g && (affinityPoints(p,g->first)>0 || (gateSecond(p,*g)!=Affinity::None && affinityPoints(p,gateSecond(p,*g))>0));
}
inline bool hybridNamed(const Player& p,const std::string& id) {
    const auto g=hybridGate(id);
    if (!g) return true;
    const Affinity second=gateSecond(p,*g);
    return affinityPoints(p,g->first)>0 && (second==Affinity::None || affinityPoints(p,second)>0);
}
// What a hybrid asks for, with how far you are; empty for other trees.
inline std::string hybridRequirement(const Player& p,const std::string& id) {
    const auto g=hybridGate(id);
    if (!g) return {};
    const auto part=[&](Affinity a) { return std::string(affinityInfo(a).name)+" "+std::to_string(std::min(affinityPoints(p,a),g->points))+"/"+std::to_string(g->points); };
    std::string text="Opens with "+part(g->first);
    if (!g->anyOf.empty()) {
        text+=" and 6 in another school of magic (";
        for (std::size_t i=0;i<g->anyOf.size();++i) text+=(i?", ":"")+std::string(affinityInfo(g->anyOf[i]).name);
        text+="): "+part(bestOf(p,g->anyOf));
    } else if (g->second!=Affinity::None) text+=" and "+part(g->second);
    return text+".";
}
// Deep trees: unseen until their lore is found, then open on colours and a level.
struct DeepGate { std::vector<std::pair<Affinity,int>> needs; int level; const char* lore; const char* loreName; };
inline std::optional<DeepGate> deepGate(const std::string& id) {
    if (id=="warbanner") return DeepGate{{{Affinity::Steel,8},{Affinity::Guard,6}},10,"warlord_standard","the Warlord's Standard"};
    if (id=="forgeborn") return DeepGate{{{Affinity::Steel,8},{Affinity::Flame,6}},10,"forgemaster_brand","the Forgemaster's Brand"};
    return std::nullopt;
}
inline bool deepTreeKnown(const Player& p,const std::string& id) {
    const auto g=deepGate(id);
    return g && p.knowsLore(g->lore);
}
inline std::string deepRequirement(const Player& p,const std::string& id) {
    const auto g=deepGate(id);
    if (!g) return {};
    if (!p.knowsLore(g->lore)) return "Something here is still hidden from you.";
    std::string text="Opens with ";
    for (std::size_t i=0;i<g->needs.size();++i) {
        const auto [colour,points]=g->needs[i];
        text+=(i?", ":"")+std::string(affinityInfo(colour).name)+" "+std::to_string(std::min(affinityPoints(p,colour),points))+"/"+std::to_string(points);
    }
    return text+" and level "+std::to_string(g->level)+" (you are "+std::to_string(p.level())+").";
}
// Hidden trees open by special means. Blood Magic: an offering at the Blood
// Altar. The hybrids: their colours. Deep trees: their lore, colours and a
// level. Every other tree is always available.
inline bool hiddenTreeAvailable(const Player& p,const std::string& id) {
    if (id=="blood_magic") return p.bloodMagicUnlocked;
    if (const auto g=hybridGate(id)) return hybridGateMet(p,*g);
    if (const auto g=deepGate(id)) {
        if (!p.knowsLore(g->lore) || p.level()<g->level) return false;
        for (const auto& [colour,points]:g->needs) if (affinityPoints(p,colour)<points) return false;
        return true;
    }
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
        if (!p.talents().rankOf(ids[element])) p.talents().learnTalent(findTalentDefinition(ids[element])->atRank(rank));
        for (std::size_t i=0;i<p.talents().knownTalents().size();++i) if (p.talents().knownTalents()[i].id==ids[element]) {
            p.talents().setRank(i,rank); p.talents().setCooldownRemaining(i,cooldown);
        }
    }
}
}
