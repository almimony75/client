#include "AppLogger.hpp"
#include <stdexcept>
#include <iostream>
#include <iomanip>


AppLogger& AppLogger::getInstance() {
    static AppLogger instance;
    return instance;
}


AppLogger::AppLogger() = default;

AppLogger::~AppLogger() {
    if (logFile.is_open()) {
        logFile << "--- Log Ended: " << getTimestamp() << " ---\n";
        logFile.close();
    }
}

bool AppLogger::open(const std::string& filename) {
    std::filesystem::path logPath(filename);
    if (logPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(logPath.parent_path(), ec);
        if (ec) {
            std::cerr << "Error: Could not create log directory " << logPath.parent_path() << ": " << ec.message() << std::endl;
            logFile.setstate(std::ios_base::failbit);
            return false;
        }
    }
    logFile.open(filename, std::ios_base::app);
    if (!logFile.is_open()) {
        std::cerr << "Error: Could not open log file: " << filename << std::endl;
        return false;
    }
    logFile << "--- Log Started: " << getTimestamp() << " ---\n";
    return true;
}

void AppLogger::info(const std::string& message) {
    logToStream("[INFO] " + message + "\n");
}

void AppLogger::error(const std::string& message) {
    std::cerr << "[ERROR] " << message << "\n";
    logToStream("[ERROR] " + message + "\n");
    logFile.flush();
}

std::string AppLogger::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* local_tm = std::localtime(&now_c);
    
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local_tm);
    return buffer;
}

void AppLogger::logToStream(const std::string& message) {
    if (logFile.is_open()) {
        logFile << getTimestamp() << " " << message;
    } else {
        // fallback to std::cout if the log file is not open
        std::cout << getTimestamp() << " " << message;
    }
}