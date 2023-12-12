// NetworkingCPP.cpp : Defines the entry point for the application.
//

#include "NetworkingCPP.h"
#

// Function to get response from OpenAI API
bool getResponse(openai::Message& msg) {
    nlohmann::json data = {
        {"model", "gpt-3.5-turbo"},
        {"messages" , {
            {
                {"role", "system"},
                {"content", "You are a helpful assistant."}
            },
            {
                {"role", "user"},
                {"content" , "Who won the world series in 2020?"}
            },
            {
                {"role", "assistant"},
                {"content", "The Los Angeles Dodgers won the World Series in 2020."}
            },
            {
                {"role", "user"},
                {"content" , "Where was it played?"}
            },
            {
                {"role", "assistant"},
                {"content", "The World Series was played in Arlington, Texas."}
            },
            {
                {"role", "user"},
                {"content", "Where is Texas? Give a 3 sentence long answer"}
            }
        }},
        {"stream", true},
        {"max_tokens", 150}
    };
	openai::OpenAI openAI{ };  // Replace with your API key
	bool success = openAI.chat(data.dump(), &msg);
	std::cout << "Success: " << success << "\n";

	return success;
}


int main() {
    char buffer[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, buffer);
    std::cout << "Current Working Directory: " << buffer << std::endl;

    // Create a folder named 'audio' in the current working directory
    std::filesystem::path audioFolderPath = std::filesystem::current_path() / "audio";
    if (!std::filesystem::exists(audioFolderPath)) {
        std::filesystem::create_directory(audioFolderPath);
    }

    // Modify the file path to include the audio folder
    std::filesystem::path filePath = audioFolderPath / "live.opus";
    std::string str = filePath.string();
    char* cstr = new char[str.length() + 1];
    strcpy(cstr, str.c_str());


    // Open file to store audio
    FILE* fp = fopen(cstr, "wb");
    if (!fp) {
		std::cout << "Could not open file for writing: " << cstr << std::endl;
		return 1;
	}

    openai::SharedData sharedData{ fp };
    std::thread([&sharedData]{
        openai::OpenAI openAI{ }; // Replace with your API key
        openAI.textToSpeech("C plus plus is the best language in the world", &sharedData);
    }).detach();

    openai::playAudio(&sharedData);

    // Close the file
    fclose(fp);
    return 0;
}
