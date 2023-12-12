#include <iostream>
#include <vector>
#include <portaudio.h>
#include <sndfile.h>

// Define constants for audio settings
const int SAMPLE_RATE = 24000;
const int CHANNELS = 1;
const int FRAMES_PER_BUFFER = 256;

#include <mutex>
#include <condition_variable>
#include <thread>
#include "openai-reduced.hpp"


// Function to initialize PortAudio
PaError initPortAudio(PaStream** playback_stream) {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio initialization error: " << Pa_GetErrorText(err) << std::endl;
    }
    return err;
}

// Define PortAudio callback function to play audio
static int audioCallback(const void* inputBuffer, void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData) {
    SNDFILE* sndfile = static_cast<SNDFILE*>(userData);
    sf_count_t bytesRead = sf_readf_float(sndfile, static_cast<float*>(outputBuffer), framesPerBuffer);
    if (bytesRead < framesPerBuffer) {
        // End of file, stop playback
        return paComplete;
    }
    return paContinue;
}

// Function to play an audio file using libsndfile and PortAudio
void playAudioFile(const char* filePath) {
    SF_INFO sfinfo;
    SNDFILE* sndfile = sf_open(filePath, SFM_READ, &sfinfo);
    if (!sndfile) {
        std::cout << "Failed to open file for read " << sf_strerror(sndfile) << std::endl;
        return;
    }

    // Initialize PortAudio
    PaStream* stream = nullptr;
    PaError err = initPortAudio(&stream);
    if (err != paNoError) {
        sf_close(sndfile);
        return;
    }

    // Open PortAudio stream
    err = Pa_OpenDefaultStream(&stream, 0, sfinfo.channels, paFloat32, SAMPLE_RATE, FRAMES_PER_BUFFER, audioCallback, sndfile);
    if (err != paNoError) {
        std::cerr << "PortAudio stream open error: " << Pa_GetErrorText(err) << std::endl;
        Pa_Terminate();
        sf_close(sndfile);
        return;
    }

    // Start PortAudio stream for playback
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "PortAudio stream start error: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(stream);
        Pa_Terminate();
        sf_close(sndfile);
        return;
    }

    // Wait for user input to stop playback (you can implement your own stop mechanism)
    std::cout << "Press Enter to stop playback..." << std::endl;
    std::cin.get();

    // Stop and close PortAudio stream
    err = Pa_StopStream(stream);
    if (err != paNoError) {
        std::cerr << "PortAudio stream stop error: " << Pa_GetErrorText(err) << std::endl;
        sf_close(sndfile);
        Pa_Terminate();
        return;
    }

    err = Pa_CloseStream(stream);
    if (err != paNoError) {
        std::cerr << "PortAudio stream close error: " << Pa_GetErrorText(err) << std::endl;
        sf_close(sndfile);
        Pa_Terminate();
        return;
    }

    // Terminate PortAudio and close the audio file
    Pa_Terminate();
}


