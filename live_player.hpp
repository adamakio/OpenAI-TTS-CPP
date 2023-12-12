#include <iostream>
#include <vector>
#include <portaudio.h>
#include <curl/curl.h>

#define MINIMP3_IMPLEMENTATION
#include <minimp3/minimp3.h>
#include <minimp3/minimp3_ex.h>
#include "nlohmann/json.hpp"

// Constants for PortAudio
const int SAMPLE_RATE = 24000;
const int CHANNELS = 1;
const int FRAME_SIZE = 256;

// PortAudio stream
PaStream* stream;

// Define a structure to hold our decoded audio data
struct AudioData {
    std::vector<mp3d_sample_t> samples;
    size_t readIndex = 0;
    mp3dec_t mp3d;
    mp3dec_file_info_t info;

    AudioData() {
        mp3dec_init(&mp3d);
        info.channels = CHANNELS;
        info.hz = SAMPLE_RATE;
    }
};


static int livePaCallback(const void* inputBuffer, void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData) {
    AudioData* data = static_cast<AudioData*>(userData);
    mp3d_sample_t* out = static_cast<mp3d_sample_t*>(outputBuffer);
    size_t framesLeft = data->samples.size() - data->readIndex;

    size_t framesToCopy = framesLeft < framesPerBuffer ? framesLeft : framesPerBuffer;
    if (framesToCopy > 0) {
        std::memcpy(out, data->samples.data() + data->readIndex, framesToCopy);
        data->readIndex += framesToCopy;
    }

    return framesToCopy < framesPerBuffer ? paComplete : paContinue;
}



static size_t writeMP3Data(void* buffer, size_t size, size_t nmemb, void* userData) {
    size_t bufferSize = size * nmemb;

    AudioData* audioData = static_cast<AudioData*>(userData);

    // Decode MP3 data
    int res = mp3dec_load_buf(&audioData->mp3d, static_cast<uint8_t*>(buffer), bufferSize, &audioData->info, NULL, NULL);
    if (res != 0) {
        std::cerr << "Error decoding MP3. Error code: " << res << std::endl;
        return 0;
    }

    audioData->samples.insert(audioData->samples.end(), audioData->info.buffer, audioData->info.buffer + audioData->info.samples);

    free(audioData->info.buffer);
    return bufferSize;
}


int live_player_main() {
    std::cout << "Starting live player..." << std::endl;
    CURL* curl;
    CURLcode res;
    AudioData audioData;
    std::string token{ "sk- .. " };

    // JSON data setup using nlohmann::json
    nlohmann::json jsonData = {
        {"model", "tts-1-hd"},
        {"input", "I fucking love c plus plus. It is the best language in the world."},
        {"voice", "alloy"}
    };

    // Convert JSON object to string
    std::string postData = jsonData.dump();

    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    std::cout << "Performing libcurl request..." << std::endl;

    // Initialize PortAudio
    Pa_Initialize();
    PaStream* stream;

    // Set libcurl options
    if (curl) {
        // Set the headers
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, std::string{ "Authorization: Bearer " + token }.c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Set the URL and callback function
        const char* url = "https://api.openai.com/v1/audio/speech";
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeMP3Data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &audioData);

        std::cout << "<< request: " << url << std::endl;

        // Perform the libcurl request
        res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

        std::cout << ">> response: " << res << std::endl;

        // Clean up
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();

    std::cin.get();
    // Setup and start the PortAudio stream
    Pa_OpenDefaultStream(&stream, 0, CHANNELS, paInt16, SAMPLE_RATE, FRAME_SIZE, livePaCallback, &audioData);
    Pa_StartStream(stream);

    while (Pa_IsStreamActive(stream)) {
		Pa_Sleep(100);
	}
    std::cout << "Stream is complete." << std::endl;

    // Clean up PortAudio
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    return 0;
}
