// Hex
#include "HexForge/pch.h"
#include "HexForge/Core/Logger.h"

// C++ standard library includes for time
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>


namespace Hex
{
    std::string GetCurrentTimestamp()
    {
        // 1. Get the current time point
        const auto now = std::chrono::system_clock::now();

        // 2. Convert to std::time_t for the date/time part
        const auto time_t_now = std::chrono::system_clock::to_time_t(now);

        // 3. Extract the milliseconds part
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        // 4. Convert to a tm struct in a thread-safe way
        std::tm tm_struct = {};
#if defined(_MSC_VER) // Microsoft Visual C++
        localtime_s(&tm_struct, &time_t_now);
#else // GCC, Clang on Linux, etc.
        localtime_r(&time_t_now, &tm_struct);
#endif

        // 5. Format the string using a string stream
        std::ostringstream oss;
        oss << std::put_time(&tm_struct, "%Y-%m-%d %H:%M:%S"); // Format: YYYY-MM-DD HH:MM:SS
        oss << '.' << std::setw(3) << std::setfill('0') << ms.count(); // Append milliseconds

        return oss.str();
    }
}

void Hex::Logger::SetLogFile(const std::string &filename)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (ofs_.is_open()) {
        ofs_.close();
    }
    ofs_.open(filename, std::ios_base::trunc  | std::ios_base::out);

    if (ofs_.is_open()) {
        // FIXED: Use the new timestamp function
        const std::string timestamp = GetCurrentTimestamp();
        ofs_ <<                 "\n========================================\n";
        ofs_ << "[" << timestamp << "] [INFO] Log session started\n";
        ofs_ <<                   "========================================\n";
        ofs_.flush();
    }
}

void Hex::Logger::Log(const LogLevel level, const std::string &msg)
{
    if (level < log_level_) return;

    std::lock_guard<std::mutex> lock(mutex_);

    // FIXED: Use the new timestamp function
    const std::string timestamp = GetCurrentTimestamp();

    std::ostringstream log_stream;
    log_stream << "[" << timestamp << "] ";

    switch (level) {
        case LogLevel::Debug:   log_stream << "[DEBUG] ";   break;
        case LogLevel::Info:    log_stream << "[INFO] ";    break;
        case LogLevel::Warning: log_stream << "[WARNING] "; break;
        case LogLevel::Error:   log_stream << "[ERROR] ";   break;
        case LogLevel::Fatal:   log_stream << "[FATAL] ";   break;
    }

    log_stream << msg;
    const std::string log_entry = log_stream.str();

    // Output to console
    std::cout << log_entry << '\n';

    // Output to file if enabled
    if (ofs_.is_open()) {
        ofs_ << log_entry << '\n';
        ofs_.flush();
    }

    // Notify all registered callbacks
    for (const auto& callback : callbacks_) {
        callback(level, log_entry);
    }
}

void Hex::Logger::Clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (ofs_.is_open()) {
        ofs_.close();
        ofs_.open(log_file_, std::ofstream::out | std::ofstream::trunc);
    }
}