#pragma once
#include <string>
#include <vector>
#include <cstdint> 
#include <portaudio.h> 

class MicrophoneRecorder
{
public:
  MicrophoneRecorder(int sampleRate = 16000, int channels = 1);

  ~MicrophoneRecorder();

  bool isInitialized() const;

  std::vector<int16_t> recordWithVAD();

  bool playAudioData(const std::vector<uint8_t> &wavData);

private:
  bool initialized;
  int sampleRate;
  int channels;

  static constexpr float VAD_START_THRESHOLD_SQ = 500.0f * 500.0f;
  static constexpr float VAD_STOP_THRESHOLD_SQ = 300.0f * 300.0f;

  static constexpr int MAX_SILENCE_FRAMES_BEFORE_STOP = 30;
  static constexpr int FRAME_DURATION_MS = 20;

  static constexpr size_t MAX_RECORDING_SAMPLES = 60 * 16000; // 60 seconds at 16kHz
  std::vector<int16_t> recordingBuffer_;

  float computeRMS(const int16_t *data, size_t numSamples);
};