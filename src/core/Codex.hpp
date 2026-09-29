#pragma once

#include <array>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <system_error>

namespace engine {
struct LoreFragment { const char* id; int family; const char* title; const char* source; const char* text; };
inline constexpr std::array<LoreFragment,9> kLoreFragments{{
    {"town.ember",0,"The Scorched Practice Blade","Town book", "The blade was dull, yet its cuts left cinders. In the margin: first learn the weight of steel, then the patience of a flame."},
    {"warlord.ember",0,"Orders to the Ash Guard","Goblin Warlord", "The captain forbids his guards to choose between sword drills and the furnace rites. A divided education, he writes, makes a single weapon."},
    {"town.grave",1,"A Bell Without a Ringer","Town rumour", "A traveller heard the crypt bell after its keeper was buried. Every night, the ringing moved one chamber nearer the gate."},
    {"skeleton.grave",1,"A Name Carved Inside a Rib","Crypt Skeleton", "The same name has been carved again and again, deeper each time. Beside it: remember who called you, and you will never rest."},
    {"lich.grave",1,"The Last Page of the Funeral Book","Lich", "The final rite is crossed out. In its place: death is a threshold, and a name spoken with enough will can hold the door."},
    {"town.shadow",2,"The Hunter Who Left No Tracks","Town book", "The hunter waited until even the birds forgot her. When the bowstring sounded, no one could agree where she had stood."},
    {"archer.shadow",2,"A Folded Range Card","Archer", "The marks are not distances. They record blind corners, broken torchlight, and the places a watcher never thinks to look."},
    {"town.blood",3,"The Red Ledger","Town book","The healer counted each spell in heartbeats. What the page took from her veins, she reclaimed from those who barred the descent."},
    {"shaman.blood",3,"Blood Testament","Blood relic","A ritual written in blood describes a bargain: master magic first, then offer life in place of mana."},
}};
inline const LoreFragment* findLore(const std::string& id) {
    for (const auto& lore:kLoreFragments) if (id==lore.id) return &lore;
    return nullptr;
}
// Profile knowledge is separate from run saves. Stable IDs, no character power.
class Codex {
public:
    bool knows(const std::string& id) const { return fragments_.count(id)!=0; }
    bool discover(const std::string& id) { return findLore(id) && fragments_.insert(id).second; }
    bool reveal(const std::string& tree) { return fragments_.insert("revealed."+tree).second; }
    bool revealed(const std::string& tree) const { return knows("revealed."+tree); }
    std::size_t size() const { return fragments_.size(); }
    int count(int family) const {
        int count=0;
        for (const auto& lore:kLoreFragments) if (lore.family==family && knows(lore.id)) ++count;
        return count;
    }
    const std::string& error() const { return error_; }
    bool load(const std::filesystem::path& path) {
        std::error_code ec;
        auto source=path;
        if (!std::filesystem::exists(source,ec)) {
            if (ec) return fail("Cannot read Codex profile.");
            source=path.string()+".bak";
            if (!std::filesystem::exists(source,ec)) {
                if (ec) return fail("Cannot read Codex backup.");
                return true;
            }
        }
        const auto bytes=std::filesystem::file_size(source,ec);
        if (ec || bytes>65536) return fail("Codex profile is unreadable or too large; original kept.");
        std::ifstream in(source);
        std::string header; std::getline(in,header);
        if (header!="ROGUELIKE_CODEX 1") return fail("Unknown Codex format; original kept.");
        std::set<std::string> loaded;
        std::string id;
        while (std::getline(in,id)) {
            // Keep unknown IDs for forward compatibility, but never display untrusted text.
            if (id.empty() || id.size()>80 || id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._")!=std::string::npos)
                return fail("Damaged Codex profile; original kept.");
            loaded.insert(id);
        }
        if (in.bad()) return fail("Could not finish reading Codex profile; original kept.");
        fragments_=std::move(loaded); return true;
    }
    bool save(const std::filesystem::path& path) {
        if (writeBlocked_) return false;
        const std::filesystem::path temp=path.string()+".tmp", backup=path.string()+".bak";
        std::ofstream out(temp,std::ios::trunc);
        out<<"ROGUELIKE_CODEX 1\n";
        for (const auto& id:fragments_) out<<id<<'\n';
        out.close();
        if (!out) { error_="Codex could not be saved. Knowledge remains in this session; J then S retries."; return false; }
        std::error_code ec;
        const bool existing=std::filesystem::exists(path,ec);
        if (ec) { error_="Cannot access Codex profile; J then S retries."; return false; }
        if (existing) {
            std::filesystem::remove(backup,ec);
            if (!ec) std::filesystem::rename(path,backup,ec);
            if (ec) { error_="Could not preserve Codex backup; J then S retries."; return false; }
        }
        std::filesystem::rename(temp,path,ec);
        if (ec) {
            if (existing) { std::error_code restoreError; std::filesystem::rename(backup,path,restoreError); }
            error_="Could not replace Codex profile; previous knowledge kept in profile or backup. J then S retries.";
            return false;
        }
        error_.clear(); return true;
    }
private:
    bool fail(const char* message) { error_=message; writeBlocked_=true; return false; }
    std::set<std::string> fragments_;
    std::string error_;
    bool writeBlocked_=false; // Never overwrite a damaged or newer profile automatically.
};
} // namespace engine
