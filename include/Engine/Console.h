#include "Logger.h"
#include "imgui.h"
#include <vector>
#include <string>
#include <mutex>
#include <functional>

namespace Hex
{
    class Console {
    public:
        Console() {
            // Register the Logger callback to forward logs to the console
            Hex::Logger::Instance().AddLogCallback(
                [this](Hex::LogLevel level, const std::string& message) {
                    AppendLogEntry(message);
                });
        }

        // Render the console window
        void Render() {
            ImGui::Begin("Console");

            // Log Display Area
            ImGui::BeginChild("LogArea", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto& entry : logEntries_) {
                    ImGui::TextUnformatted(entry.c_str());
                }
                if (scrollToBottom_) {
                    ImGui::SetScrollHereY(1.0f);
                    scrollToBottom_ = false;
                }
            }
            ImGui::EndChild();

            // Command Input Area
            if (ImGui::InputText("Command", inputBuffer_, IM_ARRAYSIZE(inputBuffer_),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                ExecuteCommand(inputBuffer_);
                inputBuffer_[0] = '\0'; // Clear the input buffer after execution
            }
            ImGui::SameLine();
            if (ImGui::Button("Execute")) {
                ExecuteCommand(inputBuffer_);
                inputBuffer_[0] = '\0'; // Clear the input buffer after execution
            }

            ImGui::End();
        }

        // Clear the console log
        void Clear() {
            std::lock_guard<std::mutex> lock(mutex_);
            logEntries_.clear();
        }

    private:
        std::vector<std::string> logEntries_; // Stores log messages
        std::mutex mutex_;                    // Mutex to protect logEntries_
        char inputBuffer_[256] = "";          // Input buffer for commands
        bool scrollToBottom_ = true;          // Auto-scroll flag

        // Append a log entry to the console
        void AppendLogEntry(const std::string& entry) {
            std::lock_guard<std::mutex> lock(mutex_);
            logEntries_.push_back(entry);
            scrollToBottom_ = true;
        }

        // Execute a user-entered command
        void ExecuteCommand(const std::string& command) {
            if (command.empty()) return;

            // Echo the command in the console
            AppendLogEntry("> " + command);

            // Handle predefined commands
            if (command == "clear")
            {
                Clear();
            }
            else if (command == "log debug")
            {
                Log(LogLevel::Debug, "Debug command executed.");
            }
            else if (command.find("echo") == 0) // Check if command starts with "echo"
            {
                std::string message = command.substr(5); // Get everything after "echo "
                AppendLogEntry(message); // Echo the message back in the console
            }
            else
            {
                AppendLogEntry("Unknown command: " + command);
            }
        }
    };
}