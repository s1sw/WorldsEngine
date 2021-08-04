#include "Export.hpp"
#include "Audio/Audio.hpp"

using namespace worlds;

extern "C" {
    EXPORT void audio_playOneShot(AssetID clipId, glm::vec3* location, bool spatialise, float volume, MixerChannel channel) {
        AudioSystem::getInstance()->playOneShotClip(clipId, *location, spatialise, volume, channel);
    }
}
