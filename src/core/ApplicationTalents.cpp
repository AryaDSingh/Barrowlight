#include "core/Application.hpp"
#include "core/GameIcons.hpp"
#include "entities/TalentProgression.hpp"
#include "entities/Stealth.hpp"
#include "entities/ArmourTalents.hpp"
#include "world/FieldOfView.hpp"
#include <algorithm>

namespace engine {
namespace {
constexpr sf::Keyboard::Key bindingKeys[]{sf::Keyboard::Key::Num1,sf::Keyboard::Key::Num2,sf::Keyboard::Key::Num3,
    sf::Keyboard::Key::Num4,sf::Keyboard::Key::Num5,sf::Keyboard::Key::Num6,sf::Keyboard::Key::Num7,sf::Keyboard::Key::Num8,sf::Keyboard::Key::Num9};
// ToME-style layout: trees grouped under category headings in three
// columns, details and actions in a framed panel on the right.
const sf::FloatRect playButton{{1100,18},{156,36}};
const sf::FloatRect kDetails{{848,70},{410,636}};
const sf::FloatRect treeButton{{866,566},{184,36}}, abilityButton{{1058,566},{184,36}},
    bindButton{{866,610},{184,36}}, variantButton{{1058,610},{184,36}};
const sf::FloatRect kBindingDialog{{230,246},{820,258}};
const sf::FloatRect cancelBindingButton{{kBindingDialog.position.x+kBindingDialog.size.x-138,kBindingDialog.position.y+16},{120,30}};
constexpr float kTreeColumnX[]{28,300,572};
constexpr float kTreeTop=70, kCategoryHeight=30, kTreeRowHeight=96, kIcon=50, kIconStride=60, kColumnWidth=250;

enum class TreeCategory { Martial, Magic, Utility, Defence, Hybrid };
struct CategoryInfo { const char* name; sf::Color color; };
CategoryInfo categoryInfo(TreeCategory c) {
    switch(c) {
        case TreeCategory::Martial: return {"Martial",sf::Color(222,120,96)};
        case TreeCategory::Magic: return {"Magic",sf::Color(128,156,240)};
        case TreeCategory::Utility: return {"Utility",sf::Color(124,204,144)};
        case TreeCategory::Defence: return {"Defence",sf::Color(208,176,112)};
        case TreeCategory::Hybrid: return {"Hybrid",sf::Color(190,132,222)};
    }
    return {"",ui::kText};
}
TreeCategory treeCategory(const std::string& id) {
    if (id=="one_handed" || id=="two_handed" || id=="bow") return TreeCategory::Martial;
    if (id=="fire" || id=="ice" || id=="lightning" || id=="arcane") return TreeCategory::Magic;
    if (id=="stealth" || id=="acrobatics") return TreeCategory::Utility;
    if (id=="shield" || id=="cloth" || id=="light_armour" || id=="heavy_armour") return TreeCategory::Defence;
    return TreeCategory::Hybrid;
}

// Locked hidden trees stay off the screen, unless an older save already owns one.
bool treeVisible(const Player& player,std::size_t tree) {
    return hiddenTreeAvailable(player,kTalentTrees[tree].id) || treeAccess(player,kTalentTrees[tree].id);
}

// Where every visible tree and category heading goes. `trees` is in on-screen
// order (column by column), which keyboard browsing follows too.
struct TreeLayout {
    std::vector<std::size_t> trees;
    std::vector<sf::Vector2f> origins;
    std::vector<std::pair<TreeCategory,sf::Vector2f>> headings;
};
TreeLayout layoutTrees(const Player& player) {
    TreeLayout layout;
    float columnY[3]{kTreeTop,kTreeTop,kTreeTop};
    const auto place=[&](int column,TreeCategory category,const std::vector<std::size_t>& trees) {
        if (trees.empty()) return;
        layout.headings.push_back({category,{kTreeColumnX[column],columnY[column]}});
        columnY[column]+=kCategoryHeight;
        for (const auto tree:trees) {
            layout.trees.push_back(tree);
            layout.origins.push_back({kTreeColumnX[column],columnY[column]});
            columnY[column]+=kTreeRowHeight;
        }
    };
    const auto treesIn=[&](TreeCategory category) {
        std::vector<std::size_t> result;
        for (std::size_t i=0;i<kTalentTrees.size();++i)
            if (treeCategory(kTalentTrees[i].id)==category && treeVisible(player,i)) result.push_back(i);
        return result;
    };
    place(0,TreeCategory::Martial,treesIn(TreeCategory::Martial));
    place(0,TreeCategory::Utility,treesIn(TreeCategory::Utility));
    place(1,TreeCategory::Magic,treesIn(TreeCategory::Magic));
    place(2,TreeCategory::Defence,treesIn(TreeCategory::Defence));
    // Hybrid trees only show for saves that already own one; split them
    // between the two shorter columns.
    const auto hybrid=treesIn(TreeCategory::Hybrid);
    place(1,TreeCategory::Hybrid,std::vector<std::size_t>(hybrid.begin(),hybrid.begin()+std::min<std::size_t>(2,hybrid.size())));
    if (hybrid.size()>2) place(2,TreeCategory::Hybrid,std::vector<std::size_t>(hybrid.begin()+2,hybrid.end()));
    return layout;
}
sf::FloatRect treeHeaderRect(sf::Vector2f origin) { return {origin,{kColumnWidth,22}}; }
sf::FloatRect abilityRect(sf::Vector2f origin,std::size_t ability) {
    return {{origin.x+4+kIconStride*ability,origin.y+24},{kIcon,kIcon}};
}
sf::FloatRect bindingRect(std::size_t slot) {
    return {{kBindingDialog.position.x+28+86.f*(slot%9),kBindingDialog.position.y+76+86.f*(slot/9)},{74,74}};
}
}

sf::FloatRect Application::talentTreeAbilityRect(std::size_t tree, std::size_t ability) const {
    const auto layout=layoutTrees(player_);
    for (std::size_t row=0;row<layout.trees.size();++row)
        if (layout.trees[row]==tree) return abilityRect(layout.origins[row],ability);
    return {};
}

void Application::handleTreeMouse(const sf::Event& event) {
    if(const auto* move=event.getIf<sf::Event::MouseMoved>()) mousePixel_=move->position;
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
    if(playButton.contains(p)) { closeTalentTrees(); return; }
    if(treeButton.contains(p)) { handleTreeKey(sf::Keyboard::Key::Enter,false); return; }
    if(abilityButton.contains(p)) { handleTreeKey(sf::Keyboard::Key::A,false); return; }
    if(bindButton.contains(p)) { handleTreeKey(sf::Keyboard::Key::B,false); return; }
    if(variantButton.contains(p)) { handleTreeKey(sf::Keyboard::Key::V,false); return; }
    const auto layout=layoutTrees(player_);
    for(std::size_t row=0;row<layout.trees.size();++row) {
        if(treeHeaderRect(layout.origins[row]).contains(p)) {
            treeSelection_=layout.trees[row]; abilitySelection_=0; imbueSelection_=0; treeFeedback_.clear(); return;
        }
        for(std::size_t i=0;i<4;++i) if(abilityRect(layout.origins[row],i).contains(p)) {
            treeSelection_=layout.trees[row]; abilitySelection_=i; imbueSelection_=0; treeFeedback_.clear(); return;
        }
    }
}

void Application::requestHotbar(std::size_t slot) {
    if (const auto index=player_.talents().hotbarIndex(slot)) requestTalent(*index);
}
void Application::openTalentTrees() {
    cancelTargeting(); mousePixel_.reset(); inventoryOpen_=false;
    if (!treeVisible(player_,treeSelection_)) treeSelection_=0;
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
    if (key==sf::Keyboard::Key::Up || key==sf::Keyboard::Key::Down) {
        // Browse in on-screen order, category by category.
        const auto order=layoutTrees(player_).trees;
        if (!order.empty()) {
            const auto at=static_cast<std::size_t>(std::find(order.begin(),order.end(),treeSelection_)-order.begin());
            const std::size_t current=at<order.size()?at:0;
            treeSelection_=order[(current+(key==sf::Keyboard::Key::Up?order.size()-1:1))%order.size()];
        }
        abilitySelection_=0;
    }
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
    const auto mouse=mousePixel_?std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)):std::nullopt;
    const auto hovered=[&](const sf::FloatRect& r){ return !bindingTalent_ && mouse && r.contains(*mouse); };
    ui_.panel(window_,{{0,0},{1280,720}},true,sf::Color(150,145,140));
    ui_.heading(window_,"Talents",{28,16},28);
    // Point counters, like ToME's boxed "Class points: 0" tabs.
    float x=200;
    for(const auto& [label,value]:{std::pair<std::string,int>{"Level",player_.level()},{"Tree points",player_.treePoints()},
                                   {"Ability points",player_.abilityPoints()}}) {
        const std::string text=label+": "+std::to_string(value);
        const sf::FloatRect box{{x,20},{ui_.textWidth(text,16,ui::Font::Bold)+24,30}};
        ui_.inset(window_,box,sf::Color(140,108,62));
        ui_.textCentered(window_,text,box,16,ui::kGold,ui::Font::Bold);
        x+=box.size.x+10;
    }
    ui_.button(window_,playButton,"Continue (T)",hovered(playButton));

    // --- Trees by category, three columns of icon rows ------------------------
    const auto layout=layoutTrees(player_);
    for(float dx:{kTreeColumnX[1]-14,kTreeColumnX[2]-14,836.f}) ui_.divider(window_,dx,72,700);
    for(const auto& [category,at]:layout.headings) {
        const auto info=categoryInfo(category);
        const float w=ui_.text(window_,info.name,{at.x,at.y},19,info.color,ui::Font::Title);
        sf::RectangleShape rule({kColumnWidth-w-10,1}); rule.setPosition({at.x+w+10,at.y+13});
        rule.setFillColor(sf::Color(info.color.r,info.color.g,info.color.b,110)); window_.draw(rule);
    }
    for(std::size_t row=0;row<layout.trees.size();++row) {
        const auto t=layout.trees[row];
        const auto origin=layout.origins[row];
        const auto& tree=kTalentTrees[t]; const auto* access=treeAccess(player_,tree.id);
        const auto header=treeHeaderRect(origin);
        const sf::Color headerColor=access?(access->specialized?ui::kGold:ui::kGood):ui::kMuted;
        sf::RectangleShape dash({10,2}); dash.setPosition({header.position.x,header.position.y+11});
        dash.setFillColor(headerColor); window_.draw(dash);
        ui_.text(window_,std::string(tree.name)+(access?(access->specialized?"  (specialised)":""):"  (locked)"),
            {header.position.x+16,header.position.y},16,t==treeSelection_?sf::Color(255,236,170):headerColor,ui::Font::Bold);
        for(std::size_t i=0;i<4;++i) {
            const auto& d=talentCatalog()[t*4+i];
            const int rank=player_.talents().rankOf(d.id);
            const auto r=abilityRect(origin,i);
            const bool chosen=t==treeSelection_ && i==abilitySelection_;
            ui_.inset(window_,r,chosen?ui::kGold:hovered(r)?ui::kBronze:i==3?sf::Color(120,70,150,120):sf::Color::Transparent);
            ui_.icon(window_,talentIcon(d.ranks[0]),{{r.position.x+6,r.position.y+6},{r.size.x-12,r.size.y-12}},
                rank?sf::Color(240,230,206):access?sf::Color(150,142,128):sf::Color(84,80,76));
            const std::string label=std::to_string(rank)+"/3";
            ui_.text(window_,label,{r.position.x+(r.size.x-ui_.textWidth(label,13,ui::Font::Bold))/2,r.position.y+r.size.y+1},13,
                rank==3?ui::kGood:rank?ui::kText:access?ui::kMuted:sf::Color(170,70,60),ui::Font::Bold);
        }
    }

    // --- Details of the selected ability -------------------------------------
    const auto& tree=kTalentTrees[treeSelection_];
    const auto* access=treeAccess(player_,tree.id);
    const auto& d=talentCatalog()[treeSelection_*4+abilitySelection_];
    const auto& t0=d.ranks[0];
    const int rank=player_.talents().rankOf(d.id);
    ui_.panel(window_,kDetails,false,sf::Color(130,125,125));
    const float left=kDetails.position.x+18, width=kDetails.size.x-36;
    float y=kDetails.position.y+14;
    const sf::FloatRect bigIcon{{left,y},{56,56}};
    ui_.inset(window_,bigIcon,ui::kBronze);
    ui_.icon(window_,talentIcon(t0),{{bigIcon.position.x+7,bigIcon.position.y+7},{42,42}},rank?ui::kGold:ui::kText);
    ui_.text(window_,t0.name,{left+68,y},22,ui::kGold,ui::Font::Title);
    ui_.text(window_,std::string(categoryInfo(treeCategory(tree.id)).name)+" / "+tree.name+(access?(access->specialized?", specialised":", open"):", locked")+
        (abilitySelection_==3?", advanced":"")+(t0.passive?", passive":""),{left+68,y+30},14,access?ui::kGood:ui::kMuted);
    y+=68;
    ui_.text(window_,"Current rank: "+std::to_string(rank)+" of 3",{left,y},15,ui::kText,ui::Font::Bold); y+=22;
    if (t0.armourRequirement!=ArmourRequirement::None) {
        const bool active=armourMatches(player_,t0.armourRequirement);
        ui_.paragraph(window_,std::string(active?"Equipment matches: ":"Inactive: ")+armourRequirementText(t0.armourRequirement),
            left,y,width,14,active?ui::kGood:ui::kBad);
    }
    y+=4;
    ui_.paragraph(window_,t0.description,left,y,width,15,ui::kText,ui::Font::Body,332);
    y=342;
    // Rank table.
    const char* headings[]{"Rank","Damage","Mana","Cooldown","Move","Extra"};
    const float columns[]{0,48,124,180,262,316};
    for(int c=0;c<6;++c) ui_.text(window_,headings[c],{left+columns[c],y},14,ui::kGold,ui::Font::Bold);
    y+=22;
    for (int r=0;r<3;++r) {
        const auto& t=d.ranks[r];
        const sf::Color c=r<rank?ui::kGood:ui::kText;
        const std::string cells[]{std::to_string(r+1),
            (t.passive || t.effectKind!=TalentEffectKind::Damage || t.shape==EffectShape::Movement)?"-":std::to_string(t.damagePercent)+"%",
            std::to_string(t.manaCost),std::to_string(t.cooldownTurns),t.moveDistance?std::to_string(t.moveDistance):"-",
            t.passive?std::to_string(t.passiveMagnitude):t.restoreMana?"+"+std::to_string(t.restoreMana)+" MP":
            t.restoreHpPercent?std::to_string(t.restoreHpPercent)+"% HP":
            t.selfBuffEffect && t.selfBuffEffect->type==StatusEffectType::Concealed?std::to_string(t.selfBuffEffect->magnitude):"-"};
        for(int col=0;col<6;++col) ui_.text(window_,cells[col],{left+columns[col],y},14,c);
        y+=20;
    }
    y+=10;
    const auto treeReason=treePurchaseReason(player_,playerClass_,tree);
    const auto reason=abilityPurchaseReason(player_,d);
    ui_.paragraph(window_,treeReason.empty()?(access?"You can specialise in this tree for 1 tree point.":"Unlock this tree for 1 tree point."):
        treeReason,left,y,width,14,treeReason.empty()?ui::kInfo:ui::kMuted,ui::Font::Body,500);
    ui_.paragraph(window_,reason.empty()?"Learning or ranking up costs 1 ability point; ranks keep their damage tier.":reason,
        left,y,width,14,reason.empty()?ui::kInfo:sf::Color(232,196,130),ui::Font::Body,548);
    ui_.text(window_,"Tree investment: "+std::to_string(treeInvestment(player_,tree.id)),{left,548},13,ui::kMuted);

    ui_.button(window_,treeButton,access?"Specialise (Enter)":"Unlock tree (Enter)",hovered(treeButton),treeReason.empty(),14);
    ui_.button(window_,abilityButton,rank>=3?"Maximum rank":rank?"Rank up (A)":"Learn (A)",hovered(abilityButton),reason.empty(),14);
    ui_.button(window_,bindButton,"Assign hotbar (B)",hovered(bindButton),rank>0 && !t0.passive,14);
    if(d.id=="spellblade.imbue") ui_.button(window_,variantButton,"Next element (V)",hovered(variantButton),rank>0,14);
    y=656;
    ui_.paragraph(window_,treeFeedback_.empty()?"Arrows browse trees and abilities. Purchases take no turn.":treeFeedback_,
        left,y,width,14,treeFeedback_.empty()?ui::kMuted:sf::Color(255,226,150),ui::Font::Body,704);

    // Hover tooltip for an ability icon you're not already inspecting.
    if(!bindingTalent_ && mouse) for(std::size_t row=0;row<layout.trees.size();++row) for(std::size_t i=0;i<4;++i) {
        if(!abilityRect(layout.origins[row],i).contains(*mouse)) continue;
        const auto& hd=talentCatalog()[layout.trees[row]*4+i];
        ui_.tooltip(window_,{{hd.ranks[0].name,ui::kGold,17,ui::Font::Title},
            {"Rank "+std::to_string(player_.talents().rankOf(hd.id))+" of 3"+(hd.ranks[0].passive?", passive":""),ui::kMuted,13},
            {hd.ranks[0].description,ui::kText,14},{"Click to see ranks and learn it.",ui::kInfo,13}},*mouse,300);
    }

    // --- Hotbar binding dialog -------------------------------------------------
    if(bindingTalent_) {
        sf::RectangleShape dim({1280,720}); dim.setFillColor(sf::Color(0,0,0,140)); window_.draw(dim);
        ui_.panel(window_,kBindingDialog,true,sf::Color(150,145,140));
        ui_.text(window_,"Assign to a hotbar slot",{kBindingDialog.position.x+24,kBindingDialog.position.y+16},20,ui::kGold,ui::Font::Title);
        ui_.text(window_,"Page one on top, page two below. The slot you pick is replaced.",
            {kBindingDialog.position.x+24,kBindingDialog.position.y+46},14,ui::kMuted);
        ui_.button(window_,cancelBindingButton,"Cancel (Esc)",mouse && cancelBindingButton.contains(*mouse),true,14);
        const auto& bindings=player_.talents().hotbar();
        for(std::size_t slot=0;slot<18;++slot) {
            const auto rect=bindingRect(slot);
            ui_.inset(window_,rect,mouse && rect.contains(*mouse)?ui::kGold:sf::Color::Transparent);
            ui_.text(window_,std::string(slot<9?"":"Shift ")+std::to_string(slot%9+1),{rect.position.x+4,rect.position.y+2},12,ui::kMuted,ui::Font::Bold);
            const auto* bound=slot<bindings.size()?findTalentDefinition(bindings[slot]):nullptr;
            const Talent* known=nullptr;
            if(!bound && slot<bindings.size()) for(const auto& k:player_.talents().knownTalents()) if(k.id==bindings[slot]) { known=&k; break; }
            if(bound) ui_.icon(window_,talentIcon(bound->ranks[0]),{{rect.position.x+12,rect.position.y+12},{50,50}},ui::kText);
            else if(known) ui_.icon(window_,talentIcon(*known),{{rect.position.x+12,rect.position.y+12},{50,50}},ui::kText);
        }
    }
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
