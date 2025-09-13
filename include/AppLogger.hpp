#pragma once

#include <string>
#include <fstream>
#include <chrono>
#include <ctime>
#include <filesystem>

// AppLogger clas for centralized logging
class AppLogger
{
public:
  static AppLogger &getInstance();

  bool open(const std::string &filename);

  void info(const std::string &message);

  void error(const std::string &message);

  ~AppLogger();

private:
  AppLogger();

  AppLogger(const AppLogger &) = delete;
  AppLogger &operator=(const AppLogger &) = delete;

  std::ofstream logFile;

  std::string getTimestamp();

  void logToStream(const std::string &message);
};