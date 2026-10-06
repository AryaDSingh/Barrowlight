#pragma once

#include <deque>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include <SFML/Audio.hpp>

#include "core/CombatSounds.hpp"

namespace engine {

// The game's non-combat moments -- see SoundManager::play(). Each is a
// family in assets/sounds/sfx/ (assets/sounds/CREDITS.txt).
enum class SoundEffect {
    Hit,     // a plain blow: striking a landmark
    Death,   // the player dies
    LevelUp, // the player levels up, or gains a power
    Dodge,   // an attack was dodged -- either side
    Select,  // a UI choice: silent, the click is enough
};

// Background music (assets/music/CREDITS.txt). Each track loops; a change
// of track crossfades.
enum class MusicTrack { None, Title, Town, Barracks, Sanctum, Crypts, Boss };

// Owns every SoundBuffer/Sound pair the game uses -- the only class
// besides Application itself that touches SFML::Audio directly,
// mirroring how Application.cpp has been the only place touching
// SFML::Graphics/Window since Prompt 0. Loads every sound file once at
// construction.
//
// play() is silently a no-op if a buffer failed to load (a missing
// file, say) or if the platform has no audio device at all -- confirmed
// directly during development, not assumed, that miniaudio (which
// SFML's audio module is built on) degrades gracefully in that case,
// logging to stderr rather than throwing or crashing. Sound is a
// presentation detail; the game must stay fully playable with it
// silent, never a hard requirement anything else depends on.
//
// Every sound plays through a small pool of voices, so a hit, its crit
// layer and a level-up can overlap.
class SoundManager {
public:
    SoundManager();

    void play(SoundEffect effect);

    // Combat sounds from assets/sounds/sfx/<family>_<n>.ogg: a random
    // variant (never the same one twice running) at a slightly varied pitch.
    // The same family twice within a few hundredths of a second plays once,
    // so a spell striking six foes doesn't stack into one deafening blast.
    void playFamily(const std::string& family, float volume = 100.f, float pitch = 1.f);
    // A landed hit: the family's own sound, and on a critical its heavier layer.
    void playHit(HitSound sound, bool critical, bool targetBleeds);
    void playDodge();
    // A monster's voice: kind from monsterVoice(), event "alert", "hurt" or
    // "death". One voice of each kind at a time, so a pack shouts once.
    void playVoice(const char* kind, const char* event);
    bool hasFamily(const std::string& family) const { return families_.count(family) > 0; }

    // Music: setMusic() picks the track to fade to (a no-op if it's
    // already the one playing); updateMusic() advances the crossfade and
    // must be called every frame. Streams from disk; a missing file or
    // no audio device simply stays silent.
    void setMusic(MusicTrack track);
    void updateMusic(float seconds);
    void toggleMusic();
    bool musicEnabled() const { return musicEnabled_; }

private:
    sf::Music decks_[2];
    MusicTrack deckTrack_[2]{MusicTrack::None, MusicTrack::None};
    float deckVolume_[2]{0.f, 0.f}; // 0..1 of the music volume
    int front_ = 0;                  // the deck fading in / playing
    MusicTrack target_ = MusicTrack::None;
    bool musicEnabled_ = true;

    // Combat families. Buffers live in deques so their addresses never move
    // while a voice is playing them.
    std::map<std::string, std::deque<sf::SoundBuffer>> families_;
    std::map<std::string, int> lastVariant_;
    std::map<std::string, float> lastPlayed_;
    std::vector<std::unique_ptr<sf::Sound>> voices_;
    std::size_t nextVoice_ = 0;
    sf::Clock clock_;
    std::mt19937 rng_{std::random_device{}()}; // its own: sound never touches combat randomness
};

} // namespace engine
