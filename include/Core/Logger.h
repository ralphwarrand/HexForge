#pragma once

// STL
#include <fstream>
#include <string>
#include <mutex>
#include <functional>
#include <memory>

// Third-party
#include "date/date.h"
#include "date/tz.h"

namespace Hex
{
    enum class LogLevel {
        Debug,
        Info,
        Warning,
        Error,
        Fatal
    };

    class Logger {
    public:

        // Add a type for the log callback
        using LogCallback = std::function<void(LogLevel, const std::string&)>;

        // Method to register a callback
        void AddLogCallback(const LogCallback& callback) {
            std::lock_guard<std::mutex> lock(mutex_);
            callbacks_.push_back(callback);
        }

        static Logger& Instance() {
            static Logger instance;
            return instance;
        }

        void SetLogLevel(LogLevel level) { log_level_ = level; }
        void SetLogFile(const std::string& filename);

        void Log(const LogLevel level, const std::string& msg);

        void Clear();

        // Prevent copy or move
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        Logger(Logger&&) = delete;
        Logger& operator=(Logger&&) = delete;

    private:
        Logger() : log_level_(LogLevel::Debug), log_file_("log.txt") {
            ofs_.open(log_file_, std::ios_base::trunc | std::ios_base::out);
        }

        ~Logger() {
            if (ofs_.is_open()) {
                ofs_.close();
            }
        }

        LogLevel log_level_;
        std::string log_file_;
        std::ofstream ofs_;
        std::mutex mutex_;
        std::vector<LogCallback> callbacks_;
    };

    // Helper function for convenience
    inline void Log(const LogLevel level, const std::string& msg) {
        Logger::Instance().Log(level, msg);
    }

    inline void SetLogLevel(LogLevel level) {
        Logger::Instance().SetLogLevel(level);
    }

    inline void SetLogFile(const std::string& filename) {
        Logger::Instance().SetLogFile(filename);
    }

    inline void ClearLog() {
        Logger::Instance().Clear();
    }
}