#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace microgrid {

enum class LogLevel { Debug, Info, Warn, Error };

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void configure(const std::string& path, bool enable = true) {
        std::lock_guard<std::mutex> lock(mu_);
        enabled_ = enable;
        if (file_.is_open()) file_.close();
        if (enabled_ && !path.empty()) {
            file_.open(path, std::ios::out | std::ios::trunc);
        }
    }

    void log(LogLevel level, const std::string& msg) {
        if (!enabled_) return;
        std::lock_guard<std::mutex> lock(mu_);
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S")
            << " [" << level_str(level) << "] " << msg << "\n";
        const std::string line = oss.str();
        if (file_.is_open()) file_ << line;
        if (level >= LogLevel::Info) {
            std::cout << line;
        }
    }

    void info(const std::string& msg)  { log(LogLevel::Info,  msg); }
    void warn(const std::string& msg)  { log(LogLevel::Warn,  msg); }
    void error(const std::string& msg) { log(LogLevel::Error, msg); }
    void debug(const std::string& msg) { log(LogLevel::Debug, msg); }

private:
    Logger() = default;
    std::mutex mu_;
    std::ofstream file_;
    bool enabled_ = true;

    static const char* level_str(LogLevel l) {
        switch (l) {
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info:  return "INFO";
            case LogLevel::Warn:  return "WARN";
            case LogLevel::Error: return "ERROR";
        }
        return "UNKNOWN";
    }
};

#define LOG_INFO(msg)  microgrid::Logger::instance().info(msg)
#define LOG_WARN(msg)  microgrid::Logger::instance().warn(msg)
#define LOG_ERROR(msg) microgrid::Logger::instance().error(msg)
#define LOG_DEBUG(msg) microgrid::Logger::instance().debug(msg)

} // namespace microgrid
