#include "core/SoundManager.hpp"

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
