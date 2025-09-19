#pragma once

#include <string>
#include <map>

// A simple configuration loader that uses only the C++ Standard Library.
class ConfigLoader
{
public:
  // Reads the file and loads key-value pairs.
  bool loadFromFile(const std::string &filename);

  // Get a value as a string.
  std::string getString(const std::string &key, const std::string &defaultValue) const;

  // Getters for other types (int, float, bool).
  int getInt(const std::string &key, int defaultValue) const;
  float getFloat(const std::string &key, float defaultValue) const;
  bool getBool(const std::string &key, bool defaultValue) const;

private:
  std::map<std::string, std::string> data;
};