#include "core/Application.hpp"
#include "entities/RunProgression.hpp"
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
    if (id=="one_handed" || id=="two_handed" || id=="bow" || id=="brawling" || id=="whip" ||
        id=="spear" || id=="daggers" || id=="mace" || id=="crossbow") return TreeCategory::Martial;
    if (id=="fire" || id=="ice" || id=="lightning" || id=="arcane" || id=="shadow" || id=="radiance" ||
        id=="earth" || id=="tide" || id=="hexes" || id=="venom") return TreeCategory::Magic;
    if (id=="stealth" || id=="acrobatics" || id=="alchemy" || id=="traps" || id=="skirmish") return TreeCategory::Utility;
    if (id=="shield" || id=="cloth" || id=="light_armour" || id=="heavy_armour") return TreeCategory::Defence;
    return TreeCategory::Hybrid;
}

// Hybrid trees always show, with what they ask for; Blood Magic stays a
// secret until the altar (or an older save already owns it).
bool treeVisible(const Player& player,std::size_t tree) {
    const std::string id=kTalentTrees[tree].id;
    return player.sandbox || hiddenTreeAvailable(player,id) || treeAccess(player,id) || !hybridRequirement(id).empty();
}

// A forked tree lays its nodes out by tier, the two sides of each fork one
// above the other; it takes an extra lane of height.
constexpr float kLane=72;
bool forkedTree(std::size_t tree) {
    for (const auto* d:treeNodes(tree)) if (!d->fork.empty()) return true;
    return false;
}
float treeRowHeight(std::size_t tree) { return kTreeRowHeight+(forkedTree(tree)?kLane:0); }
// Whether another side of this node's fork has been taken.
bool forkClosed(const Player& player,const TalentDefinition& d) {
    if (d.fork.empty() || player.talents().rankOf(d.id)) return false;
    for (const auto* other:treeNodes(d.treeId)) if (other!=&d && other->fork==d.fork && player.talents().rankOf(other->id)) return true;
    return false;
}

// Where every visible tree and category heading goes. `trees` is in on-screen
// order (column by column), which keyboard browsing follows too.
struct TreeLayout {
    std::vector<std::size_t> trees;
    std::vector<sf::Vector2f> origins;
    std::vector<int> columns;          // which column each tree sits in
    std::vector<std::pair<TreeCategory,sf::Vector2f>> headings;
    float bottoms[3]{kTreeTop,kTreeTop,kTreeTop}; // where each column ends, before scrolling
};
// Each column moves up by its own scroll.
TreeLayout layoutTrees(const Player& player,const std::array<float,3>& scroll={}) {
    TreeLayout layout;
    float columnY[3]{kTreeTop,kTreeTop,kTreeTop};
    const auto place=[&](int column,TreeCategory category,const std::vector<std::size_t>& trees) {
        if (trees.empty()) return;
        const float shift=scroll.at(static_cast<std::size_t>(column));
        layout.headings.push_back({category,{kTreeColumnX[column],columnY[column]-shift}});
        columnY[column]+=kCategoryHeight;
        for (const auto tree:trees) {
            layout.trees.push_back(tree);
            layout.columns.push_back(column);
            layout.origins.push_back({kTreeColumnX[column],columnY[column]-shift});
            columnY[column]+=treeRowHeight(tree);
        }
    };
    const auto treesIn=[&](TreeCategory category) {
        std::vector<std::size_t> result;
        for (std::size_t i=0;i<kTalentTrees.size();++i)
            if (treeCategory(kTalentTrees[i].id)==category && treeVisible(player,i)) result.push_back(i);
        return result;
    };
    place(0,TreeCategory::Martial,treesIn(TreeCategory::Martial));
    place(1,TreeCategory::Magic,treesIn(TreeCategory::Magic));
    place(2,TreeCategory::Defence,treesIn(TreeCategory::Defence));
    // Utility goes wherever there is most room.
    place(static_cast<int>(std::min_element(columnY,columnY+3)-columnY),TreeCategory::Utility,treesIn(TreeCategory::Utility));
    // Hybrid trees only show for saves that already own one; split them
    // between the two shorter columns.
    const auto hybrid=treesIn(TreeCategory::Hybrid);
    place(1,TreeCategory::Hybrid,std::vector<std::size_t>(hybrid.begin(),hybrid.begin()+std::min<std::size_t>(2,hybrid.size())));
    if (hybrid.size()>2) place(2,TreeCategory::Hybrid,std::vector<std::size_t>(hybrid.begin()+2,hybrid.end()));
    for (int c=0;c<3;++c) layout.bottoms[c]=columnY[c];
    return layout;
}
constexpr float kTreeViewTop=kTreeTop-6, kTreeViewWidth=836;
sf::FloatRect treeHeaderRect(sf::Vector2f origin) { return {origin,{kColumnWidth,22}}; }
sf::FloatRect abilityRect(sf::Vector2f origin,std::size_t tree,std::size_t ability) {
    if (!forkedTree(tree)) return {{origin.x+4+kIconStride*ability,origin.y+24},{kIcon,kIcon}};
    const auto& nodes=treeNodes(tree);
    const auto* d=nodes.at(ability);
    int lane=0, inTier=0;
    for (std::size_t i=0;i<nodes.size();++i) if (nodes[i]->tier==d->tier) { if (i<ability) ++lane; ++inTier; }
    const float y=origin.y+24+(inTier==1?kLane/2:kLane*static_cast<float>(lane));
    return {{origin.x+4+kIconStride*static_cast<float>(d->tier),y},{kIcon,kIcon}};
}
sf::FloatRect bindingRect(std::size_t slot) {
    return {{kBindingDialog.position.x+28+86.f*(slot%9),kBindingDialog.position.y+76+86.f*(slot/9)},{74,74}};
}
}

