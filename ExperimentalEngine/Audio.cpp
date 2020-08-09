#include <SDL2/SDL_audio.h>
#include "Log.hpp"
int playbackSamples;

double dspTime = 0.0;

void audioCallback(void* userData, uint8_t* u8stream, int len) {
    float* stream = reinterpret_cast<float*>(u8stream);
    double sampleLength = 1.0 / 44100.0;

    for (int i = 0; i < len / sizeof(float); i++) {
        //stream[i] = (float)i / (float)len;
        double time = dspTime + (i * sampleLength);
        //stream[i] = (sin(time * 2.0 * glm::pi<double>() * 440.0) > 0.0 ? 1.0 : -1.0) * 0.2;
    }

    dspTime += (double)(len / 4) / 44100.0;
}

void setupAudio() {
    int numAudioDevices = SDL_GetNumAudioDevices(0);

    if (numAudioDevices == -1) {
        logErr("Failed to enumerate audio devices");
    }

    for (int i = 0; i < numAudioDevices; i++) {
        logMsg("Found audio device: %s", SDL_GetAudioDeviceName(i, 0));
    }

    int numCaptureDevices = SDL_GetNumAudioDevices(1);

    if (numCaptureDevices == -1) {
        logErr("Failed to enumerate capture devices");
    }

    for (int i = 0; i < numCaptureDevices; i++) {
        logMsg("Found capture device: %s", SDL_GetAudioDeviceName(i, 1));
    }

    SDL_AudioSpec desired;
    desired.channels = 1;
    desired.format = AUDIO_F32;
    desired.freq = 44100;
    desired.samples = 1024 * desired.channels;
    desired.callback = audioCallback;

    SDL_AudioSpec obtained;
    int pbDev = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);

    logMsg("Obtained samples %i", obtained.samples);
    playbackSamples = obtained.samples;

    SDL_PauseAudioDevice(pbDev, 0);
}