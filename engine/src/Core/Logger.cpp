// Hex
#include "HexForge/pch.h"
#include "HexForge/Core/Logger.h"

void Hex::Logger::SetLogFile(const std::string &filename)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (ofs_.is_open()) {
        ofs_.close();
    }
    ofs_.open(filename, std::ios_base::trunc  | std::ios_base::out);

    if (ofs_.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto local_time = 0; //TODO: FIX time
        ofs_ <<                 "\n===============================\n";
        ofs_ << fmt::format("[{}] [INFO] Log session started\n", local_time);
        ofs_ <<                   "===============================\n";
        ofs_.flush(); // Ensure header is written immediately
    }
}

void Hex::Logger::Log(const LogLevel level, const std::string &msg)
{
    if (level < log_level_) return;

    std::lock_guard<std::mutex> lock(mutex_);

    const auto now = std::chrono::system_clock::now();
     auto local_time = 0; //TODO: FIX time


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

void Hex::Logger::Clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (ofs_.is_open()) {
        ofs_.close();
        ofs_.open(log_file_, std::ofstream::out | std::ofstream::trunc);
    }
}
