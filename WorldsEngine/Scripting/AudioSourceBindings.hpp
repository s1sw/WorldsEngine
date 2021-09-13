#include "Export.hpp"
#include <Audio/Audio.hpp>

using namespace worlds;

extern "C" {
    AssetID audiosource_getClipId(entt::registry* reg, entt::entity ent) {
        return reg->get<AudioSource>(ent).clipId;
    }

    void audiosource_setClipId(entt::registry* reg, entt::entity ent, AssetID id) {
        reg->get<AudioSource>(ent).clipId = id;
    }

    bool audiosource_getIsPlaying(entt::registry* reg, entt::entity ent) {
        return reg->get<AudioSource>(ent).isPlaying;
    }

    void audiosource_setIsPlaying(entt::registry* reg, entt::entity ent, bool val) {
        reg->get<AudioSource>(ent).isPlaying = val;
    }

    float audiosource_getVolume(entt::registry* reg, entt::entity ent) {
        return reg->get<AudioSource>(ent).volume;
    }

    void audiosource_setVolume(entt::registry* reg, entt::entity ent, float vol) {
        reg->get<AudioSource>(ent).volume = vol;
    }
}
