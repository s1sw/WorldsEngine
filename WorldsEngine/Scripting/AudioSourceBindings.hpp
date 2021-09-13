#include "Export.hpp"
#include <Audio/Audio.hpp>

using namespace worlds;

extern "C" {
    EXPORT AssetID audiosource_getClipId(entt::registry* reg, entt::entity ent) {
        return reg->get<AudioSource>(ent).clipId;
    }

    EXPORT void audiosource_setClipId(entt::registry* reg, entt::entity ent, AssetID id) {
        AudioSystem::getInstance()->precacheAudioClip(id);
        reg->get<AudioSource>(ent).clipId = id;
    }

    EXPORT bool audiosource_getIsPlaying(entt::registry* reg, entt::entity ent) {
        return reg->get<AudioSource>(ent).isPlaying;
    }

    EXPORT void audiosource_setIsPlaying(entt::registry* reg, entt::entity ent, bool val) {
        reg->get<AudioSource>(ent).isPlaying = val;
    }

    EXPORT float audiosource_getVolume(entt::registry* reg, entt::entity ent) {
        return reg->get<AudioSource>(ent).volume;
    }

    EXPORT void audiosource_setVolume(entt::registry* reg, entt::entity ent, float vol) {
        reg->get<AudioSource>(ent).volume = vol;
    }

    EXPORT bool audiosource_getLooping(entt::registry* reg, entt::entity ent) {
        return reg->get<AudioSource>(ent).loop;
    }

    EXPORT void audiosource_setLooping(entt::registry* reg, entt::entity ent, bool val) {
        reg->get<AudioSource>(ent).loop = val;
    }
}
