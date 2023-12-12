#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace assemblyai{
using json = nlohmann::json;
std::string constructWebSocketUrl(int sampleRate, const std::vector<std::string>& wordBoost) {
    // Construct JSON array for word_boost using nlohmann JSON
    json wordBoostJson = json::array();
    for (const auto& word : wordBoost) {
        wordBoostJson.push_back(word);
    }

    // URL encode the JSON array
    char* encodedWordBoost = curl_easy_escape(nullptr, wordBoostJson.dump().c_str(), 0);
    std::string encodedWordBoostStr(encodedWordBoost);
    curl_free(encodedWordBoost);

    // Construct the URL
    std::string url = "wss://api.assemblyai.com/v2/realtime/ws?sample_rate=" + std::to_string(sampleRate) + "&word_boost=" + encodedWordBoostStr;
    return url;
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
        return newLength;
    }
    catch (std::bad_alloc& e) {
        std::cout << "Memory allocation failed inside WriteCallback" << e.what() << std::endl;
        return 0;
    }
}

std::string fetchToken(const std::string& apiToken) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("authorization: " + apiToken).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        json data = { {"expires_in", 3 * 60 * 60} }; // Expire in 3 hours
        std::string postData = data.dump();

        curl_easy_setopt(curl, CURLOPT_URL, "https://api.assemblyai.com/v2/realtime/token");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            return "";
        }
    }
    delete curl;
    // Parse JSON response and extract token
    json response = json::parse(readBuffer);
    return response["token"];
}

int main() {
    int sampleRate = 48000;
    std::vector<std::string> wordBoost = { "i have control", "you have control" };

    std::string url = constructWebSocketUrl(sampleRate, wordBoost);
    std::cout << "WebSocket URL (without token): " << url << std::endl;

    // Replace YOUR_API_TOKEN with the actual API token
    std::string token = fetchToken("7e4983bb8d1d47acb2dec97ee5e4c3ed");
    if (!token.empty()) {
        std::string finalUrl = url + "&token=" + token;
        std::cout << "Final WebSocket URL: " << finalUrl << std::endl;
    }
    else {
        std::cerr << "Failed to fetch token" << std::endl;
    }

    return 0;
}

}

