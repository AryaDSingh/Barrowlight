#include "core/Application.hpp"
#include "entities/Difficulty.hpp"
#include "world/LineOfFire.hpp"
#include <queue>
#include "entities/HiddenTrees.hpp"
#include "entities/HiddenCombat.hpp"
#include "entities/MonsterFactory.hpp"
#include "world/FieldOfView.hpp"
#include "world/Pathfinder.hpp"
#include <algorithm>
#include <cmath>

namespace engine {
void Application::alertEnemyGroup(Monster& source,Position target) {
    // Only direct sightings/damage call this. Recipients never relay an alert.
    if(source.tactics.alert==0 && !source.tactics.concealed && exploredMap_.at(source.position().x,source.position().y)==Visibility::Visible)
        log(source.name()," shouts a warning!");
    for(auto& ally:monsters_) {
        if(ally->allied || ally->stats().hp<=0) continue;
        const int dx=ally->position().x-source.position().x, dy=ally->position().y-source.position().y;
        if(dx*dx+dy*dy>25 || !hasLineOfFire(map_,source.position(),ally->position())) continue;
        ally->tactics.alert=8; ally->tactics.lastKnown=target;
    }
    source.tactics.alert=8; source.tactics.lastKnown=target;
}

bool Application::sensedMonster(const Monster& m) const {
    const auto p=m.position();
    return !m.allied && m.stats().hp>0 && m.tactics.alert>0 && !m.tactics.concealed && m.glimpseTurns>0 &&
        exploredMap_.at(p.x,p.y)!=Visibility::Visible;
}

AIDecision Application::enemyDecision(Monster& m,Actor* opponent) {
    auto& t=m.tactics;
    const auto here=m.position();
    const auto distance=[](Position a,Position b){return std::abs(a.x-b.x)+std::abs(a.y-b.y);};
    const bool visible=exploredMap_.at(here.x,here.y)==Visibility::Visible && !t.concealed;
    const bool boss=m.type()==MonsterType::GoblinWarlord || m.type()==MonsterType::Lich;
    if(opponent) {
        alertEnemyGroup(m,opponent->position());
        if(t.concealed && distance(here,opponent->position())<=3) {
            t.concealed=false;
            if(exploredMap_.at(here.x,here.y)==Visibility::Visible) log(m.name()," emerges from the shadows!");
            return {}; // reveal spends the ambusher's action
        }
    } else if(t.alert>0) --t.alert;
    // Hunters follow your scent: they always know where you are.
    if(!opponent && m.roam==Roam::Hunter) { t.alert=8; t.lastKnown=player_.position(); }
    auto moveToward=[&](Position goal) {
        AIDecision d;
        const auto path=findPath(map_,here,goal);
        if(path && path->size()>1) {
            if(!isOccupied((*path)[1],&m)) { d.type=AIActionType::Move; d.movePosition=(*path)[1]; }
            else if(const auto around=flankStep(m,goal)) { d.type=AIActionType::Move; d.movePosition=*around; }
        }
        return d;
    };
    if(!boss && !t.retreated && m.stats().hp*100<=m.stats().maxHp*30 && t.alert>0) {
        t.retreated=true; t.retreat=3;
        if(visible) log(m.name()," falls back!");
        return {};
    }
    if(t.retreat>0) {
        --t.retreat;
        // Search reachable nearby cover, rather than stepping blindly into a wall.
        std::queue<std::pair<Position,Position>> queue;
        std::vector<Position> seen{here};
        queue.push({here,here});
        Position step=here; int best=-10000;
        while(!queue.empty()) {
            const auto [p,first]=queue.front(); queue.pop();
            const int score=(!hasLineOfFire(map_,t.lastKnown,p)?100:0)+distance(p,t.lastKnown)*3-distance(p,here);
            if(distance(p,t.lastKnown)>distance(here,t.lastKnown) || !hasLineOfFire(map_,t.lastKnown,p))
                if(score>best) { best=score; step=first; }
            for(const Position n: {Position{p.x+1,p.y},Position{p.x-1,p.y},Position{p.x,p.y+1},Position{p.x,p.y-1}}) {
                if(distance(n,here)>4 || !map_.isWalkable(n.x,n.y) || isOccupied(n,&m) ||
                    std::any_of(seen.begin(),seen.end(),[&](Position q){return q.x==n.x && q.y==n.y;})) continue;
                seen.push_back(n); queue.push({n,distance(p,here)==0?n:first});
            }
        }
        if(step.x!=here.x || step.y!=here.y) { AIDecision d; d.type=AIActionType::Move; d.movePosition=step; return d; }
    }
    if(!opponent) {
        if(t.alert>0) return moveToward(t.lastKnown);
        if(m.roam!=Roam::None) return roamStep(m);
        if(boss || m.vaultGuard) return {};
        // Short, deterministic local circuits; never a floor-wide random walk.
        t.patrol=(t.patrol+1)%24;
        if(t.patrol%2) return {};
        const Position offsets[]{{2,0},{0,2},{-2,0},{0,-2}};
        const auto offset=offsets[t.patrol/6];
        Position goal{t.home.x+offset.x,t.home.y+offset.y};
        if(!map_.isWalkable(goal.x,goal.y)) goal=t.home;
        auto d=moveToward(goal);
        if(d.type==AIActionType::Move && distance(d.movePosition,t.home)>4) return AIDecision{};
        return d;
    }
    auto allies=aliveAllies(&m);
    allies.erase(std::remove_if(allies.begin(),allies.end(),[&](Actor* a){return distance(here,a->position())>5 || !hasLineOfFire(map_,here,a->position());}),allies.end());
    if(enemyHealer(m.type()) && t.heals>0 && m.talents().isReady(0)) {
        Actor* wounded=nullptr;
        for(auto* a:allies) if(a->stats().hp*100<a->stats().maxHp*70 &&
            (!wounded || a->stats().hp*wounded->stats().maxHp<wounded->stats().hp*a->stats().maxHp)) wounded=a;
        if(wounded) {
            AIDecision d; d.type=AIActionType::UseAbility; d.target=wounded; d.abilityIndex=0;
            d.healAmount=std::max(2,wounded->stats().maxHp*18/100); return d;
        }
    }
    // Keep support and skirmishers behind the healthiest nearby frontliner.
    Actor* tank=nullptr;
    for(auto* a:allies) if(auto* other=dynamic_cast<Monster*>(a); other && enemyTank(other->type()) && a->stats().hp*2>a->stats().maxHp &&
        (!tank || a->stats().hp>tank->stats().hp)) tank=a;
    if(enemyBackline(m.type()) && tank && distance(here,opponent->position())<=distance(tank->position(),opponent->position())) {
        Position best=here; int score=distance(here,opponent->position());
        for(const Position p:{Position{here.x+1,here.y},Position{here.x-1,here.y},Position{here.x,here.y+1},Position{here.x,here.y-1}})
            if(map_.isWalkable(p.x,p.y) && !isOccupied(p,&m) && distance(p,tank->position())<=4 && distance(p,opponent->position())>score) {
                best=p; score=distance(p,opponent->position());
            }
        if(best.x!=here.x || best.y!=here.y) { AIDecision d; d.type=AIActionType::Move; d.movePosition=best; return d; }
    }
    if(enemyHealer(m.type())) {
        // Empty-handed medics stay with the group, never cast a zero-strength buff.
        return tank && distance(here,tank->position())>2?moveToward(tank->position()):AIDecision{};
    }
    std::stable_sort(allies.begin(),allies.end(),[](Actor* a,Actor* b) {
        const auto* ma=dynamic_cast<const Monster*>(a); const auto* mb=dynamic_cast<const Monster*>(b);
        const bool ta=ma && enemyTank(ma->type()), tb=mb && enemyTank(mb->type());
        return ta!=tb?ta:a->stats().hp>b->stats().hp;
    });
    auto decision=m.ai()->decideAction(m,map_,*opponent,allies);
    // Blocked by an ally: go round it, so a pack spreads out and surrounds you.
    if(decision.type==AIActionType::Move && isOccupied(decision.movePosition,&m))
        if(const auto around=flankStep(m,opponent->position())) decision.movePosition=*around;
    if(decision.type==AIActionType::Wait && enemyBackline(m.type()) && tank && distance(here,tank->position())>2) return moveToward(tank->position());
    return decision;
}

void Application::scaleDungeonMonster(Monster& m,int floor) {
    scaleDeepMonster(m,floor);
    if(m.allied) return;
    const int bonus=dungeonDepthBonus(floor);
    m.stats().maxHp=m.stats().maxHp*(100+8*bonus)/100*kMonsterLifePercent/100;
    m.stats().hp=m.stats().maxHp;
    m.stats().strength+=bonus/2; m.stats().intelligence+=bonus/2;
    m.setXpReward(m.xpReward()*(100+5*bonus)/100);
    m.lastObservedHp=m.stats().hp;
}
void Application::scaleDeepMonster(Monster& m,int floor) {
    if (floor<=10 || m.allied) return;
    const int depth=floor-10;
    m.stats().maxHp+=depth*2; m.stats().hp=m.stats().maxHp;
    m.stats().strength+=depth/3; m.stats().intelligence+=depth/3;
    m.setXpReward(m.xpReward()*2+depth*3);
}
void Application::configureMinion(Monster& m,int rank,int intelligence) {
    m.allied=true; m.summonRank=rank; m.summonIntelligence=intelligence;
    m.setRewardsEligible(false);
    m.stats().maxHp=12+4*rank+intelligence; m.stats().hp=m.stats().maxHp;
    m.stats().strength=2+rank+intelligence/5; m.stats().dexterity=2;
    m.stats().intelligence=0; m.stats().speed=100;
}
// Slag that fights for you (Slagcaller): a slagling, or a slow and tough golem.
void Application::raiseSlag(MonsterType kind,Position at,int turns,bool shard) {
    auto m=createMonster(kind,at);
    configureMinion(*m,1,player_.stats().intelligence);
    if (kind==MonsterType::SlagGolem) { m->stats().maxHp*=2; m->stats().hp=m->stats().maxHp; m->stats().strength+=3; m->stats().speed=70; }
    if (kind==MonsterType::Slagling) m->stats().speed=120;
    m->remainingLife=turns+1; m->shard=shard;
    scheduler_.add(*m); monsters_.push_back(std::move(m));
}
int Application::minionCap() const { return std::clamp(1+player_.stats().intelligence/10,1,5); }
void Application::enforceMinionCap() {
    int count=0; for (const auto& m:monsters_) if (m->allied && m->stats().hp>0 && !m->remainingLife) ++count;
    for (auto& m:monsters_) if (count>minionCap() && m->allied && m->stats().hp>0 && !m->remainingLife) {
        m->stats().hp=0; scheduler_.remove(*m); --count; log("Your oldest skeleton dissolves: minion cap fell.");
    }
    removeDeadMonsters();
}
void Application::dissolveMinions() {
    for (auto& m:monsters_) if (m->allied) { m->stats().hp=0; scheduler_.remove(*m); }
    removeDeadMonsters();
}
void Application::summonMinions(const Talent& t) {
    int permanent=0; bool army=false;
    for (const auto& m:monsters_) if (m->allied && m->stats().hp>0) { if (m->remainingLife) army=true; else ++permanent; }
    const bool slag=t.summonKind>=0;
    if (t.summonDuration && army && !slag) { log("Your existing army prevents another army. Cast spent."); return; }
    int remaining=t.summonDuration?t.summonCount:std::min(t.summonCount,minionCap()-permanent);
    const auto p=player_.position(); int raised=0;
    for (const Position d:std::vector<Position>{{1,0},{0,1},{-1,0},{0,-1}}) {
        const Position pos{p.x+d.x,p.y+d.y};
        if (remaining<=0) break;
        if (!map_.isWalkable(pos.x,pos.y) || isOccupied(pos,nullptr)) continue;
        if (slag) { raiseSlag(static_cast<MonsterType>(t.summonKind),pos,t.summonDuration,false); --remaining; ++raised; continue; }
        auto m=createMonster(MonsterType::Skeleton,pos);
        configureMinion(*m,t.summonRank,player_.stats().intelligence);
        m->remainingLife=t.summonDuration?t.summonDuration+1:0;
        scheduler_.add(*m); monsters_.push_back(std::move(m)); --remaining; ++raised;
    }
    if (slag) log(raised?"The slag rises to fight for you.":"There's no room for the slag to rise.");
    else log("Raised ",raised," skeleton(s). Permanent minion cap: ",minionCap(),".");
}
Actor* Application::nearestOpponent(Actor& actor,bool playerHidden) {
    const auto* monster=dynamic_cast<const Monster*>(&actor);
    const bool puppet=monster && monster->statusEffects().has(StatusEffectType::Puppeted);
    const bool allied=monster && (monster->allied || puppet);
    const auto visible=computeFieldOfView(map_,actor.position(),8);
    Actor* nearest=nullptr; int best=100000;
    auto consider=[&](Actor& target) {
        if (target.stats().hp<=0) return;
        const auto p=target.position();
        if (std::none_of(visible.begin(),visible.end(),[&](Position q){return p.x==q.x && p.y==q.y;})) return;
        if (!canSee(actor,p)) return; // humans need light to see past arm's reach
        const int distance=std::abs(p.x-actor.position().x)+std::abs(p.y-actor.position().y);
        if (distance<best) { best=distance; nearest=&target; }
    };
    if (!allied && !playerHidden) consider(player_);
    // A puppet turns on its own kind; its own kind ignores it.
    for (auto& other:monsters_) if (other.get()!=&actor && (puppet ? !other->allied : other->allied!=allied)) consider(*other);
    return nearest;
}
void Application::actMinion(Monster& minion) {
    if (auto* enemy=nearestOpponent(minion,false)) {
        auto decision=minion.ai()->decideAction(minion,map_,*enemy,{});
        executeAIDecision(minion,decision);
        return;
    }
    const auto pos=minion.position(), player=player_.position();
    if (std::abs(pos.x-player.x)+std::abs(pos.y-player.y)<=1) return;
    const auto path=findPath(map_,pos,player);
    if (path && path->size()>1 && !isOccupied((*path)[1],&minion)) minion.setPosition((*path)[1]);
}
void Application::afterHiddenCast(const Talent& t,bool landed,bool killed,int concealed) {
    if (isMeleeAttack(t) && hasMeleeWeapon(player_)) {
        player_.statusEffects().remove(StatusEffectType::BattleRhythm);
        if (landed && player_.talents().passiveValue(PassiveKind::BattleRhythm,player_.stats())) {
            std::optional<std::size_t> choice; int longest=0;
            for (std::size_t i=0;i<player_.talents().knownTalents().size();++i) {
                const auto candidate=player_.talents().effectiveTalent(i);
                const int cd=player_.talents().cooldownRemaining(i);
                if (candidate.id!=t.id && !candidate.passive && isSpell(candidate) && cd>longest) { longest=cd; choice=i; }
            }
            if (choice) player_.talents().setCooldownRemaining(*choice,longest-1);
        }
    }
    if (isSpell(t) && player_.talents().passiveValue(PassiveKind::BattleRhythm,player_.stats()))
        player_.statusEffects().apply({StatusEffectType::BattleRhythm,10000,player_.talents().passiveValue(PassiveKind::BattleRhythm,player_.stats())});
    const bool offensive=(t.effectKind==TalentEffectKind::Damage && t.shape!=EffectShape::Movement) || t.huntersMark || t.id=="blood_magic.wither";
    const int stay=t.stayHiddenPercent+player_.talents().passiveValue(PassiveKind::LingeringShadow,player_.stats());
    bool remain=concealed && stay && rollChance(stay/100.f);
    if (concealed && player_.talents().passiveValue(PassiveKind::UnseenHand,player_.stats()) && !tileLit(player_.position())) remain=true;
    // Templar's Zeal: every spell guards you for the next response.
    if (isSpell(t) && t.effectKind==TalentEffectKind::Damage)
        if (const int zeal=player_.talents().passiveValue(PassiveKind::Zeal,player_.stats())) player_.statusEffects().apply({StatusEffectType::Guard,1,zeal});
    if (offensive && !remain) player_.statusEffects().remove(StatusEffectType::Concealed);
    if (t.returnConcealed && t.selfBuffEffect) player_.statusEffects().apply(*t.selfBuffEffect);
    if (killed && concealed && player_.talents().passiveValue(PassiveKind::Unseen,player_.stats()) && player_.statusEffects().has(StatusEffectType::UnseenReady)) {
        player_.statusEffects().remove(StatusEffectType::UnseenReady);
        player_.statusEffects().apply({StatusEffectType::Concealed,
            player_.talents().passiveValue(PassiveKind::Unseen,player_.stats()),concealed});
    }
}
} // namespace engine
