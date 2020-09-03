#include <SDL2/SDL_audio.h>
#include "Log.hpp"
#include "Audio.hpp"
#include <glm/glm.hpp>
#include <glm/ext/scalar_constants.hpp>

namespace worlds {
    int playbackSamples;

    double dspTime = 0.0;

    double squarify(double in) {
        return in > 0.0 ? 1.0 : -1.0;
    }

    double sineWave(double time, double freq) {
        return sin(time * 2.0 * glm::pi<double>() * freq);
    }

    double remapToZeroOne(double in) {
        return (in + 1.0) * 0.5;
    }

    void audioCallback(void* userData, uint8_t* u8stream, int len) {
        float* stream = reinterpret_cast<float*>(u8stream);
        double sampleLength = 1.0 / 44100.0;

        for (int i = 0; i < len / sizeof(float); i++) {
            //stream[i] = (float)i / (float)len;
            double time = dspTime + (i * sampleLength);
            //stream[i] = sineWave(time, 440.0) * remapToZeroOne(sineWave(time, 1.0));
        }

        dspTime += (double)(len / 4) / 44100.0;
    }

    void setupAudio() {
        int numAudioDevices = SDL_GetNumAudioDevices(0);

        if (numAudioDevices == -1) {
            logErr(WELogCategoryAudio, "Failed to enumerate audio devices");
        }

        for (int i = 0; i < numAudioDevices; i++) {
            logMsg(WELogCategoryAudio, "Found audio device: %s", SDL_GetAudioDeviceName(i, 0));
        }

        int numCaptureDevices = SDL_GetNumAudioDevices(1);

        if (numCaptureDevices == -1) {
            logErr(WELogCategoryAudio, "Failed to enumerate capture devices");
        }

        for (int i = 0; i < numCaptureDevices; i++) {
            logMsg(WELogCategoryAudio, "Found capture device: %s", SDL_GetAudioDeviceName(i, 1));
        }

        SDL_AudioSpec desired;
        desired.channels = 1;
        desired.format = AUDIO_F32;
        desired.freq = 44100;
        desired.samples = 1024 * desired.channels;
        desired.callback = audioCallback;

        SDL_AudioSpec obtained;
        int pbDev = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);

        logMsg(WELogCategoryAudio, "Obtained samples %i", obtained.samples);
        playbackSamples = obtained.samples;

        SDL_PauseAudioDevice(pbDev, 0);
    }
}