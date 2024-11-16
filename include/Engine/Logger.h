#pragma once

//STL
#include <iostream>
#include <chrono>
#include "fmt/core.h"
#include "date/date.h"
#include <string>
#include <fstream>
#include "date/tz.h"

namespace Hex 
{
	enum class LogLevel
	{
		Debug,
		Info,
		Warning,
		Error,
		Fatal
	};

	inline void Log(const LogLevel log_level, const std::string& log_msg) {
		std::ofstream ofs("log.txt", std::ios_base::app | std::ios_base::out);

		auto now = std::chrono::system_clock::now();
		auto local_time = date::format("%F %H:%M:%S", date::make_zoned(date::current_zone(), now));
		std::string log = fmt::format("{}", local_time);

		switch (log_level) {
		case LogLevel::Debug:
			log += "[DEBUG] ";
			break;
		case LogLevel::Info:
			log += "[INFO] ";
			break;
		case LogLevel::Warning:
			log += "[WARNING] ";
			break;
		case LogLevel::Error:
			log += "[ERROR] ";
			break;
		case LogLevel::Fatal:
			log += "[FATAL] ";
			break;
		}

		log += log_msg;

		ofs << log << '\n';
		ofs.close();

		std::cout << log << '\n';
	}

	inline void Clear() {
		std::ofstream ofs;
		ofs.open("log.txt", std::ofstream::out | std::ofstream::trunc);
		ofs.close();
	}
}