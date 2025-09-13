#pragma once

#include "httplib.h"
#include <cstdint>
#include <vector>
#include <string>

struct WavHeader
{
  char riff[4] = {'R', 'I', 'F', 'F'};
  uint32_t fileSize;
  char wave[4] = {'W', 'A', 'V', 'E'};
  char fmt[4] = {'f', 'm', 't', ' '};
  uint32_t fmtSize = 16;
  uint16_t audioFormat = 1;
  uint16_t numChannels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample = 16;
  char data[4] = {'d', 'a', 't', 'a'};
  uint32_t dataSize;
} __attribute__((packed));

// HttpClient class for posting audio to an orchestrator and handling responses
class HttpClient
{
public:
  HttpClient(const std::string &host, int port);

  bool postOrch(const std::string &path,
                const std::vector<int16_t> &audioData,
                int sampleRate,
                int channels);

  std::vector<uint8_t> getLastResponseAudio() const;

private:
  httplib::Client cli_;
  std::vector<uint8_t> lastResponseAudio_;

  std::vector<uint8_t> createWavFromPCM(const std::vector<int16_t> &pcmData,
                                        int sampleRate,
                                        int channels);
};