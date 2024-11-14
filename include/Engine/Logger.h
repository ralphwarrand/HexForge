#pragma once

//STL
#include <iostream>
#include <chrono>
#include <format>
#include <string>
#include <fstream>

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

		auto const time = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
		std::string log = std::format("{:%Y-%m-%d %X }", time);

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