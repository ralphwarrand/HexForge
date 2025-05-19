#include "pch.h"

#include "Core/Console.h"

// Hex
#include "Core/Logger.h"

namespace Hex
{
    Console::Console()
    {
        // Register the Logger callback to forward logs to the console
        Hex::Logger::Instance().AddLogCallback(
            [this](Hex::LogLevel level, const std::string& message) {
                AppendLogEntry(message);
            });
    }

    void Console::Render()
    {
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

    void Console::ExecuteCommand(const std::string &command)
    {
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
}
