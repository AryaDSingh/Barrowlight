#pragma once

#include <SFML/Audio.hpp>

namespace engine {

// Which sound effect to play -- see SoundManager::play(). One entry per
// generated WAV file in assets/sounds/ (see
// ARCHITECTURE_DECISIONS.md, "Sound effects," for why they're
// synthesized rather than sourced from an external library).
enum class SoundEffect {
    Hit,     // a talent or attack lands (either side, on either side of the fight)
    Death,   // the player or a monster dies
    LevelUp, // the player levels up
    Dodge,   // an attack was dodged -- either side
    Select,  // a UI choice: class selection, the AbilityChoice screen
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
// One sf::Sound per effect, not a pool of interchangeable ones: this is
// a turn-based game, not real-time action, so sound events are
// naturally spaced out enough that re-triggering the same effect's
// single Sound instance (restarting it if it happens to still be
// playing) is entirely adequate -- deliberately simpler than building a
// pool for overlapping playback this project doesn't need.
class SoundManager {
public:
    SoundManager();

    void play(SoundEffect effect);

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

    sf::SoundBuffer hitBuffer_;
    sf::SoundBuffer deathBuffer_;
    sf::SoundBuffer levelUpBuffer_;
    sf::SoundBuffer dodgeBuffer_;
    sf::SoundBuffer selectBuffer_;

    sf::Sound hitSound_;
    sf::Sound deathSound_;
    sf::Sound levelUpSound_;
    sf::Sound dodgeSound_;
    sf::Sound selectSound_;
};

} // namespace engine
