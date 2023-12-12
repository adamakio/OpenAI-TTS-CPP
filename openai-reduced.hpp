#ifndef OPENAI_REDUCED_HPP_
#define OPENAI_REDUCED_HPP_

#include <iostream>
#include <stdexcept>
#include <string>
#include <mutex>
#include <fstream>
#include <queue>

#include <condition_variable>

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <opus/opus.h>
#include <ogg/ogg.h>
#include <portaudio.h>

#include "ChatStructures.hpp"

#define DEBUG 0

// Define constants for audio settings
const int SAMPLE_RATE = 24000;
const int CHANNELS = 1;
const int FRAMES_PER_BUFFER = 960;

namespace openai {
    OpusDecoder* opusDecoder = nullptr;
    int opusError;
    
    class AudioBuffer {
    private:
        std::queue<float> buffer;
        std::mutex mutex;

    public:
        void addData(const float* data, size_t size) {
            std::lock_guard<std::mutex> lock(mutex);
            for (size_t i = 0; i < size; ++i) {
                buffer.push(data[i]);
            }
        }

        size_t getData(float* output, size_t framesPerBuffer) {
            std::lock_guard<std::mutex> lock(mutex);
            size_t i = 0;
            for (; i < framesPerBuffer && !buffer.empty(); ++i) {
                output[i] = buffer.front();
                buffer.pop();
            }
            return i; // Number of frames read
        }

        bool isEmpty() const {
            return buffer.empty();
        }
    };

    struct SharedData {
        FILE* file;
        std::atomic<bool> dataReady;
        AudioBuffer audioBuffer;

        ogg_sync_state oy;          // Ogg sync state, for syncing with the Ogg stream
        ogg_stream_state os;        // Ogg stream state, for handling logical streams
        ogg_page og;                // Ogg page, a single unit of data in an Ogg stream
        ogg_packet op;              // Ogg packet, contains encoded Opus data
        OpusDecoder* opusDecoder;   // Opus decoder state
        int opusError;              // Error code returned by Opus functions
        bool oggInitialized;        // Flag to track if Ogg and Opus have been initialized
        int serial_number;          // Serial number for the Ogg stream

        // Constructor
        SharedData(FILE* file) : file(file), dataReady(false), opusDecoder(nullptr), opusError(OPUS_OK), oggInitialized(false), serial_number(-1) {
            // Initialize the Ogg sync state
            ogg_sync_init(&oy);
        }

        // Destructor
        ~SharedData() {
            cleanup();
        }

        void initOpusDecoder() {
            if (!opusDecoder) {
                opusDecoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &opusError);
                if (opusError != OPUS_OK) {
                    throw std::runtime_error("Failed to create Opus decoder: " + std::string(opus_strerror(opusError)));
                }
            }
        }

        void cleanup() {
            if (opusDecoder) {
                opus_decoder_destroy(opusDecoder);
                opusDecoder = nullptr;
            }
            if (oggInitialized) {
                ogg_stream_clear(&os);
                ogg_sync_clear(&oy);
                oggInitialized = false;
            }
        }

        // Initialize the Ogg stream state; should be called when a new stream is detected
        void initOggStream(int serial) {
            if (ogg_stream_init(&os, serial) != 0) {
                throw std::runtime_error("Failed to initialize Ogg stream state.");
            }
            serial_number = serial;
            oggInitialized = true;
        }

