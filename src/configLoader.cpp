#include "configLoader.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

// Helper function to trim whitespace from both ends of a string
std::string trim(const std::string &s)
{
  const std::string WHITESPACE = " \t\n\r\f\v";
  size_t first = s.find_first_not_of(WHITESPACE);
  if (std::string::npos == first)
  {
    return s;
  }
  size_t last = s.find_last_not_of(WHITESPACE);
  return s.substr(first, (last - first + 1));
}

bool ConfigLoader::loadFromFile(const std::string &filename)
{
  data.clear();
  std::ifstream file(filename);
  if (!file.is_open())
  {
    std::cerr << "Error: Could not open configuration file: " << filename << std::endl;
    return false;
  }

  std::string line;
  while (std::getline(file, line))
  {
    if (line.empty() || line[0] == '#')
    {
      continue;
    }

    // --- NEW, ROBUST LOGIC ---
    size_t delimiter_pos = line.find('=');
    if (delimiter_pos != std::string::npos)
    {
      std::string key = line.substr(0, delimiter_pos);
      std::string value = line.substr(delimiter_pos + 1);

      data[trim(key)] = trim(value);
    }
  }
  return true;
}

std::string ConfigLoader::getString(const std::string &key, const std::string &defaultValue) const
{
  auto it = data.find(key);
  return (it != data.end()) ? it->second : defaultValue;
}

int ConfigLoader::getInt(const std::string &key, int defaultValue) const
{
  auto it = data.find(key);
  if (it != data.end())
  {
    try
    {
      return std::stoi(it->second);
    }
    catch (...)
    {
    }
  }
  return defaultValue;
}

float ConfigLoader::getFloat(const std::string &key, float defaultValue) const
{
  auto it = data.find(key);
  if (it != data.end())
  {
    try
    {
      return std::stof(it->second);
    }
    catch (...)
    {
    }
  }
  return defaultValue;
}

bool ConfigLoader::getBool(const std::string &key, bool defaultValue) const
{
  auto it = data.find(key);
  if (it != data.end())
  {
    const std::string &val = it->second;
    if (val == "true" || val == "1" || val == "yes")
      return true;
    if (val == "false" || val == "0" || val == "no")
      return false;
  }
  return defaultValue;
}
