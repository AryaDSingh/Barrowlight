#include "core/Application.hpp"
#include "entities/TalentProgression.hpp"
#include "entities/Stealth.hpp"
#include "entities/ArmourTalents.hpp"
#include "world/FieldOfView.hpp"
#include <algorithm>

namespace engine {
namespace {
constexpr sf::Keyboard::Key bindingKeys[]{sf::Keyboard::Key::Num1,sf::Keyboard::Key::Num2,sf::Keyboard::Key::Num3,
    sf::Keyboard::Key::Num4,sf::Keyboard::Key::Num5,sf::Keyboard::Key::Num6,sf::Keyboard::Key::Num7,sf::Keyboard::Key::Num8,sf::Keyboard::Key::Num9};
const sf::FloatRect playButton{{1100,24},{155,38}}, treeButton{{1100,172},{155,50}},
    abilityButton{{1100,265},{155,50}}, bindButton{{1100,325},{155,38}}, variantButton{{1100,373},{155,38}},
    cancelBindingButton{{1090,419},{150,32}};
const sf::FloatRect previousTreeButton{{32,619},{145,28}}, nextTreeButton{{190,619},{145,28}};
std::vector<std::size_t> visibleTrees(const Player& player) {
    std::vector<std::size_t> result;
    for(std::size_t i=0;i<kTalentTrees.size();++i)
        if(hiddenTreeAvailable(player,kTalentTrees[i].id)) result.push_back(i);
    return result;
}
std::size_t firstTreeRow(const std::vector<std::size_t>& visible,std::size_t selection) {
    const auto row=static_cast<std::size_t>(std::find(visible.begin(),visible.end(),selection)-visible.begin());
    return row>=12?row-11:0;
}
sf::FloatRect bindingRect(std::size_t slot) {
    return {{380+95.f*(slot%9),465+78.f*(slot/9)},{89,65}};
}
}

void Application::handleTreeMouse(const sf::Event& event) {
    if(const auto* move=event.getIf<sf::Event::MouseMoved>()) mousePixel_=move->position;
    if(const auto* wheel=event.getIf<sf::Event::MouseWheelScrolled>(); wheel && !bindingTalent_) {
        if(wheel->position.x<350 && wheel->delta!=0)
            handleTreeKey(wheel->delta>0?sf::Keyboard::Key::Up:sf::Keyboard::Key::Down,false);
    }
    const auto* click=event.getIf<sf::Event::MouseButtonPressed>();
    if(!click || click->button!=sf::Mouse::Button::Left) return;
    const auto p=sf::Vector2f(click->position);
    // Assignment is modal: every other click leaves points and selection alone.
    if(bindingTalent_) {
        if(cancelBindingButton.contains(p)) { handleTreeKey(sf::Keyboard::Key::Escape,false); return; }
        for(std::size_t i=0;i<18;++i) if(bindingRect(i).contains(p)) {
            handleTreeKey(bindingKeys[i%9],i>=9); return;
        }
        return;
    }
    if(previousTreeButton.contains(p)) { handleTreeKey(sf::Keyboard::Key::Up,false); return; }
    if(nextTreeButton.contains(p)) { handleTreeKey(sf::Keyboard::Key::Down,false); return; }
    if(playButton.contains(p)) { closeTalentTrees(); return; }
    if(treeButton.contains(p)) { handleTreeKey(sf::Keyboard::Key::Enter,false); return; }
    if(abilityButton.contains(p)) { handleTreeKey(sf::Keyboard::Key::A,false); return; }
    if(bindButton.contains(p)) { handleTreeKey(sf::Keyboard::Key::B,false); return; }
    if(variantButton.contains(p)) { handleTreeKey(sf::Keyboard::Key::V,false); return; }
    const auto visible=visibleTrees(player_);
    const auto first=firstTreeRow(visible,treeSelection_);
    for(std::size_t row=first;row<visible.size() && row<first+13;++row) {
        if(sf::FloatRect({25,174+30.f*(row-first)},{325,29}).contains(p)) {
            treeSelection_=visible[row]; abilitySelection_=0; imbueSelection_=0; treeFeedback_.clear(); return;
        }
    }
    for(std::size_t i=0;i<4;++i) if(sf::FloatRect({365,262+30.f*i},{720,29}).contains(p)) {
        abilitySelection_=i; imbueSelection_=0; treeFeedback_.clear(); return;
    }
}

void Application::requestHotbar(std::size_t slot) {
    if (const auto index=player_.talents().hotbarIndex(slot)) requestTalent(*index);
}
void Application::openTalentTrees() {
    cancelTargeting(); mousePixel_.reset(); inventoryOpen_=false;
    refreshHiddenDiscoveries();
    if (!hiddenTreeAvailable(player_,kTalentTrees[treeSelection_].id)) treeSelection_=0;
    bindingTalent_=false; treeFeedback_.clear(); mode_=GameMode::AbilityChoice;
}
void Application::closeTalentTrees() {
    if (player_.trees().empty()) { treeFeedback_="Unlock your first tree with the Unlock button or Enter before continuing."; return; }
    if (player_.abilityPoints()==3 && player_.level()==1) { treeFeedback_="Learn at least one ability with the Learn button or A before continuing."; return; }
    progressionReviewPending_=false;
    mode_=GameMode::Playing; resumeLevelUpSequence();
}
void Application::handleTreeKey(sf::Keyboard::Key key, bool shift) {
    if (key==sf::Keyboard::Key::Escape && bindingTalent_) { bindingTalent_=false; treeFeedback_="Hotbar assignment cancelled."; return; }
    if (key==sf::Keyboard::Key::Up || key==sf::Keyboard::Key::Down || key==sf::Keyboard::Key::Left || key==sf::Keyboard::Key::Right || key==sf::Keyboard::Key::A || key==sf::Keyboard::Key::Enter) bindingTalent_=false;
    if (key==sf::Keyboard::Key::F5) { saveGame(); return; }
    if (key==sf::Keyboard::Key::T || key==sf::Keyboard::Key::Escape) { closeTalentTrees(); return; }
    if (key==sf::Keyboard::Key::Up) { do { treeSelection_=(treeSelection_+kTalentTrees.size()-1)%kTalentTrees.size(); } while (!hiddenTreeAvailable(player_,kTalentTrees[treeSelection_].id)); abilitySelection_=0; }
    if (key==sf::Keyboard::Key::Down) { do { treeSelection_=(treeSelection_+1)%kTalentTrees.size(); } while (!hiddenTreeAvailable(player_,kTalentTrees[treeSelection_].id)); abilitySelection_=0; }
    if (key==sf::Keyboard::Key::Left) abilitySelection_=(abilitySelection_+3)%4;
    if (key==sf::Keyboard::Key::Right) abilitySelection_=(abilitySelection_+1)%4;
    const auto& tree=kTalentTrees[treeSelection_];
    const auto& ability=talentCatalog()[treeSelection_*4+abilitySelection_];
    if (key==sf::Keyboard::Key::Enter) {
        treeFeedback_=treePurchaseReason(player_,playerClass_,tree);
        const bool first=player_.trees().empty();
        if (purchaseTree(player_,playerClass_,tree)) {
            treeFeedback_=std::string(tree.name)+" purchased. Select an ability, then click Learn or press A.";
            if (first && tree.starterItem[0]) {
                const char* id=tree.starterItem;
                if (tree.tree==TalentTree::OneHanded) id="training_sword";
                if (tree.tree==TalentTree::TwoHanded) id="training_greatsword";
                if (tree.tree==TalentTree::Bow) id="training_bow";
                if (tree.tree==TalentTree::Shield) id="training_shield";
                player_.inventory().add(std::make_unique<Item>(*findItemDefinition(id),nextItemId_++));
                player_.equip(player_.inventory().items().size()-1);
            }
        }
    }
    if (key==sf::Keyboard::Key::A) {
        treeFeedback_=abilityPurchaseReason(player_,ability);
        if (purchaseAbility(player_,ability)) treeFeedback_=ability.ranks[0].name+" is now rank "+std::to_string(player_.talents().rankOf(ability.id))+". Hotbar and running cooldown preserved.";
    }
    refreshHiddenDiscoveries();
    std::vector<std::string> variants;
    if (ability.id=="spellblade.imbue") for (const auto& t:player_.talents().knownTalents()) if (isImbueVariant(t.id)) variants.push_back(t.id);
    if (key==sf::Keyboard::Key::V && !variants.empty()) imbueSelection_=(imbueSelection_+1)%variants.size();
    const auto binding=variants.empty()?ability.id:variants[imbueSelection_%variants.size()];
    if (!variants.empty() && key==sf::Keyboard::Key::V) treeFeedback_="Selected "+findTalentDefinition(binding)->ranks[0].name+". V: change element; B then 1-9/Shift+1-9: bind.";
    if (key==sf::Keyboard::Key::B) {
        if (!player_.talents().rankOf(ability.id) || ability.ranks[0].passive) { treeFeedback_="Only learned active abilities can be assigned."; return; }
        bindingTalent_=true;
        treeFeedback_="Assign "+findTalentDefinition(binding)->ranks[0].name+": press 1-9 (Shift for page 2). This replaces that slot. Esc cancels.";
    }
    static constexpr sf::Keyboard::Key keys[]{sf::Keyboard::Key::Num1,sf::Keyboard::Key::Num2,sf::Keyboard::Key::Num3,
        sf::Keyboard::Key::Num4,sf::Keyboard::Key::Num5,sf::Keyboard::Key::Num6,sf::Keyboard::Key::Num7,sf::Keyboard::Key::Num8,sf::Keyboard::Key::Num9};
    for (std::size_t i=0;i<9;++i) if (key==keys[i]) {
        if (!bindingTalent_) { treeFeedback_="A learns or ranks up. To change a hotbar binding, press B first, then a number."; return; }
        bindingTalent_=false;
        if (!player_.talents().rankOf(ability.id) || ability.ranks[0].passive) { treeFeedback_="Only learned active abilities can be assigned."; return; }
        player_.talents().bind(binding,i+(shift?9:0));
        treeFeedback_="Assigned to page "+std::string(shift?"2":"1")+", key "+std::to_string(i+1)+". Basic attack is always available by bumping.";
    }
}
void Application::renderTalentTrees() {
    const sf::Color text(220,228,240), accent(110,225,210), dim(155,165,185), selected(255,220,110);
    drawText("TALENT TREES",32,24,27,accent);
    drawText("Level "+std::to_string(player_.level())+"   Tree points: "+std::to_string(player_.treePoints())+
        "   Ability points: "+std::to_string(player_.abilityPoints()),32,65,18,selected);
    drawText("Click rows to select. Buttons spend points. Wheel over trees to browse. T/Esc: play",32,98,15,text);
    drawText("B then 1-9: assign hotbar (Shift: page 2)   V: Imbue variant   F5/F9: save/load. Purchases cost no turns.",32,122,14,dim);
    const auto button=[&](const sf::FloatRect& rect,const std::string& label,bool enabled=true) {
        sf::RectangleShape box(rect.size); box.setPosition(rect.position);
        const bool hover=mousePixel_ && rect.contains(sf::Vector2f(*mousePixel_));
        box.setFillColor(enabled?(hover?sf::Color(45,75,83):sf::Color(28,48,60)):sf::Color(26,30,39));
        box.setOutlineThickness(1); box.setOutlineColor(enabled?accent:dim); window_.draw(box);
        float y=rect.position.y+6;
        drawWrapped(label,rect.position.x+7,y,17,enabled?text:dim,rect.position.y+rect.size.y);
    };
    button(playButton,"Continue [T]");
    button(previousTreeButton,"Previous tree"); button(nextTreeButton,"Next tree");
    const auto visible=visibleTrees(player_);
    const auto first=firstTreeRow(visible,treeSelection_);
    for (std::size_t row=first;row<visible.size() && row<first+13;++row) {
        const auto i=visible[row];
        const auto& tree=kTalentTrees[i]; const auto* access=treeAccess(player_,tree.id);
        std::string label=std::string(i==treeSelection_?"> ":"  ")+tree.name;
        label+=access?(access->specialized?" [specialized]":" [open]"):" [locked]";
        sf::RectangleShape rowBox({325,29}); rowBox.setPosition({25,174+30.f*(row-first)});
        rowBox.setFillColor(i==treeSelection_?sf::Color(40,56,68):sf::Color(20,28,38)); window_.draw(rowBox);
        drawText(label,32,178+30.f*(row-first),16,i==treeSelection_?selected:text);
    }
    drawText("Trees "+std::to_string(first+1)+"-"+std::to_string(std::min(first+13,visible.size()))+" / "+std::to_string(visible.size()),32,595,14,dim);
    const auto& tree=kTalentTrees[treeSelection_];
    float y=165;
    drawWrapped(tree.description,370,y,85,accent,220);
    const auto treeReason=treePurchaseReason(player_,playerClass_,tree);
    const auto* access=treeAccess(player_,tree.id);
    button(treeButton,access?"Specialize [Enter] 1 tree point":"Unlock [Enter] 1 tree point",treeReason.empty());
    drawWrapped(treeReason.empty()?"Enter: spend 1 tree point to unlock or specialize.":treeReason,370,y,85,dim,265);
    for (std::size_t i=0;i<4;++i) {
        const auto& d=talentCatalog()[treeSelection_*4+i];
        sf::RectangleShape rowBox({720,29}); rowBox.setPosition({365,262+30.f*i});
        rowBox.setFillColor(i==abilitySelection_?sf::Color(40,56,68):sf::Color(20,28,38)); window_.draw(rowBox);
        drawText(std::string(i==abilitySelection_?"> ":"  ")+d.ranks[0].name+
            "  "+std::to_string(player_.talents().rankOf(d.id))+"/3"+(d.ranks[0].passive?"  PASSIVE":"")+(i==3?"  ADVANCED":""),
            370,265+30.f*i,18,i==abilitySelection_?selected:text);
    }
    const auto& d=talentCatalog()[treeSelection_*4+abilitySelection_];
    if (d.ranks[0].armourRequirement!=ArmourRequirement::None) {
        const bool active=armourMatches(player_,d.ranks[0].armourRequirement);
        drawText(std::string(active?"EQUIPMENT MATCHES: ":"INACTIVE: ")+armourRequirementText(d.ranks[0].armourRequirement),370,383,13,active?accent:selected);
    }
    y=403; drawWrapped(d.ranks[0].description,370,y,85,text,455);
    const char* headings[]{"Rank","Damage scale","Mana","Cooldown","Move","Passive","Extra"};
    const float columns[]{380,460,620,710,820,905,1010};
    for(int column=0;column<7;++column) drawText(headings[column],columns[column],460,15,accent);
    for (int r=0;r<3;++r) {
        const auto& t=d.ranks[r];
        drawText(std::to_string(r+1),380,486+24.f*r,15,text);
        drawText((t.passive || t.effectKind!=TalentEffectKind::Damage || t.shape==EffectShape::Movement)?"-":std::to_string(t.damagePercent)+"%",460,486+24.f*r,15,text);
        drawText(std::to_string(t.manaCost),620,486+24.f*r,15,text);
        drawText(std::to_string(t.cooldownTurns),710,486+24.f*r,15,text);
        drawText(std::to_string(t.moveDistance),820,486+24.f*r,15,text);
        drawText(t.passive?std::to_string(t.passiveMagnitude):"-",905,486+24.f*r,15,text);
        drawText(t.restoreMana ? "+"+std::to_string(t.restoreMana)+" MP" : t.restoreHpPercent ? std::to_string(t.restoreHpPercent)+"% HP" : t.selfBuffEffect && t.selfBuffEffect->type==StatusEffectType::Concealed ?
            std::to_string(t.selfBuffEffect->magnitude) : "-",1010,486+24.f*r,15,text);
    }
    y=565;
    const auto reason=abilityPurchaseReason(player_,d);
    const int rank=player_.talents().rankOf(d.id);
    button(abilityButton,rank>=3?"Maximum rank":rank?"Rank up [A] 1 ability point":"Learn [A] 1 ability point",reason.empty());
    button(bindButton,"Assign hotbar [B]",rank>0 && !d.ranks[0].passive);
    if(d.id=="spellblade.imbue") button(variantButton,"Next element [V]",rank>0);
    drawWrapped(reason.empty()?"A: spend 1 ability point. Ranks preserve the original damage-scaling tier.":reason,370,y,85,selected,610);
    drawWrapped("Listed ranks are base values. Owned passives and equipped attributes modify the live ability preview. Tree investment: "+std::to_string(treeInvestment(player_,tree.id)),370,y,85,dim,648);
    if(bindingTalent_) {
        sf::RectangleShape panel({890,253}); panel.setPosition({365,395});
        panel.setFillColor(sf::Color(12,21,34)); panel.setOutlineThickness(1); panel.setOutlineColor(accent); window_.draw(panel);
        drawText("Choose a slot to replace (page 1 above, page 2 below)",380,423,16,accent);
        button(cancelBindingButton,"Cancel [Esc]");
        const auto& bindings=player_.talents().hotbar();
        for(std::size_t slot=0;slot<18;++slot) {
            const auto rect=bindingRect(slot);
            sf::RectangleShape box(rect.size); box.setPosition(rect.position);
            const bool hover=mousePixel_ && rect.contains(sf::Vector2f(*mousePixel_));
            box.setFillColor(hover?sf::Color(45,75,83):sf::Color(28,48,60)); window_.draw(box);
            drawText(std::string(slot<9?"1 / ":"2 / ")+std::to_string(slot%9+1),rect.position.x+5,rect.position.y+3,13,selected);
            const auto* bound=slot<bindings.size()?findTalentDefinition(bindings[slot]):nullptr;
            std::string name=bound?bound->ranks[0].name:slot<bindings.size() && !bindings[slot].empty()?bindings[slot]:"Empty";
            if(!bound && slot<bindings.size()) for(const auto& known:player_.talents().knownTalents())
                if(known.id==bindings[slot]) { name=known.name; break; }
            if(name.size()>22) name=name.substr(0,19)+"...";
            float labelY=rect.position.y+23;
            drawWrapped(name,rect.position.x+5,labelY,9,text,rect.position.y+64);
        }
    }
    y=655; drawWrapped(treeFeedback_,32,y,135,selected,710);
}

float Application::enemyStealthDetectionChance(const Actor& enemy) const {
    if (!player_.statusEffects().has(StatusEffectType::Concealed) || enemy.stats().hp<=0) return 0.f;
    const auto target=player_.position();
    float chance=stealthDetectionChance(player_.stats().dexterity,enemy.stats().dexterity,
        player_.statusEffects().magnitudeOf(StatusEffectType::Concealed),target,enemy.position());
    if (chance==0.f) return 0.f;
    const auto visible=computeFieldOfView(map_,enemy.position(),kStealthDetectionRadius);
    if (std::none_of(visible.begin(),visible.end(),[&](Position p){return p.x==target.x && p.y==target.y;})) return 0.f;
    if (enemy.statusEffects().has(StatusEffectType::HuntersMark)) chance=std::max(.05f,chance*.5f);
    return chance;
}

void Application::applyMovementTalents(Position previous) {
    const auto now=player_.position();
    if (now.x==previous.x && now.y==previous.y) return;
    player_.statusEffects().apply({StatusEffectType::Opening,2,0});
    const int evasion=player_.talents().passiveValue(PassiveKind::Footwork);
    if (evasion) player_.statusEffects().apply({StatusEffectType::Evasion,1,evasion});
    const int burn=player_.talents().passiveValue(PassiveKind::Kindle);
    if (burn) for (const auto& enemy:monsters_) {
        const auto pos=enemy->position();
        if (!enemy->allied && enemy->stats().hp>0 && exploredMap_.at(pos.x,pos.y)==Visibility::Visible &&
            std::abs(pos.x-now.x)+std::abs(pos.y-now.y)<=1)
        {
            enemy->statusEffects().apply({StatusEffectType::Burn,2,burn});
            player_.statusEffects().remove(StatusEffectType::Concealed);
        }
    }
}
} // namespace engine
