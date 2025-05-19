#pragma once

// STL
#include <vector>
#include <string>
#include <mutex>

namespace Hex
{
    class Console
    {
    public:
        Console();

        // Render the console window
        void Render();

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
        void ExecuteCommand(const std::string& command);
    };
}