float Application::treeScrollMax(int column) const {
    return std::max(0.f,layoutTrees(player_).bottoms[std::clamp(column,0,2)]-treeViewBottom_);
}
void Application::scrollTrees(int column, float pixels) {
    auto& scroll=treeScroll_.at(static_cast<std::size_t>(std::clamp(column,0,2)));
    scroll=std::clamp(scroll+pixels,0.f,treeScrollMax(column));
}
int Application::treeColumnOf(std::size_t tree) const {
    const auto layout=layoutTrees(player_);
    for (std::size_t row=0;row<layout.trees.size();++row) if (layout.trees[row]==tree) return layout.columns[row];
    return 0;
}
// Keep the selected tree's row on screen, in its own column.
void Application::revealSelectedTree() {
    const auto layout=layoutTrees(player_);
    for (std::size_t row=0;row<layout.trees.size();++row) if (layout.trees[row]==treeSelection_) {
        auto& scroll=treeScroll_.at(static_cast<std::size_t>(layout.columns[row]));
        const float top=layout.origins[row].y, bottom=top+treeRowHeight(treeSelection_);
        if (top-kCategoryHeight<scroll+kTreeViewTop) scroll=top-kCategoryHeight-kTreeViewTop;
        if (bottom>scroll+treeViewBottom_) scroll=bottom-treeViewBottom_;
    }
    for (int c=0;c<3;++c) scrollTrees(c,0);
}

sf::FloatRect Application::talentTreeAbilityRect(std::size_t tree, std::size_t ability) const {
    const auto layout=layoutTrees(player_,treeScroll_);
    for (std::size_t row=0;row<layout.trees.size();++row)
        if (layout.trees[row]==tree) return abilityRect(layout.origins[row],tree,ability);
    return {};
}

