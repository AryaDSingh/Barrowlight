#include "core/SoundManager.hpp"

#include <algorithm>
#include <tuple>

namespace engine {

namespace {
// Same "relative to launch directory" convention kFontPath already
// established (Prompt 13) -- these assets ship alongside the
// executable, not embedded in it.
constexpr const char* kHitPath = "assets/sounds/hit.wav";
constexpr const char* kDeathPath = "assets/sounds/death.wav";
constexpr const char* kLevelUpPath = "assets/sounds/levelup.wav";
constexpr const char* kDodgePath = "assets/sounds/dodge.wav";
constexpr const char* kSelectPath = "assets/sounds/select.wav";

constexpr float kMusicVolume = 32.f;   // of 100: under the sound effects
constexpr float kFadeSeconds = 1.4f;

const char* musicPath(MusicTrack track) {
    switch (track) {
        case MusicTrack::Title: return "assets/music/title.mp3";
        case MusicTrack::Town: return "assets/music/town.mp3";
        case MusicTrack::Barracks: return "assets/music/barracks.ogg";
        case MusicTrack::Sanctum: return "assets/music/sanctum.ogg";
        case MusicTrack::Crypts: return "assets/music/crypts.ogg";
        case MusicTrack::Boss: return "assets/music/boss.ogg";
        case MusicTrack::None: break;
    }
    return nullptr;
}
} // namespace

SoundManager::SoundManager()
    : hitSound_(hitBuffer_),
      deathSound_(deathBuffer_),
      levelUpSound_(levelUpBuffer_),
      dodgeSound_(dodgeBuffer_),
      selectSound_(selectBuffer_) {
    // Each Sound above is constructed bound to its buffer *before* the
    // buffer has any real data loaded into it -- Sound holds a
    // reference to the SoundBuffer object itself, not a snapshot of its
    // data at construction time, so loading the actual file content
    // here afterward is fine; play() reads whatever the buffer holds at
    // that later point in time. A failed load leaves that buffer empty
    // (0 duration, no samples) -- play() on an empty buffer is a
    // harmless no-op, not a crash, so no failure handling is needed
    // beyond just not crashing on a missing file.
    // Prompt 25: loadFromFile()'s bool return is deliberately discarded
    // here, not left as an accidental compiler warning -- a failed load
    // (missing file, say) leaves that buffer empty, and play() on an
    // empty buffer is already a harmless no-op (see this class's own
    // header comment), so there's genuinely nothing to react to on
    // failure beyond not crashing, which already holds either way.
    std::ignore = hitBuffer_.loadFromFile(kHitPath);
    std::ignore = deathBuffer_.loadFromFile(kDeathPath);
    std::ignore = levelUpBuffer_.loadFromFile(kLevelUpPath);
    std::ignore = dodgeBuffer_.loadFromFile(kDodgePath);
    std::ignore = selectBuffer_.loadFromFile(kSelectPath);
}

void SoundManager::setMusic(MusicTrack track) {
    if (track == target_) return;
    target_ = track;
    // The current deck fades out; the other one loads the new track and fades in.
    const int next = 1 - front_;
    decks_[next].stop();
    deckTrack_[next] = MusicTrack::None;
    if (const char* path = musicPath(track); path && decks_[next].openFromFile(path)) {
        decks_[next].setLooping(true);
        decks_[next].setVolume(0.f);
        deckVolume_[next] = 0.f;
        deckTrack_[next] = track;
        if (musicEnabled_) decks_[next].play();
    }
    front_ = next;
}

void SoundManager::updateMusic(float seconds) {
    const float step = seconds / kFadeSeconds;
    for (int deck = 0; deck < 2; ++deck) {
        const bool rising = deck == front_ && deckTrack_[deck] != MusicTrack::None;
        deckVolume_[deck] = std::clamp(deckVolume_[deck] + (rising ? step : -step), 0.f, 1.f);
        decks_[deck].setVolume(musicEnabled_ ? deckVolume_[deck] * kMusicVolume : 0.f);
        if (!rising && deckVolume_[deck] <= 0.f && decks_[deck].getStatus() != sf::SoundSource::Status::Stopped) {
            decks_[deck].stop();
            deckTrack_[deck] = MusicTrack::None;
        }
    }
}

void SoundManager::toggleMusic() {
    musicEnabled_ = !musicEnabled_;
    auto& deck = decks_[front_];
    if (deckTrack_[front_] == MusicTrack::None) return;
    if (musicEnabled_) deck.play(); else deck.pause();
}

void SoundManager::play(SoundEffect effect) {
    switch (effect) {
        case SoundEffect::Hit:
            hitSound_.play();
            break;
        case SoundEffect::Death:
            deathSound_.play();
            break;
        case SoundEffect::LevelUp:
            levelUpSound_.play();
            break;
        case SoundEffect::Dodge:
            dodgeSound_.play();
            break;
        case SoundEffect::Select:
            selectSound_.play();
            break;
    }
}

} // namespace engine
