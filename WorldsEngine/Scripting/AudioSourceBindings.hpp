#include "Export.hpp"
#include <Audio/Audio.hpp>

using namespace worlds;

extern "C" {
    EXPORT void audiosource_start(entt::registry* reg, entt::entity entity) {
        reg->get<AudioSource>(entity).eventInstance->start();
    }

    EXPORT void audiosource_stop(entt::registry* reg, entt::entity entity, FMOD_STUDIO_STOP_MODE stopMode) {
        reg->get<AudioSource>(entity).eventInstance->stop(stopMode);
    }

    EXPORT FMOD_STUDIO_PLAYBACK_STATE audiosource_getPlayState(entt::registry* reg, entt::entity entity) {
        FMOD_STUDIO_PLAYBACK_STATE state;

        reg->get<AudioSource>(entity).eventInstance->getPlaybackState(&state);

        return state;
    }

    EXPORT void audiosource_setParameter(entt::registry* reg, entt::entity entity, const char* parameterName, float value) {
        reg->get<AudioSource>(entity).eventInstance->setParameterByName(parameterName, value);
    }
}
