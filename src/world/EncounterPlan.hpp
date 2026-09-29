#pragma once

#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>
#include "entities/MonsterTier.hpp"
#include "entities/MonsterType.hpp"
#include "world/DungeonGenerator.hpp"
#include "world/FloorTheme.hpp"

namespace engine {
struct EncounterSpawn {
    MonsterType type;
    Position position;
    MonsterTier tier=MonsterTier::Base;
};

// Layout-derived choices never consume combat or loot RNG. Packs are placed
// atomically, away from the entrance; rare/unique upgrades share the floor budget.
inline std::vector<EncounterSpawn> planEncounters(const GeneratedDungeon& dungeon, int floor) {
    std::vector<EncounterSpawn> result;
    int budget=23+4*(std::clamp(floor,1,20)-1);
    int elitesLeft=floor>=7?2:floor>=3?1:0;
    bool rarePlaced=false, uniquePlaced=false;
    if (dungeon.hasVault) { budget-=7; elitesLeft=std::max(0,elitesLeft-1); }
    const auto region=floorTheme(floor).region;
    for (std::size_t i=0;i<dungeon.otherRoomCenters.size();++i) {
        const Position center=dungeon.otherRoomCenters[i];
        const bool crypt=region==FloorRegion::Crypts;
        std::vector<MonsterType> pack;
        int cost=0;
        bool dangerous=false;
        if (crypt) {
            switch(i%5) {
                case 0: pack={MonsterType::CryptSentinel,MonsterType::SkeletonArcher,MonsterType::GraveMender}; cost=12; break;
                case 1: pack={MonsterType::SkeletonGuard,MonsterType::CryptShade,MonsterType::Bonecaller}; cost=11; break;
                case 2: pack={MonsterType::Skeleton,MonsterType::FrostAcolyte,MonsterType::SkeletonArcher}; cost=10; break;
                case 3: pack={MonsterType::CryptSentinel,MonsterType::CryptShade,MonsterType::Skeleton}; cost=11; break;
                default: pack={MonsterType::SkeletonGuard,MonsterType::FrostAcolyte,MonsterType::GraveMender}; cost=12; break;
            }
        } else if(floor>=3 && i%5==0) {
            pack={MonsterType::GoblinBulwark,MonsterType::GoblinSlinger,MonsterType::GoblinMedic}; cost=11;
        } else if(floor>=2 && i%5==1) {
            pack={MonsterType::GoblinRaider,MonsterType::GoblinStalker,MonsterType::Shaman}; cost=10;
        } else if(i%5==2 && floor>=2) {
            pack={MonsterType::GoblinBulwark,MonsterType::GoblinRaider,MonsterType::Bomber}; cost=11; dangerous=true;
        } else if(i%5==3 && floor>=3) {
            pack={MonsterType::Ogre,MonsterType::Goblin,MonsterType::GoblinMedic}; cost=11; dangerous=true;
        } else {
            pack={MonsterType::Goblin,floor>=2?MonsterType::GoblinSlinger:MonsterType::Goblin,i%2?MonsterType::Archer:MonsterType::Spider}; cost=7;
        }
        if (cost>budget) continue;
        std::vector<Position> positions;
        for (int y=center.y-1;y<=center.y+1;++y) for (int x=center.x-1;x<=center.x+1;++x) {
            const int dx=x-dungeon.playerStart.x, dy=y-dungeon.playerStart.y;
            if (!dungeon.map.isWalkable(x,y) || dx*dx+dy*dy<=64) continue;
            if (std::any_of(result.begin(),result.end(),[&](const auto& spawn){return spawn.position.x==x && spawn.position.y==y;})) continue;
            positions.push_back({x,y});
        }
        if (positions.size()<pack.size()) continue;
        const Position entry=i?dungeon.otherRoomCenters[i-1]:dungeon.playerStart;
        std::stable_sort(positions.begin(),positions.end(),[&](Position a,Position b) {
            return std::abs(a.x-entry.x)+std::abs(a.y-entry.y)<std::abs(b.x-entry.x)+std::abs(b.y-entry.y);
        });
        MonsterTier tier=MonsterTier::Base;
        // Named encounters recur every five floors, never in the opening pack.
        if (!result.empty() && floor%5==3 && !uniquePlaced && !dangerous && budget>=cost+5) {
            pack.front()=crypt?MonsterType::OssuaryWarden:MonsterType::GoblinCaptain;
            cost+=5; uniquePlaced=true;
        } else if (!result.empty() && !dangerous && floor>=4 && !rarePlaced && budget>=cost+4 &&
            (static_cast<unsigned>(center.x*31+center.y*17+floor*13)%4==0)) {
            tier=MonsterTier::Nightmare; cost+=4; rarePlaced=true;
        } else if (!result.empty() && !dangerous && elitesLeft>0 && budget>=cost+2) {
            tier=MonsterTier::Elite; cost+=2; --elitesLeft;
        }
        result.push_back({pack.front(),positions.front(),tier});
        if (pack.size()==3) result.push_back({pack[1],positions[positions.size()/2],MonsterTier::Base});
        result.push_back({pack.back(),positions.back(),MonsterTier::Base});
        budget-=cost;
    }
    return result;
}
} // namespace engine
