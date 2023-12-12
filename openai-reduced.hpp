#ifndef OPENAI_REDUCED_HPP_
#define OPENAI_REDUCED_HPP_

#include <iostream>
#include <stdexcept>
#include <string>
#include <mutex>
#include <fstream>

#include <condition_variable>

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sndfile.h>

#include "ChatStructures.hpp"

#define DEBUG 0

namespace openai {
    
    struct SharedData {
        FILE* file;
        std::atomic<bool> dataReady;
    };



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
            SharedData* sharedData = static_cast<SharedData*>(stream);
            size_t written = fwrite(ptr, size, nmemb, sharedData->file);

            size_t realSize = size * nmemb;
            char* incomingData = static_cast<char*>(ptr);

#ifdef DEBUG 
            std::cout << "Binary data size: " << realSize << std::endl;
#endif

            return written;
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
            // Prepare the data for the TTS request
            nlohmann::json data;
            data["model"] = "tts-1-hd"; // Set the model to use for the TTS request
            data["input"] = text;
            data["voice"] = "alloy"; // Set the voice to use for the TTS request
            // Add other necessary fields as per the TTS service's API

            std::string dataStr = data.dump();
            std::cout << "Sending text to speech request with: " << dataStr << "\n";

            return post("audio/speech", dataStr, shared_data);
        }

    private:
        Session session_;
        std::string token_;
        std::string organization_;
        std::string base_url = "https://api.openai.com/v1/";
    };


} // namespace openai

#endif // OPENAI_REDUCED_HPP_
