#pragma once

#include <string>
#include <vector>

namespace engine {

// Lore: the things you find and read (G) on the ground. Each one is kept in
// your journal (J). `opens` names what it opened, if anything.
struct LoreEntry { const char* id; const char* title; std::vector<const char*> text; const char* opens; };

inline const std::vector<LoreEntry>& loreEntries() {
    static const std::vector<LoreEntry> entries{
        {"warlord_standard", "The Warlord's standard",
         {"Torn goblin silk on a broken spear, stitched under the Warlord's mark:", "\"Plant it where you stand. Let them break on it.\""}, "Warbanner"},
        {"foreman_key", "The Foreman's key",
         {"An iron key on a chain, stamped with a hammer and a flame."}, "the Ashen Foundry"},
        {"forgemaster_brand", "The Forgemaster's brand",
         {"A branding iron, still hot, its mark a hammer inside a flame. The heat runs up your arm and stays."}, "Forgeborn"},
        {"slag_formula", "A slag-scrawled formula",
         {"A slab of cooled slag, scratched with a smith's formula: how to wake the slag, and how to make it stand."}, "Slagcaller"},
        {"chorister_hymn", "The Drowned Chorister's hymn",
         {"Water-stained notes of a hymn no living throat could sing. Reading them, you hear the thunder in it."}, "Tempest"},
        {"bonecaller_journal", "A Bonecaller's journal",
         {"A journal bound in skin: how bones remember their shape, and how to ask them to take a new one."}, "Bonewright"},
        {"acolyte_catechism", "A Frost Acolyte's catechism",
         {"A catechism of the cold, its pages stiff with frost: the winter does not end, it only waits."}, "Rimeheart"},
        {"hollow_map", "A map drawn on bone",
         {"A map scratched into a flat bone: a way down to a cloister the forest swallowed."}, "Thornwood Hollow"},
        {"witch_seed", "A Rot Witch's seed",
         {"A black seed, still warm. Hold it and you can feel the thorns wanting to grow."}, "Briarheart"},
        {"hound_collar", "A braided hound collar",
         {"A collar of braided thorn-bark, worn smooth. Someone kept these hounds once."}, "Packmaster"},
    };
    return entries;
}

inline const LoreEntry* loreEntry(const std::string& id) {
    for (const auto& e : loreEntries()) if (id == e.id) return &e;
    return nullptr;
}

} // namespace engine