void Application::handleTreeMouse(const sf::Event& event) {
    if(const auto* move=event.getIf<sf::Event::MouseMoved>()) mousePixel_=move->position;
    if(const auto* wheel=event.getIf<sf::Event::MouseWheelScrolled>(); wheel && !bindingTalent_ && wheel->position.x<kTreeViewWidth) {
        // The wheel scrolls the column it's over.
        const float x=static_cast<float>(wheel->position.x);
        const int column=x<kTreeColumnX[1]-14?0:x<kTreeColumnX[2]-14?1:2;
        scrollTrees(column,-wheel->delta*kTreeRowHeight/2); return;
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
    if(playButton.contains(p)) { closeTalentTrees(); return; }
    if(treeButton.contains(p)) { handleTreeKey(sf::Keyboard::Key::Enter,false); return; }
    if(abilityButton.contains(p)) { handleTreeKey(sf::Keyboard::Key::A,false); return; }
    if(bindButton.contains(p)) { handleTreeKey(sf::Keyboard::Key::B,false); return; }
    if(variantButton.contains(p)) { handleTreeKey(sf::Keyboard::Key::V,false); return; }
    const auto shown=visibleResonances();
    for(std::size_t i=0;i<shown.size();++i) if(resonanceRect(i).contains(p)) { resonanceSelection_=shown[i]; treeFeedback_.clear(); return; }
    if(p.y<kTreeViewTop || p.y>treeViewBottom_) return; // scrolled out of view
    const auto layout=layoutTrees(player_,treeScroll_);
    for(std::size_t row=0;row<layout.trees.size();++row) {
        if(treeHeaderRect(layout.origins[row]).contains(p)) {
            resonanceSelection_.reset();
            treeSelection_=layout.trees[row]; abilitySelection_=0; imbueSelection_=0; treeFeedback_.clear(); return;
        }
        for(std::size_t i=0;i<treeNodes(layout.trees[row]).size();++i) if(abilityRect(layout.origins[row],layout.trees[row],i).contains(p)) {
            resonanceSelection_.reset();
            treeSelection_=layout.trees[row]; abilitySelection_=i; imbueSelection_=0; treeFeedback_.clear(); return;
        }
    }
}

// The resonance strip sits above the details panel, right to left from Continue.
sf::FloatRect Application::resonanceRect(std::size_t visible) const {
    return {{1088.f-46.f*static_cast<float>(visible+1),18.f},{38.f,38.f}};
}
std::vector<std::size_t> Application::visibleResonances() const {
    std::vector<std::size_t> shown;
    for (std::size_t i=0;i<resonances().size();++i) if (resonanceGlimpsed(player_,*resonances()[i])) shown.push_back(i);
    return shown;
}
void Application::renderResonanceDetails() {
    const auto mouse=mousePixel_?std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)):std::nullopt;
    const auto& d=*resonances().at(*resonanceSelection_);
    const bool learned=player_.talents().rankOf(d.id)>0, awake=resonanceAwake(player_,d);
    ui_.glass(window_,kDetails,false);
    const float left=kDetails.position.x+18, width=kDetails.size.x-36;
    float y=kDetails.position.y+14;
    const sf::FloatRect bigIcon{{left,y},{56,56}};
    ui_.inset(window_,bigIcon,awake||learned?ui::kGold:ui::kBronze);
    ui_.icon(window_,talentIcon(d.ranks[0]),{{bigIcon.position.x+7,bigIcon.position.y+7},{42,42}},
        learned?ui::kGold:awake?sf::Color(255,226,160):sf::Color(58,54,66));
    const auto a=affinityInfo(d.resonance[0]), b=affinityInfo(d.resonance[1]);
    ui_.text(window_,awake||learned?d.ranks[0].name:"???",{left+68,y},22,ui::kGold,ui::Font::Title);
    ui_.text(window_,a.name,{left+68,y+30},14,sf::Color(a.r,a.g,a.b),ui::Font::Bold);
    const float ax=left+68+ui_.textWidth(a.name,14,ui::Font::Bold);
    ui_.text(window_," and ",{ax,y+30},14,ui::kMuted);
    ui_.text(window_,b.name,{ax+ui_.textWidth(" and ",14),y+30},14,sf::Color(b.r,b.g,b.b),ui::Font::Bold);
    y+=78;
    if (awake || learned) ui_.paragraph(window_,d.ranks[0].description,left,y,width,15,ui::kText,ui::Font::Body,292);
    else ui_.paragraph(window_,"Something stirs between "+std::string(a.name)+" and "+b.name+".",left,y,width,15,ui::kMuted,ui::Font::Body,292);
    // Where each colour stands, as plain numbers.
    y=300;
    for (const auto colour:{d.resonance[0],d.resonance[1]}) {
        const auto info=affinityInfo(colour);
        const int points=affinityPoints(player_,colour);
        ui_.text(window_,std::string(info.name)+" "+std::to_string(points)+(points>=kResonancePoints?"":" / "+std::to_string(kResonancePoints)),
            {left,y},16,sf::Color(info.r,info.g,info.b),ui::Font::Bold);
        y+=24;
    }
    const auto reason=abilityPurchaseReason(player_,d);
    ui_.button(window_,abilityButton,learned?"Learned":"Learn (A)",mouse && abilityButton.contains(*mouse),reason.empty(),14);
    y=656; ui_.paragraph(window_,treeFeedback_,left,y,width,14,sf::Color(255,226,150),ui::Font::Body,704);
}

