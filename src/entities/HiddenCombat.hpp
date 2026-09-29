#pragma once
#include "entities/Actor.hpp"
namespace engine {
inline bool hasMeleeWeapon(const Actor& a) {
    const auto* item=a.inventory().equipped(EquipmentSlot::Weapon);
    return item && (item->definition()->weaponKind==WeaponKind::OneHanded || item->definition()->weaponKind==WeaponKind::TwoHanded);
}
inline bool releasableAilment(StatusEffectType type) {
    return isCleansable(type) || type==StatusEffectType::Shock || type==StatusEffectType::Stun || type==StatusEffectType::Wither || type==StatusEffectType::HuntersMark;
}
inline Talent combatTalent(const Actor& caster,Talent t) {
    if (caster.statusEffects().has(StatusEffectType::BloodPact) && isSpell(t) && t.id!="blood_magic.pact") {
        t.hpCost+=t.manaCost; t.manaCost=0;
    }
    t.committedRhythm=isMeleeAttack(t) && hasMeleeWeapon(caster)?caster.statusEffects().magnitudeOf(StatusEffectType::BattleRhythm):0;
    if (t.returnConcealed && caster.statusEffects().has(StatusEffectType::Concealed)) t.damagePercent=t.damagePercent*150/100;
    return t;
}
inline void applyImbueHit(Actor& attacker,Actor& target) {
    if (!hasMeleeWeapon(attacker)) return;
    const int rank=std::max(1,attacker.talents().rankOf("spellblade.imbue"));
    for (int i=0;i<4;++i) {
        const auto type=static_cast<StatusEffectType>(static_cast<int>(StatusEffectType::FlameBlade)+i);
        if (!attacker.statusEffects().has(type)) continue;
        if (target.stats().hp>0) {
            const StatusEffectType effects[]{StatusEffectType::Burn,StatusEffectType::Chill,StatusEffectType::Shock,StatusEffectType::Marked};
            target.statusEffects().apply({effects[i],3+(rank==3),i==0?2+(rank==3):i==1?20+5*(rank-1):1});
        }
        for (auto& e:attacker.statusEffects().active()) if (e.type==type) --e.magnitude;
        if (attacker.statusEffects().magnitudeOf(type)<=0) attacker.statusEffects().remove(type);
        break;
    }
}
}
