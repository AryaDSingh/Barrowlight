#include "core/SoundManager.hpp"

#include <algorithm>
#include <filesystem>

namespace engine {

namespace {
// Test builds (ROGUELIKE_SILENT, set in CMakeLists.txt) never make a sound.
#ifdef ROGUELIKE_SILENT
constexpr bool kSilent = true;
#else
constexpr bool kSilent = false;
#endif

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

SoundManager::SoundManager() {
    // Every combat family, by file name: slash_0.ogg, slash_1.ogg, crit_gore_0.ogg...
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator("assets/sounds/sfx", error)) {
        if (entry.path().extension() != ".ogg") continue;
        const std::string stem = entry.path().stem().string();
        const auto cut = stem.rfind('_');
        if (cut == std::string::npos) continue;
        auto& buffers = families_[stem.substr(0, cut)];
        buffers.emplace_back();
        if (!buffers.back().loadFromFile(entry.path().string())) buffers.pop_back();
    }
}

void SoundManager::playFamily(const std::string& family, float volume, float pitch) {
    if (kSilent) return;
    const auto found = families_.find(family);
    if (found == families_.end() || found->second.empty()) return;
    const float now = clock_.getElapsedTime().asSeconds();
    if (const auto last = lastPlayed_.find(family); last != lastPlayed_.end() && now - last->second < .045f) return;
    lastPlayed_[family] = now;
    auto& buffers = found->second;
    int variant = std::uniform_int_distribution<int>(0, static_cast<int>(buffers.size()) - 1)(rng_);
    if (buffers.size() > 1 && variant == lastVariant_[family]) variant = (variant + 1) % static_cast<int>(buffers.size());
    lastVariant_[family] = variant;
    // A free voice, or a new one, or (all 24 busy) the oldest.
    sf::Sound* voice = nullptr;
    for (auto& v : voices_) if (v->getStatus() == sf::SoundSource::Status::Stopped) { voice = v.get(); break; }
    if (!voice && voices_.size() < 24) { voices_.push_back(std::make_unique<sf::Sound>(buffers[static_cast<std::size_t>(variant)])); voice = voices_.back().get(); }
    if (!voice) { voice = voices_[nextVoice_ % voices_.size()].get(); ++nextVoice_; voice->stop(); }
    voice->setBuffer(buffers[static_cast<std::size_t>(variant)]);
    voice->setPitch(pitch * std::uniform_real_distribution<float>(.94f, 1.06f)(rng_));
    voice->setVolume(std::clamp(volume, 0.f, 100.f));
    voice->play();
}

void SoundManager::playHit(HitSound sound, bool critical, bool targetBleeds) {
    playFamily(hitFamily(sound), critical ? 100.f : 85.f);
    if (critical) playFamily(critFamily(sound, targetBleeds), 100.f, .93f);
}

void SoundManager::playDodge() {
    playFamily("dodge", 90.f);
}

void SoundManager::playVoice(const char* kind, const char* event) {
    const std::string family = std::string("voice_") + kind + "_" + event;
    const float now = clock_.getElapsedTime().asSeconds();
    const bool dying = std::string(event) == "death";
    // Grunts give way to anything else the same kind says for a moment; deaths always sound.
    if (const auto last = lastPlayed_.find("voice_" + std::string(kind)); !dying && last != lastPlayed_.end() && now - last->second < .4f) return;
    lastPlayed_["voice_" + std::string(kind)] = now;
    playFamily(family, dying ? 85.f : std::string(event) == "hurt" ? 60.f : 75.f);
}

void SoundManager::setMusic(MusicTrack track) {
    if (kSilent || track == target_) return;
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
        case SoundEffect::Hit: playFamily("blunt", 85.f); break;
        case SoundEffect::Death: playFamily("death"); break;
        case SoundEffect::LevelUp: playFamily("levelup", 90.f); break;
        case SoundEffect::Dodge: playDodge(); break;
        case SoundEffect::Select: break;
    }
}

} // namespace engine
