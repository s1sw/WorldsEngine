#include "Audio/Audio.hpp"
#include "Export.hpp"

using namespace worlds;

extern "C"
{
    EXPORT void audio_playOneShot(AssetID clipId, glm::vec3 *location, bool spatialise, float volume,
                                  MixerChannel channel)
    {
        AudioSystem::getInstance()->playOneShotClip(clipId, *location, spatialise, volume, channel);
    }

    EXPORT void audio_playOneShotEvent(const char *eventPath, glm::vec3 *location, float volume)
    {
        AudioSystem::getInstance()->playOneShotEvent(eventPath, *location, volume);
    }

    EXPORT void audio_playOneShotAttachedEvent(const char *eventPath, glm::vec3 *location, entt::entity entity,
                                               float volume)
    {
        AudioSystem::getInstance()->playOneShotAttachedEvent(eventPath, *location, entity, volume);
    }

    EXPORT void audio_loadBank(const char *bankPath)
    {
        AudioSystem::getInstance()->loadBank(bankPath);
    }

    EXPORT void audio_stopEverything(entt::registry *reg)
    {
        AudioSystem::getInstance()->stopEverything(*reg);
    }

    EXPORT void audio_updateAudioScene(entt::registry *reg)
    {
        AudioSystem::getInstance()->updateAudioScene(*reg);
    }
}
