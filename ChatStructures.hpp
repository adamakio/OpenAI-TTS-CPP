#ifndef CHAT_STRUCTURES_H
#define CHAT_STRUCTURES_H

#include <string>
#include <chrono>
#include <vector>
#include <atomic>
#include <iostream>

#include <nlohmann/json.hpp>

namespace openai {
		using Json = nlohmann::json;

		/// @brief Struct that contains a word and its start time for the cached response
		struct Word {
			std::string text;
			long long start;

			Word(std::string t, long long s) : text(t), start(s) {}
		};

		/// @brief Enum for the type of message
		enum class MessageType {
			CachedSelect, ///< Cached message upon selecting a maneuver  (followed by "studentAssertingControl" message)
			CachedBegin, ///< Cached message after student asserts control (followed by "CachedWarn" or "CachedFatal" or "CachedSuccess" message)
			CachedWarn, ///< Cached message for warning the student (followed by "CachedWarn" or "CachedFatal" or "CachedSuccess" message)
			CachedFatal, ///< Cached message for fatal error  (followed by "studentRelinquishingControl" message)
			CachedSuccess, ///< Cached message for success (followed by "studentRelinquishingControl" message)
			Cached, ///< Default cached message (also separates cached from non-cached messages) 

			AIGeneratedResponse, ///< AI generated response (followed by "UserTranscription")
			None, ///< Default message type


			UserTranscription, ///< User transcript -> (followed by "AIGeneratedResponse" message)
			studentRelinquishingControl, ///< Student relinquished control (followed by "AIGeneratedResponse" message)
			studentAssertingControl, ///< Student asserted control (followed by "CachedBegin" message)
		};

		static bool isCached(const MessageType& message_type) {
			return message_type <= MessageType::Cached;
		}

		static bool isUser(const MessageType& message_type) {
			return message_type >= MessageType::UserTranscription;
		}

		static bool isAI(const MessageType& message_type) {
			return message_type == MessageType::AIGeneratedResponse;
		}

		static const std::string messageTypeToString(const MessageType& type) {
			switch (type) {
			case MessageType::UserTranscription: return "UserTranscription";
			case MessageType::AIGeneratedResponse: return "AIGeneratedResponse";
			case MessageType::studentAssertingControl: return "studentAssertingControl";
			case MessageType::studentRelinquishingControl: return "studentRelinquishingControl";
			case MessageType::CachedWarn: return "CachedWarn";
			case MessageType::CachedBegin: return "CachedBegin";
			case MessageType::CachedSelect: return "CachedSelect";
			case MessageType::CachedFatal: return "CachedFatal";
			case MessageType::CachedSuccess: return "CachedSuccess";
			case MessageType::Cached: return "Cached";
			case MessageType::None: return "None";
			default: return "Unknown";
			}
		}

		class Message {
		public:
			// Constructors
			Message(MessageType type) : m_type(type), m_lastUpdated(std::chrono::system_clock::now()) {}
			

			// Transcription methods

			void setPartialTranscript(const std::string& transcript) {
				m_partialTranscript = transcript;
				m_text = m_finalTranscript + m_partialTranscript;
				m_lastUpdated = std::chrono::system_clock::now();
			}

			void setFinalTranscript(const std::string& transcript) {
				m_finalTranscript += transcript;
				m_partialTranscript = "";
				m_text = m_finalTranscript;
				m_lastUpdated = std::chrono::system_clock::now();
			}

			bool receivedFinal() const noexcept {
				return !m_finalTranscript.empty();
			}

			
			/// @brief Set the response from the API
			void setAIResponse(const std::string& data) {
				if (!isAI(this->m_type)) {
					return;
				}

				// Append new data to the buffer
				m_buffer += data;

				// Try to find a complete JSON object
				size_t startPos = m_buffer.find("data: ");
				while (startPos != std::string::npos) {
					size_t endPos = m_buffer.find("\n\n", startPos); // Assuming "\n\n" is the delimiter between events
					if (endPos == std::string::npos) {
						// If we don't have the complete JSON object, break and wait for more data
						break;
					}

					// Extract the JSON object
					std::string json_data = m_buffer.substr(startPos + 6, endPos - (startPos + 6)); // Remove "data: " prefix
					m_buffer.erase(0, endPos + 2); // Remove the processed object from the buffer

					// Parse the JSON data
					Json parsed;
					try {
						parsed = Json::parse(json_data);
					}
					catch (std::exception&) {
						// If parsing fails, it's not a valid JSON, so we continue to the next object
						startPos = m_buffer.find("data: ", endPos);
						continue;
					}

					// Process the parsed JSON as before
					for (const auto& choice : parsed["choices"]) {
						if (choice.contains("delta") && choice["delta"].contains("content")) {
							std::string chunk = choice["delta"]["content"].get<std::string>();
							std::cout << chunk;
							m_text += chunk;
						}
						if (choice.contains("finish_reason") && !choice["finish_reason"].is_null()) {
							std::cout << std::endl;
							m_isUpdating = false;
						}
					}
					if (!m_isUpdating) {
						std::cout << "Finished updating: " << messageTypeToString(m_type) << '\n';
						break;
					}

					// Look for the next JSON object
					startPos = m_buffer.find("data: ");
				}
			}


			// Getters
			MessageType getType() const { return m_type; }
			std::string getText() const { return m_text; }
			std::chrono::system_clock::time_point getLastUpdated() const { return m_lastUpdated; }
			bool isUpdating() const { return m_isUpdating; }

		private:
			// General fields
			MessageType m_type{ MessageType::None }; ///< Type of message
			std::string m_text{ "" }; ///< Text of the message
			std::chrono::system_clock::time_point m_lastUpdated; ///< Latest timestamp of the message
			std::atomic<bool> m_isUpdating{ true }; ///< Flag to indicate whether the message is being updated

			// Fields specific to transcription
			std::string m_partialTranscript{ "" };
			std::string m_finalTranscript{ "" };

			// Fields specific to cached messages
			std::vector<Word> m_words; ///< Vector of words in the cached message
			std::chrono::steady_clock::time_point m_wordsStartTime; ///< Start time of the words
			size_t m_lastProcessedWordIndex; ///< Index of the last processed word

			// Fields specific to AI generated response
			std::string m_buffer{ "" }; ///< Buffer to hold incomplete JSON data

		};

	} // namespace openai
#endif // XPROTECTION_CHAT_STRUCTURES_H
