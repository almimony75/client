#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <portaudio.h>
#include <pv_porcupine.h>


class AppLogger;

class PorcupineDetector
{
public:
  PorcupineDetector(const std::string &accessKey,
                    const std::string &modelPath,
                    const std::string &keywordPath,
                    float sensitivity);

  ~PorcupineDetector();

  bool isInitialized() const;

  void run(const std::function<void()> &onWakeWord);

private:
  pv_porcupine_t *porcupineHandle = nullptr;
  PaStream *paStream = nullptr;

  bool initializedPorcupine = false;
  bool initializedStream = false;
  bool overallInitialized = false;

  int sampleRate;
  int frameLength;
  const int channels = 1;
  float sensitivity;

  std::string accessKeyCopy;
  std::string modelPathCopy;
  std::string keywordPathCopy;

  bool initializePorcupine();
  bool initializeAudioStream();
  void cleanupAudioStream();
  void cleanupPorcupine();
};