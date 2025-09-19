#include "client.hpp"
#include "httplib.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>

HttpClient::HttpClient(const std::string &host, int port, const std::string &authToken)
    : cli_(host, port)
{
  cli_.set_default_headers({{"X-Auth", authToken}});
  cli_.set_connection_timeout(std::chrono::seconds(5));
  cli_.set_read_timeout(std::chrono::seconds(30));
  cli_.set_write_timeout(std::chrono::seconds(30));
}

std::vector<uint8_t> HttpClient::createWavFromPCM(const std::vector<int16_t> &pcmData,
                                                  int sampleRate, int channels)
{
  WavHeader header;
  header.numChannels = channels;
  header.sampleRate = sampleRate;
  header.byteRate = sampleRate * channels * 2;
  header.blockAlign = channels * 2;

  uint32_t dataSize = pcmData.size() * sizeof(int16_t);
  header.dataSize = dataSize;
  header.fileSize = sizeof(WavHeader) - 8 + dataSize;

  std::vector<uint8_t> wavData;
  wavData.reserve(sizeof(WavHeader) + dataSize);

  wavData.resize(sizeof(WavHeader));
  std::memcpy(wavData.data(), &header, sizeof(WavHeader));

  const uint8_t *pcmBytes = reinterpret_cast<const uint8_t *>(pcmData.data());
  wavData.insert(wavData.end(), pcmBytes, pcmBytes + dataSize);

  return wavData;
}

std::vector<uint8_t> HttpClient::getLastResponseAudio() const
{
  return lastResponseAudio_;
}

bool HttpClient::postOrch(const std::string &path, const std::vector<int16_t> &audioData,
                          int sampleRate, int channels)
{
  std::cout << "processing audio in-memory: " << audioData.size() << " samples" << std::endl;

  std::vector<uint8_t> wavData = createWavFromPCM(audioData, sampleRate, channels);
  std::string wav_content(wavData.begin(), wavData.end());

  std::vector<httplib::MultipartFormData> items = {
      {"file",
       wav_content,
       "recording.wav",
       "audio/wav"}};

  auto res = cli_.Post(path.c_str(), items);

  if (res)
  {
    if (res->status == 200)
    {
      std::cout << "upload successful, received response size: " << res->body.size() << " bytes." << std::endl;
      lastResponseAudio_.assign(res->body.begin(), res->body.end());
      return true;
    }
    else
    {
      std::cerr << "server returned status code: " << res->status << ". Body: " << res->body << std::endl;
      return false;
    }
  }
  else
  {
    std::cerr << "request failed: " << httplib::to_string(res.error()) << std::endl;
    return false;
  }
}