        // Reset the Ogg stream state; should be called for a new logical stream
        void resetOggStream() {
            if (oggInitialized) {
                ogg_stream_clear(&os);
            }
            if (serial_number != -1) {
                initOggStream(serial_number);
            }
        }
    };


    // Define PortAudio callback function to play audio
    static int audioCallback(const void* inputBuffer, void* outputBuffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags,
        void* userData) {
        SharedData* sharedData = static_cast<SharedData*>(userData);

        float* out = static_cast<float*>(outputBuffer);
        std::fill(out, out + framesPerBuffer * CHANNELS, 0.0f); // Fill buffer with silence

        if (!sharedData->dataReady) {
            // No data available yet, just play silence
            return paContinue;
        }

        size_t bytesRead = sharedData->audioBuffer.getData(out, framesPerBuffer * CHANNELS);
        if (bytesRead < framesPerBuffer * CHANNELS) {
            // Buffer underflow, not enough data available
            sharedData->dataReady = false; // Wait for more data
        }

        return paContinue;
    }

    // Function to play an audio file using libsndfile and PortAudio
    void playAudio(SharedData* shared_data) {
        // Initialize PortAudio
        PaStream* stream = nullptr;
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "PortAudio initialization error: " << Pa_GetErrorText(err) << std::endl;
            return;
        }

        // Open PortAudio stream
        err = Pa_OpenDefaultStream(&stream, 0, CHANNELS, paFloat32, SAMPLE_RATE, FRAMES_PER_BUFFER, audioCallback, shared_data);
        if (err != paNoError) {
            std::cerr << "PortAudio stream open error: " << Pa_GetErrorText(err) << std::endl;
            Pa_Terminate();
            return;
        }

        // Start PortAudio stream for playback
        err = Pa_StartStream(stream);
        if (err != paNoError) {
            std::cerr << "PortAudio stream start error: " << Pa_GetErrorText(err) << std::endl;
            Pa_CloseStream(stream);
            Pa_Terminate();
            //sf_close(sndfile);
            return;
        }

        // Wait for user input to stop playback (you can implement your own stop mechanism)
        std::cout << "Press Enter to stop playback..." << std::endl;
        std::cin.get();

        // Stop and close PortAudio stream
        err = Pa_StopStream(stream);
        if (err != paNoError) {
            std::cerr << "PortAudio stream stop error: " << Pa_GetErrorText(err) << std::endl;
            // sf_close(sndfile);
            Pa_Terminate();
            return;
        }

        err = Pa_CloseStream(stream);
        if (err != paNoError) {
            std::cerr << "PortAudio stream close error: " << Pa_GetErrorText(err) << std::endl;
            // sf_close(sndfile);
            Pa_Terminate();
            return;
        }

        // Terminate PortAudio and close the audio file
        Pa_Terminate();
    }



    /**
    * @brief Class to handle the curl session
    */
    class Session {
    public:
        /// @brief Construct a new Session object by initializing curl and setting the options
        Session() {
            // Initialize curl
            curl_global_init(CURL_GLOBAL_ALL);// || CURL_VERSION_THREADSAFE);

            curl_ = curl_easy_init();
            if (curl_ == nullptr) {
                std::cout << "OpenAI curl_easy_init() failed" << '\n';
            }

            // Ignore SSL
            curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
        }

        /// @brief Destroy the Session object by cleaning up curl
        ~Session() {
            curl_easy_cleanup(curl_);
            curl_global_cleanup();
        }

        /// @brief Set the url to make the request to
        void setUrl(const std::string& url) { url_ = url; }

        /// @brief Set the token to use for authentication
        void setToken(const std::string& token) {
            token_ = token;
        }

        /// @brief Set the body of the request to send
        void setBody(const std::string& data) {
            if (curl_) {
                curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, data.length());
                curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, data.data());
            }
        }

        bool makeRequest(SharedData* sharedData) {
            std::lock_guard<std::mutex> lock(mutex_request_); // Lock the request to avoid concurrent requests

            // Set the headers
            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, std::string{ "Authorization: Bearer " + token_ }.c_str());
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl_, CURLOPT_URL, url_.c_str());

            // Set the callback function to write binary data
            curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeBinaryData);
            curl_easy_setopt(curl_, CURLOPT_WRITEDATA, sharedData);

            // Perform the request
            res_ = curl_easy_perform(curl_);

            // Check for errors
            if (res_ != CURLE_OK) {
                std::string error_msg = "OpenAI curl_easy_perform() failed: " + std::string{ curl_easy_strerror(res_) } + '\n';
                std::cout << error_msg << std::endl;
                return false;
            }

            // Clean up the headers
            curl_slist_free_all(headers);

            return true;
        }

        /// @brief Make the request and return whether it was successful
        /// @return true if the request was successful
        bool makeStreamRequest(Message* message) {
            std::lock_guard<std::mutex> lock(mutex_request_); // Lock the request to avoid concurrent requests

            // Set the headers
            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, std::string{ "Authorization: Bearer " + token_ }.c_str());
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl_, CURLOPT_URL, url_.c_str());

            // Set the callback function
            std::string header_string;
            curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeStreamFunction);
            curl_easy_setopt(curl_, CURLOPT_WRITEDATA, message);
            curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &header_string);

            // Perform the request
            res_ = curl_easy_perform(curl_);

            // Check for errors
            if (res_ != CURLE_OK) {
                std::string error_msg = "OpenAI curl_easy_perform() failed: " + std::string{ curl_easy_strerror(res_) } + '\n';
                std::cout << error_msg << std::endl;
                return false;
            }
            return true;
        };

    private:
        /// @brief Callback function to write the audio response to the file
        static size_t writeBinaryData(void* ptr, size_t size, size_t nmemb, void* stream) {
            // Print the first few bytes of the incoming Opus data
            std::cout << "Incoming Opus data (" << size * nmemb << " bytes): ";
            for (size_t i = 0; i < 16; ++i) {
                std::printf("%02X ", static_cast<unsigned char*>(ptr)[i]);
            }
            std::cout << "...\n";


            SharedData* sharedData = static_cast<SharedData*>(stream);

            // Buffer to store the incoming Ogg data
            char* buffer = ogg_sync_buffer(&sharedData->oy, size * nmemb);
            memcpy(buffer, ptr, size * nmemb);
            ogg_sync_wrote(&sharedData->oy, size * nmemb);

            // Process the Ogg pages and extract Opus packets
            while (ogg_sync_pageout(&sharedData->oy, &sharedData->og) == 1) {
                if (!sharedData->oggInitialized || sharedData->serial_number == -1) {
                    sharedData->serial_number = ogg_page_serialno(&sharedData->og);
                    sharedData->initOggStream(sharedData->serial_number);
                }

                if (ogg_stream_pagein(&sharedData->os, &sharedData->og) != 0) {
                    std::cerr << "Failed to read Ogg page into stream." << std::endl;
                }

                ogg_packet op;
                while (ogg_stream_packetout(&sharedData->os, &op) == 1) {
                    // Decode the Opus packet
                    float decodedPCM[FRAMES_PER_BUFFER * CHANNELS];
                    int frameSize = opus_decode_float(sharedData->opusDecoder, op.packet, op.bytes, decodedPCM, FRAMES_PER_BUFFER, 0);
                    if (frameSize < 0) {
                        // Handle Opus decoding error
                        std::cerr << "Opus decoding error: " << opus_strerror(frameSize) << std::endl;
                        continue;
                    }

                    sharedData->audioBuffer.addData(decodedPCM, frameSize * CHANNELS);
                    sharedData->dataReady = true;
                }
            }

            // Debugging output
            std::cout << "Received Ogg Opus data (" << size * nmemb << " bytes)." << std::endl;


            size_t written = fwrite(ptr, size, nmemb, sharedData->file);
            std::cout << "Written: " << written << std::endl;

            return size * nmemb;
        }


        /// @brief Callback function to write the response to our StreamResponse object
        static size_t writeStreamFunction(void* ptr, size_t size, size_t nmemb, Message* msg) {
            size_t realsize = size * nmemb;
            std::string text((char*)ptr, realsize);
#if DEBUG
            std::cout << "Write stream message type: " + messageTypeToString(msg->getType()) << std::endl;
            std::cout << "Write stream text: " + text << std::endl;
#endif
            msg->setAIResponse(text);
            return size * nmemb;
        }



    private:
        CURL* curl_; ///< The curl session
        CURLcode    res_; ///< The curl result
        std::string url_; ///< The url to make the request to
        std::string token_; ///< The token to use for authentication
        std::mutex  mutex_request_; ///< Mutex to avoid concurrent requests
    };

    /// @brief Class to handle the OpenAI API
    class OpenAI {
    public:
        /// @brief Construct a new OpenAI object
        /// @param token The token to use for authentication (optional if set as environment variable (OPENAI_API_KEY)
        OpenAI(const std::string& token = "")
            : token_{ token } {
            if (token.empty()) { // If no token is provided, try to get it from the environment variable
                if (const char* env_p = std::getenv("OPENAI_API_KEY")) {
                    token_ = std::string{ env_p }; // Set the token from the environment variable
                }
                else { // If the environment variable is not set, log an error
                    std::cout << "OPENAI_API_KEY environment variable not set" << '\n';
                }
            }
            session_.setToken(token_);
        }

        OpenAI(const OpenAI&) = delete;
        OpenAI& operator=(const OpenAI&) = delete;
        OpenAI(OpenAI&&) = delete;
        OpenAI& operator=(OpenAI&&) = delete;

        bool post(const std::string& suffix, const std::string& data, SharedData* shared_data = nullptr, Message* message = nullptr) {
            auto complete_url = base_url + suffix;
            session_.setUrl(complete_url);
            session_.setBody(data);
#if DEBUG
            std::cout << "<< request: " + complete_url + "  " + data + "\n";
#endif
            if (message) {
                return session_.makeStreamRequest(message);
            }
            else if (shared_data) {
                return session_.makeRequest(shared_data);
            }
            else {
                std::cout << "No file path or message provided\n";
                return false;
            }
        }

        bool chat(const std::string& input, Message* message) {
		    return post("chat/completions", input, nullptr, message);
	    }

        bool textToSpeech(const std::string& text, SharedData* shared_data) {
            shared_data->initOpusDecoder();

            // Prepare the data for the TTS request
            nlohmann::json data;
            data["input"] = text; // Set the input text for the TTS request
            data["model"] = "tts-1-hd"; // Set the model to use for the TTS request
            data["voice"] = "alloy"; // Set the voice to use for the TTS request
            data["response_format"] = "opus"; // Set the response format to use for the TTS request
            data["speed"] = 1.0f; // Set the speed to use for the TTS request

            std::string dataStr = data.dump();
            std::cout << "Sending text to speech request with: " << dataStr << "\n";

            bool success = post("audio/speech", dataStr, shared_data);

            return success;

        }

    private:
        Session session_;
        std::string token_;
        std::string organization_;
        std::string base_url = "https://api.openai.com/v1/";
    };


} // namespace openai

#endif // OPENAI_REDUCED_HPP_
