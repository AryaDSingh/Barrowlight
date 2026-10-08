#pragma once

namespace engine {

// Gods with conducts, after Dungeon Crawl's gods. You swear to one at a shrine. Each wants
// certain deeds and hates others; favor rises and falls with them. At 30
// favor its boon is yours, at 60 you can pray (spending 40), and at -20 its
// wrath falls on you. Saved as an int, so only ever append.
enum class Patron { None = 0, Seraph = 1, Sleeper = 2, AshSaint = 3, Whisperer = 4 };
inline constexpr int kPatronCount = 4;
inline constexpr int kFavorBoon = 30, kFavorPrayer = 60, kPrayerCost = 40, kFavorWrath = -20, kFavorMax = 100, kFavorMin = -50;
inline constexpr int kSwornFavor = 10, kCommuneFavor = 15;

struct PatronInfo {
    const char* name;  // "The Seraph of the Last Dawn"
    const char* short_; // "the Seraph", for sentences
    const char* kind;
    const char* likes;
    const char* hates;
    const char* boon;
    const char* prayer;
    const char* wrath;
    int r, g, b;       // the colour of its shrine's light
};

inline const PatronInfo& patronInfo(Patron p) {
    static const PatronInfo none{"", "", "", "", "", "", "", "", 255, 210, 120};
    static const PatronInfo gods[kPatronCount]{
        {"The Seraph of the Last Dawn", "the Seraph", "Angelic",
         "Slaying undead and creatures of the dark; any kill made by your own light.",
         "Shadow and Blood magic; shuttering your light.",
         "Your light burns a tile further, and you deal +3 damage to undead and darkvision creatures.",
         "Wings of Mercy: heal 40% of your life, shed ailments and curses, and stand guarded (Guard 4).",
         "Judgement: your light is taken from you and you are scorched.",
         255, 236, 170},
        {"That Which Sleeps Below", "the Sleeper", "Eldritch",
         "Kills made while you stand in water or blood.",
         "Fire and Radiance.",
         "Standing in water or blood heals you 2 life a turn.",
         "Call the Deep: dark water floods out three tiles and chills every enemy in it.",
         "Its dreams turn on you: Doom.",
         90, 150, 200},
        {"The Ash Saint", "the Ash Saint", "Zealot",
         "Setting things alight; slaying the burning.",
         "Ice.",
         "Burning ground no longer sets you alight, and you deal +2 damage to burning enemies.",
         "Pyre: every enemy within three tiles bursts into flame.",
         "Penance: you burn.",
         255, 120, 50},
        {"The Whisperer in the Walls", "the Whisperer", "Eldritch",
         "Kills made from hiding or from the dark.",
         "Light magic; being spotted.",
         "You dodge 10% more while standing in the dark.",
         "Unseeing: you vanish, and everything within six tiles is blinded.",
         "It tells them where you are: every enemy nearby is alerted, and you are Marked.",
         160, 110, 230},
    };
    const int i = static_cast<int>(p);
    return i >= 1 && i <= kPatronCount ? gods[i - 1] : none;
}

} // namespace engine