void Application::requestHotbar(std::size_t slot) {
    if (const auto index=player_.talents().hotbarIndex(slot)) requestTalent(*index);
}
void Application::openTalentTrees() {
    cancelTargeting(); mousePixel_.reset(); inventoryOpen_=false;
    if (!treeVisible(player_,treeSelection_)) treeSelection_=0;
    revealSelectedTree();
    bindingTalent_=false; treeFeedback_.clear(); mode_=GameMode::AbilityChoice;
}
void Application::closeTalentTrees() {
    if (!openTrees(player_,false)) { treeFeedback_="Unlock your first class tree with the Unlock button or Enter before continuing."; return; }
    if (player_.abilityPoints()==earnedAbilityPoints(1) && player_.level()==1) { treeFeedback_="Learn at least one ability with the Learn button or A before continuing."; return; }
    progressionReviewPending_=false;
    mode_=GameMode::Playing; resumeLevelUpSequence();
}
void Application::handleTreeKey(sf::Keyboard::Key key, bool shift) {
    if (key==sf::Keyboard::Key::Escape && bindingTalent_) { bindingTalent_=false; treeFeedback_="Hotbar assignment cancelled."; return; }
    if (key==sf::Keyboard::Key::Up || key==sf::Keyboard::Key::Down || key==sf::Keyboard::Key::Left || key==sf::Keyboard::Key::Right || key==sf::Keyboard::Key::A || key==sf::Keyboard::Key::Enter) bindingTalent_=false;
    if (key==sf::Keyboard::Key::F5) { saveGame(); return; }
    if (key==sf::Keyboard::Key::T || key==sf::Keyboard::Key::Escape) { closeTalentTrees(); return; }
    if (key==sf::Keyboard::Key::Up || key==sf::Keyboard::Key::Down || key==sf::Keyboard::Key::Left || key==sf::Keyboard::Key::Right)
        resonanceSelection_.reset();
    if (key==sf::Keyboard::Key::Up || key==sf::Keyboard::Key::Down) {
        // Browse in on-screen order, category by category.
        const auto order=layoutTrees(player_).trees;
        if (!order.empty()) {
            const auto at=static_cast<std::size_t>(std::find(order.begin(),order.end(),treeSelection_)-order.begin());
            const std::size_t current=at<order.size()?at:0;
            treeSelection_=order[(current+(key==sf::Keyboard::Key::Up?order.size()-1:1))%order.size()];
        }
        abilitySelection_=0;
        revealSelectedTree();
    }
    const std::size_t nodes=treeNodes(treeSelection_).size();
    if (key==sf::Keyboard::Key::Left) abilitySelection_=(abilitySelection_+nodes-1)%nodes;
    if (key==sf::Keyboard::Key::Right) abilitySelection_=(abilitySelection_+1)%nodes;
    abilitySelection_=std::min(abilitySelection_,nodes-1);
    const auto& tree=kTalentTrees[treeSelection_];
    const auto& ability=resonanceSelection_?*resonances().at(*resonanceSelection_):*treeNodes(treeSelection_)[abilitySelection_];
    if (key==sf::Keyboard::Key::Enter && !resonanceSelection_) {
        treeFeedback_=treePurchaseReason(player_,playerClass_,tree);
        const bool first=player_.trees().empty();
        const bool opening=!treeAccess(player_,tree.id);
        if (purchaseTree(player_,playerClass_,tree)) {
            treeFeedback_=std::string(tree.name)+" purchased. Select an ability, then click Learn or press A.";
            // A weapon tree comes with a training weapon if you carry none of
            // its kind; your first tree also puts it in your hands.
            if (opening && tree.starterItem[0]) {
                const auto kind=findItemDefinition(tree.starterItem)->weaponKind;
                bool owned=false;
                for (const auto& item:player_.inventory().items()) owned=owned || (item->definition() && item->definition()->weaponKind==kind);
                for (int slot=0;slot<kEquipmentSlotCount && !owned;++slot)
                    if (const auto* worn=player_.inventory().equipped(static_cast<EquipmentSlot>(slot))) owned=worn->definition() && worn->definition()->weaponKind==kind;
                const ItemDefinition* training=nullptr;
                for (const auto& d:kItemDefinitions) if (trainingItem(d) && d.weaponKind==kind) training=&d;
                if (!owned && training && !player_.inventory().full()) {
                    player_.inventory().add(std::make_unique<Item>(*training,nextItemId_++));
                    if (first) player_.equip(player_.inventory().items().size()-1);
                    else treeFeedback_+=" A "+std::string(training->name)+" is in your bag.";
                }
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
    // Two framed halves (trees, details): on a wide screen they dock to its edges.
    ui_.glass(window_,{{0,0},{842,720}},true);
    ui_.glass(window_,{{842,0},{438,720}},true);
    ui_.heading(window_,"Talents",{28,16},28);
    // Point counters, like ToME's boxed "Class points: 0" tabs.
    float x=200;
    for(const auto& [label,value]:{std::pair<std::string,int>{"Level",player_.level()},{"Tree points",player_.treePoints()},
                                   {"Ability points",player_.abilityPoints()},{"Utility points",player_.utilityPoints()}}) {
        const std::string text=label+": "+std::to_string(value);
        const sf::FloatRect box{{x,20},{ui_.textWidth(text,16,ui::Font::Bold)+24,30}};
        ui_.inset(window_,box,sf::Color(140,108,62));
        ui_.textCentered(window_,text,box,16,ui::kGold,ui::Font::Bold);
        x+=box.size.x+10;
    }
    // Your colours: plain counts, in their own colour.
    x+=6;
    for (int a=1;a<=static_cast<int>(Affinity::Rot);++a) {
        const auto affinity=static_cast<Affinity>(a);
        const int points=affinityPoints(player_,affinity);
        if (!points) continue;
        const auto info=affinityInfo(affinity);
        const std::string text=std::string(info.name)+" "+std::to_string(points);
        if (x+ui_.textWidth(text,15,ui::Font::Bold)>832) break;
        ui_.text(window_,text,{x,26},15,sf::Color(info.r,info.g,info.b),ui::Font::Bold);
        x+=ui_.textWidth(text,15,ui::Font::Bold)+14;
    }
    ui_.button(window_,playButton,"Continue (T)",hovered(playButton));
    // Resonances: dim once you hold one of their colours, glowing once both are deep enough.
    {
        const auto shown=visibleResonances();
        for (std::size_t i=0;i<shown.size();++i) {
            const auto& d=*resonances()[shown[i]];
            const auto r=resonanceRect(i);
            const bool learned=player_.talents().rankOf(d.id)>0, awake=resonanceAwake(player_,d);
            const bool chosen=resonanceSelection_==shown[i];
            const float pulse=.5f+.5f*std::sin(static_cast<float>(animationClock_.getElapsedTime().asSeconds())*3.f);
            const sf::Color glow=learned?ui::kRare:awake?sf::Color(255,214,120,static_cast<std::uint8_t>(140+100*pulse)):sf::Color::Transparent;
            ui_.inset(window_,r,chosen?ui::kGold:hovered(r)?ui::kBronze:glow);
            ui_.icon(window_,talentIcon(d.ranks[0]),{{r.position.x+5,r.position.y+5},{r.size.x-10,r.size.y-10}},
                learned?sf::Color(240,230,206):awake?sf::Color(255,226,160):sf::Color(58,54,66));
        }
    }

    // --- Trees by category, three columns of icon rows ------------------------
    for (int c=0;c<3;++c) scrollTrees(c,0); // the tree list may have changed size
    const auto layout=layoutTrees(player_,treeScroll_);
    for(float dx:{kTreeColumnX[1]-14,kTreeColumnX[2]-14,836.f}) ui_.divider(window_,dx,72,700);
    const float viewHeight=treeViewBottom_-kTreeViewTop;
    // Clipped to the tree area, within whatever frame the screen is drawn in.
    const sf::View frame=window_.getView();
    // Only the part of the tree area inside this frame (a docked half may hold none of it).
    const sf::Vector2f frameOrigin=frame.getCenter()-frame.getSize()/2.f, frameSize=frame.getSize();
    const float clipLeft=std::max(0.f,frameOrigin.x), clipRight=std::min(kTreeViewWidth,frameOrigin.x+frameSize.x);
    sf::View clip(sf::FloatRect({clipLeft,kTreeViewTop},{std::max(1.f,clipRight-clipLeft),viewHeight}));
    {
        const auto vp=frame.getViewport();
        clip.setViewport(sf::FloatRect({vp.position.x+vp.size.x*(clipLeft-frameOrigin.x)/frameSize.x,vp.position.y+vp.size.y*(kTreeViewTop-frameOrigin.y)/frameSize.y},
            {clipRight>clipLeft?vp.size.x*(clipRight-clipLeft)/frameSize.x:0.f,vp.size.y*viewHeight/frameSize.y}));
    }
    window_.setView(clip);
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
        const sf::Color headerColor=access?ui::kGood:ui::kMuted;
        sf::RectangleShape dash({10,2}); dash.setPosition({header.position.x,header.position.y+11});
        dash.setFillColor(headerColor); window_.draw(dash);
        ui_.text(window_,std::string(tree.name)+(access?"":"  (locked)"),
            {header.position.x+16,header.position.y},16,t==treeSelection_?sf::Color(255,236,170):headerColor,ui::Font::Bold);
        // Forked trees: a thin line from each node back to what it needs.
        if (forkedTree(t)) for(std::size_t i=0;i<treeNodes(t).size();++i) for (const auto& need:treeNodes(t)[i]->prerequisites)
            for(std::size_t j=0;j<treeNodes(t).size();++j) if (treeNodes(t)[j]->id==need) {
                const auto a=abilityRect(origin,t,j), b=abilityRect(origin,t,i);
                const bool lit=player_.talents().rankOf(need)>0;
                const sf::Color c=lit?sf::Color(200,160,90,200):sf::Color(90,80,70,160);
                const sf::Vertex line[]{{{a.position.x+a.size.x,a.position.y+a.size.y/2},c},{{b.position.x,b.position.y+b.size.y/2},c}};
                window_.draw(line,2,sf::PrimitiveType::Lines);
            }
        for(std::size_t i=0;i<treeNodes(t).size();++i) {
            const auto& d=*treeNodes(t)[i];
            const int rank=player_.talents().rankOf(d.id);
            const auto r=abilityRect(origin,t,i);
            const bool closed=forkClosed(player_,d); // the other side of its fork was taken
            const bool chosen=t==treeSelection_ && i==abilitySelection_;
            ui_.inset(window_,r,chosen?ui::kGold:hovered(r)?ui::kBronze:d.tier==3?sf::Color(120,70,150,120):sf::Color::Transparent);
            ui_.icon(window_,talentIcon(d.ranks[0]),{{r.position.x+6,r.position.y+6},{r.size.x-12,r.size.y-12}},
                rank?sf::Color(240,230,206):closed?sf::Color(52,48,46):access?sf::Color(150,142,128):sf::Color(84,80,76));
            const std::string label=std::to_string(rank)+"/"+std::to_string(d.maxRank());
            ui_.text(window_,label,{r.position.x+(r.size.x-ui_.textWidth(label,13,ui::Font::Bold))/2,r.position.y+r.size.y+1},13,
                rank==d.maxRank()?ui::kRare:rank>=3?ui::kGood:rank?ui::kText:closed?sf::Color(70,64,60):access?ui::kMuted:sf::Color(170,70,60),ui::Font::Bold);
        }
    }
    window_.setView(frame);
    // A slim scrollbar at the right of each column that overflows.
    for (int c=0;c<3;++c) if (const float range=treeScrollMax(c); range>0) {
        const float trackTop=kTreeViewTop+4, track=viewHeight-8, thumb=std::max(40.f,track*viewHeight/(viewHeight+range));
        const float x=c<2?kTreeColumnX[c+1]-22:829.f;
        sf::RectangleShape rail({4,track}); rail.setPosition({x,trackTop}); rail.setFillColor(sf::Color(60,52,44)); window_.draw(rail);
        sf::RectangleShape bar({4,thumb}); bar.setPosition({x,trackTop+(track-thumb)*treeScroll_.at(static_cast<std::size_t>(c))/range});
        bar.setFillColor(ui::kBronze); window_.draw(bar);
    }

    // --- Details of the selected ability -------------------------------------
    if (resonanceSelection_) renderResonanceDetails(); else {
    const auto& tree=kTalentTrees[treeSelection_];
    const auto* access=treeAccess(player_,tree.id);
    const auto& d=*treeNodes(treeSelection_)[std::min(abilitySelection_,treeNodes(treeSelection_).size()-1)];
    const auto& t0=d.ranks[0];
    const int rank=player_.talents().rankOf(d.id);
    ui_.glass(window_,kDetails,false);
    const float left=kDetails.position.x+18, width=kDetails.size.x-36;
    float y=kDetails.position.y+14;
    const sf::FloatRect bigIcon{{left,y},{56,56}};
    ui_.inset(window_,bigIcon,ui::kBronze);
    ui_.icon(window_,talentIcon(t0),{{bigIcon.position.x+7,bigIcon.position.y+7},{42,42}},rank?ui::kGold:ui::kText);
    ui_.text(window_,t0.name,{left+68,y},22,ui::kGold,ui::Font::Title);
    ui_.text(window_,std::string(categoryInfo(treeCategory(tree.id)).name)+" / "+tree.name+(access?", open":", locked")+
        (d.tier==3?", advanced":"")+(t0.passive?", passive":""),{left+68,y+30},14,access?ui::kGood:ui::kMuted);
    y+=68;
    ui_.text(window_,"Current rank: "+std::to_string(rank)+" of "+std::to_string(d.maxRank()),{left,y},15,ui::kText,ui::Font::Bold); y+=22;
    if (t0.armourRequirement!=ArmourRequirement::None) {
        const bool active=armourMatches(player_,t0.armourRequirement);
        ui_.paragraph(window_,std::string(active?"Equipment matches: ":"Inactive: ")+armourRequirementText(t0.armourRequirement),
            left,y,width,14,active?ui::kGood:ui::kBad);
    }
    y+=4;
    ui_.paragraph(window_,t0.description,left,y,width,15,ui::kText,ui::Font::Body,292);
    y=298;
    // Rank table.
    const char* headings[]{"Rank","Damage","Mana","Cooldown","Move","Extra"};
    const float columns[]{0,48,124,180,262,316};
    for(int c=0;c<6;++c) ui_.text(window_,headings[c],{left+columns[c],y},14,ui::kGold,ui::Font::Bold);
    y+=22;
    for (int r=0;r<d.maxRank();++r) {
        const auto& t=d.ranks[static_cast<std::size_t>(r)];
        const sf::Color c=r<rank?ui::kGood:ui::kText;
        const std::string cells[]{std::to_string(r+1),
            (t.passive || t.effectKind!=TalentEffectKind::Damage || t.shape==EffectShape::Movement)?"-":std::to_string(t.damagePercent)+"%",
            std::to_string(t.manaCost),std::to_string(t.cooldownTurns),t.moveDistance?std::to_string(t.moveDistance):"-",
            t.passive?std::to_string(t.passiveMagnitude+passiveGrowth(t,player_.stats())):t.restoreMana?"+"+std::to_string(t.restoreMana)+" MP":
            t.restoreHpPercent?std::to_string(t.restoreHpPercent)+"% HP":
            t.selfBuffEffect && t.selfBuffEffect->type==StatusEffectType::Concealed?std::to_string(t.selfBuffEffect->magnitude):"-"};
        for(int col=0;col<6;++col) ui_.text(window_,cells[col],{left+columns[col],y},14,c);
        y+=19;
    }
    y+=4;
    if (!d.mastery.empty()) {
        ui_.paragraph(window_,"Rank "+std::to_string(d.maxRank())+" mastery: "+d.mastery,left,y,width,14,rank>=d.maxRank()?ui::kRare:sf::Color(214,170,96),ui::Font::Bold,y+40);
        y+=4;
    }
    y+=4;
    const auto treeReason=treePurchaseReason(player_,playerClass_,tree);
    const auto reason=abilityPurchaseReason(player_,d);
    if (!access) ui_.paragraph(window_,treeReason.empty()?"Unlock this tree for 1 tree point.":treeReason,
        left,y,width,14,treeReason.empty()?ui::kInfo:ui::kMuted,ui::Font::Body,500);
    ui_.paragraph(window_,reason.empty()?"Learning or ranking up costs 1 ability point; ranks keep their damage tier.":reason,
        left,y,width,14,reason.empty()?ui::kInfo:sf::Color(232,196,130),ui::Font::Body,548);
    ui_.text(window_,"Tree investment: "+std::to_string(treeInvestment(player_,tree.id)),{left,548},13,ui::kMuted);

    if (!access) ui_.button(window_,treeButton,"Unlock tree (Enter)",hovered(treeButton),treeReason.empty(),14);
    ui_.button(window_,abilityButton,rank>=d.maxRank()?"Maximum rank":rank?"Rank up (A)":"Learn (A)",hovered(abilityButton),reason.empty(),14);
    ui_.button(window_,bindButton,"Assign hotbar (B)",hovered(bindButton),rank>0 && !t0.passive,14);
    if(d.id=="spellblade.imbue") ui_.button(window_,variantButton,"Next element (V)",hovered(variantButton),rank>0,14);
    y=656;
    ui_.paragraph(window_,treeFeedback_.empty()?"Arrows browse trees and abilities. Purchases take no turn.":treeFeedback_,
        left,y,width,14,treeFeedback_.empty()?ui::kMuted:sf::Color(255,226,150),ui::Font::Body,704);

    }

    // Hover tooltip for a resonance.
    if (!bindingTalent_ && mouse) {
        const auto shown=visibleResonances();
        for (std::size_t i=0;i<shown.size();++i) if (resonanceRect(i).contains(*mouse)) {
            const auto& d=*resonances()[shown[i]];
            const bool known=resonanceAwake(player_,d) || player_.talents().rankOf(d.id);
            ui_.tooltip(window_,{{known?d.ranks[0].name:"???",ui::kGold,17,ui::Font::Title},
                {known?d.ranks[0].description:"Something stirs between "+std::string(affinityInfo(d.resonance[0]).name)+" and "+
                    affinityInfo(d.resonance[1]).name+".",ui::kText,14}},*mouse,300);
        }
    }

    // Hover tooltip for an ability icon you're not already inspecting.
    if(!bindingTalent_ && mouse && mouse->y>=kTreeViewTop && mouse->y<=treeViewBottom_) for(std::size_t row=0;row<layout.trees.size();++row) for(std::size_t i=0;i<treeNodes(layout.trees[row]).size();++i) {
        if(!abilityRect(layout.origins[row],layout.trees[row],i).contains(*mouse)) continue;
        const auto& hd=*treeNodes(layout.trees[row])[i];
        ui_.tooltip(window_,{{hd.ranks[0].name,ui::kGold,17,ui::Font::Title},
            {"Rank "+std::to_string(player_.talents().rankOf(hd.id))+" of "+std::to_string(hd.maxRank())+(hd.ranks[0].passive?", passive":""),ui::kMuted,13},
            {hd.ranks[0].description,ui::kText,14},{"Click to see ranks and learn it.",ui::kInfo,13}},*mouse,300);
    }

    // --- Hotbar binding dialog -------------------------------------------------
    if(bindingTalent_) {
        beginMenu(140);
        ui_.glass(window_,kBindingDialog,true);
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
    if (const int soft=player_.talents().passiveValue(PassiveKind::SoftSteps,player_.stats())) chance*=(100-soft)/100.f;
    // Hiding in the dark: invisible to those who need light, and harder to spot for the rest.
    if (!tileLit(target)) {
        if (!canSee(enemy,target)) return 0.f;
        chance*=.6f;
    }
    return chance;
}

void Application::applyMovementTalents(Position previous) {
    const auto now=player_.position();
    if (now.x==previous.x && now.y==previous.y) return;
    player_.statusEffects().apply({StatusEffectType::Opening,2,0});
    if (const int flow=player_.talents().passiveValue(PassiveKind::FlowingMana,player_.stats()))
        player_.stats().mana=std::min(player_.stats().maxMana,player_.stats().mana+flow);
    if (const int shade=player_.talents().passiveValue(PassiveKind::ShadeStep,player_.stats()))
        player_.statusEffects().apply({StatusEffectType::Concealed,shade,std::max(1,player_.statusEffects().magnitudeOf(StatusEffectType::Concealed))});
    const int evasion=player_.talents().passiveValue(PassiveKind::Footwork,player_.stats());
    if (evasion) player_.statusEffects().apply({StatusEffectType::Evasion,1,evasion});
    const int burn=player_.talents().passiveValue(PassiveKind::Kindle,player_.stats());
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
