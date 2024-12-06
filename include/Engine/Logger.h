#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <chrono>
#include <functional>
#include <memory>
#include "fmt/core.h"
#include "date/date.h"
#include "date/tz.h"

#include "Gameplay/EntityComponents.h"

namespace Hex {

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

        void SetLogLevel(LogLevel level) {
            log_level_ = level;
        }

        void SetLogFile(const std::string& filename) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (ofs_.is_open()) {
                ofs_.close();
            }
            ofs_.open(filename, std::ios_base::trunc  | std::ios_base::out);

            if (ofs_.is_open()) {
                auto now = std::chrono::system_clock::now();
                auto local_time = date::format("%F %T", date::make_zoned(date::current_zone(), std::chrono::time_point_cast<std::chrono::seconds>(now)));
                ofs_ <<                 "\n===============================\n";
                ofs_ << fmt::format("[{}] [INFO] Log session started\n", local_time);
                ofs_ <<                   "===============================\n";
                ofs_.flush(); // Ensure header is written immediately
            }
        }


        void Log(const LogLevel level, const std::string& msg) {
            if (level < log_level_) return;

            std::lock_guard<std::mutex> lock(mutex_);

            const auto now = std::chrono::system_clock::now();
            auto local_time = date::format("%F %H:%M:%S",
                date::make_zoned(date::current_zone(),
                    std::chrono::time_point_cast<std::chrono::seconds>(now))
            );

            std::string log_entry = fmt::format("[{}] ", local_time);

            switch (level) {
                case LogLevel::Debug: log_entry += "[DEBUG] "; break;
                case LogLevel::Info: log_entry += "[INFO] "; break;
                case LogLevel::Warning: log_entry += "[WARNING] "; break;
                case LogLevel::Error: log_entry += "[ERROR] "; break;
                case LogLevel::Fatal: log_entry += "[FATAL] "; break;
            }

            log_entry += msg;

            // Output to console
            std::cout << log_entry << '\n';

            // Output to file if enabled
            if (ofs_.is_open()) {
                ofs_ << log_entry << '\n';
                ofs_.flush(); // Ensure immediate writing to the file
            }

            // Notify all registered callbacks
            for (const auto& callback : callbacks_) {
                callback(level, log_entry);
            }
        }

        void Clear() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (ofs_.is_open()) {
                ofs_.close();
                ofs_.open(log_file_, std::ofstream::out | std::ofstream::trunc);
            }
        }

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