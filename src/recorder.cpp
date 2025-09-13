#include "recorder.hpp"
#include <portaudio.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>

MicrophoneRecorder::MicrophoneRecorder(int sampleRate, int channels)
    : sampleRate(sampleRate), channels(channels), initialized(false)
{
  recordingBuffer_.reserve(MAX_RECORDING_SAMPLES);

  PaError err = Pa_Initialize();
  if (err == paNoError)
  {
    initialized = true;
    std::cout << "PortAudio initialized successfully." << std::endl;
  }
  else
  {
    std::cerr << "Error: PortAudio initialization failed: " << Pa_GetErrorText(err) << std::endl;
  }
}

MicrophoneRecorder::~MicrophoneRecorder()
{
  if (initialized)
  {
    PaError err = Pa_Terminate();
    if (err != paNoError)
    {
      std::cerr << "Warning: PortAudio termination failed: " << Pa_GetErrorText(err) << std::endl;
    }
  }
}

bool MicrophoneRecorder::isInitialized() const
{
  return initialized;
}

float MicrophoneRecorder::computeRMS(const int16_t *data, size_t numSamples)
{
  if (numSamples == 0)
  {
    return 0.0f;
  }

  double sum_sq = 0.0;
  for (size_t i = 0; i < numSamples; ++i)
  {
    sum_sq += static_cast<double>(data[i]) * data[i];
  }
  return static_cast<float>(sum_sq / numSamples);
}

std::vector<int16_t> MicrophoneRecorder::recordWithVAD()
{
  if (!initialized)
  {
    std::cerr << "Error: PortAudio not initialized. Cannot record." << std::endl;
    return {}; // Return empty vector
  }

  const int frameSize = sampleRate * FRAME_DURATION_MS / 1000;
  std::vector<int16_t> frameBuffer(frameSize * channels);
  recordingBuffer_.clear();

  PaStream *stream;
  PaError err = Pa_OpenDefaultStream(&stream,
                                     channels,
                                     0,
                                     paInt16,
                                     sampleRate,
                                     frameSize,
                                     nullptr, nullptr);
  if (err != paNoError)
  {
    std::cerr << "Error opening audio stream: " << Pa_GetErrorText(err) << std::endl;
    return {};
  }

  err = Pa_StartStream(stream);
  if (err != paNoError)
  {
    std::cerr << "Error starting audio stream: " << Pa_GetErrorText(err) << std::endl;
    Pa_CloseStream(stream);
    return {};
  }

  std::cout << "[VAD] Listening for voice..." << std::endl;
  bool recording = false;
  int silenceCount = 0;

  while (true)
  {
    err = Pa_ReadStream(stream, frameBuffer.data(), frameSize);
    if (err != paNoError)
    {
      std::cerr << "Error reading from stream: " << Pa_GetErrorText(err) << std::endl;
      break;
    }

    float energy = computeRMS(frameBuffer.data(), frameSize * channels);

    if (!recording && energy > VAD_START_THRESHOLD_SQ)
    {
      std::cout << "[VAD] Voice detected. Recording..." << std::endl;
      recording = true;
      silenceCount = 0;
    }

    if (recording)
    {
      recordingBuffer_.insert(recordingBuffer_.end(), frameBuffer.begin(), frameBuffer.end());

      if (recordingBuffer_.size() >= MAX_RECORDING_SAMPLES)
      {
        std::cout << "[VAD] Maximum recording length reached. Stopping." << std::endl;
        break;
      }

      if (energy < VAD_STOP_THRESHOLD_SQ)
      {
        silenceCount++;
        if (silenceCount > MAX_SILENCE_FRAMES_BEFORE_STOP)
        {
          std::cout << "[VAD] Sustained silence detected. Stopping recording." << std::endl;
          break;
        }
      }
      else
      {
        silenceCount = 0;
      }
    }
  }

  Pa_StopStream(stream);
  Pa_CloseStream(stream);

  if (!recordingBuffer_.empty())
  {
    std::cout << "[Recorder] Audio recorded: " << recordingBuffer_.size() << " samples" << std::endl;
    return recordingBuffer_;
  }

  std::cout << "[Recorder] No speech detected during recording session." << std::endl;
  return {};
}

bool MicrophoneRecorder::playAudioData(const std::vector<uint8_t> &wavData)
{
  if (!initialized)
  {
    std::cerr << "Error: PortAudio not initialized. Cannot play audio." << std::endl;
    return false;
  }

  if (wavData.size() < 44)
  {
    std::cerr << "Error: Audio data too small to contain valid WAV header." << std::endl;
    return false;
  }

  const uint8_t *header = wavData.data();

  if (std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0)
  {
    std::cerr << "Error: Invalid WAV file signature." << std::endl;
    return false;
  }

  uint16_t numChannels = *reinterpret_cast<const uint16_t *>(header + 22);
  uint32_t wavSampleRate = *reinterpret_cast<const uint32_t *>(header + 24);
  uint16_t bitsPerSample = *reinterpret_cast<const uint16_t *>(header + 34);

  size_t dataOffset = 0;
  for (size_t i = 12; i < wavData.size() - 8; i += 4)
  {
    if (std::memcmp(header + i, "data", 4) == 0)
    {
      dataOffset = i + 8;
      break;
    }
  }

  if (dataOffset == 0)
  {
    std::cerr << "Error: Could not find data chunk in WAV file." << std::endl;
    return false;
  }

  size_t dataSize = wavData.size() - dataOffset;
  size_t bytesPerSample = bitsPerSample / 8;
  size_t numSamples = dataSize / (bytesPerSample * numChannels);

  PaStream *stream;
  PaError err = Pa_OpenDefaultStream(&stream,
                                     0,
                                     numChannels,
                                     paInt16,
                                     wavSampleRate,
                                     paFramesPerBufferUnspecified,
                                     nullptr, nullptr);

  if (err != paNoError)
  {
    std::cerr << "Error opening playback stream: " << Pa_GetErrorText(err) << std::endl;
    return false;
  }

  err = Pa_StartStream(stream);
  if (err != paNoError)
  {
    std::cerr << "Error starting playback stream: " << Pa_GetErrorText(err) << std::endl;
    Pa_CloseStream(stream);
    return false;
  }

  constexpr size_t BUFFER_SIZE_FRAMES = 1024;
  const int16_t *audioData = reinterpret_cast<const int16_t *>(wavData.data() + dataOffset);
  size_t framesPlayed = 0;
  size_t totalFrames = numSamples / numChannels;

  while (framesPlayed < totalFrames)
  {
    size_t framesToPlay = std::min(BUFFER_SIZE_FRAMES, totalFrames - framesPlayed);

    err = Pa_WriteStream(stream, audioData + (framesPlayed * numChannels), framesToPlay);
    if (err != paNoError)
    {
      std::cerr << "Error during playback: " << Pa_GetErrorText(err) << std::endl;
      break;
    }

    framesPlayed += framesToPlay;
  }

  Pa_StopStream(stream);
  Pa_CloseStream(stream);

  return (err == paNoError);
}