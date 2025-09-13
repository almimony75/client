#include "wakeword.hpp"
#include "AppLogger.hpp"
#include <iostream>
#include <thread>
#include <chrono>

PorcupineDetector::PorcupineDetector(const std::string &accessKey,
                                     const std::string &modelPath,
                                     const std::string &keywordPath,
                                     float sensitivity)
    : accessKeyCopy(accessKey),
      modelPathCopy(modelPath),
      keywordPathCopy(keywordPath),
      sensitivity(sensitivity) // Initialize sensitivity member
{
  initializedPorcupine = initializePorcupine();

  if (initializedPorcupine)
  {
    initializedStream = initializeAudioStream();
  }

  overallInitialized = initializedPorcupine && initializedStream;

  if (!overallInitialized)
  {
    AppLogger::getInstance().error("PorcupineDetector: Failed to fully initialize.");
  }
  else
  {
    AppLogger::getInstance().info("PorcupineDetector: Successfully initialized.");
  }
}

PorcupineDetector::~PorcupineDetector()
{
  cleanupAudioStream();
  cleanupPorcupine();
}

bool PorcupineDetector::isInitialized() const
{
  return overallInitialized;
}

bool PorcupineDetector::initializePorcupine()
{
  AppLogger::getInstance().info("PorcupineDetector: Initializing Porcupine engine...");

  const char *keywordPaths[] = {keywordPathCopy.c_str()};
  const float sensitivities[] = {sensitivity};

  pv_status_t status = pv_porcupine_init(
      accessKeyCopy.c_str(),
      modelPathCopy.c_str(),
      1,
      keywordPaths,
      sensitivities,
      &porcupineHandle);

  if (status != PV_STATUS_SUCCESS)
  {
    AppLogger::getInstance().error("PorcupineDetector: Failed to initialize Porcupine engine: " + std::string(pv_status_to_string(status)));
    porcupineHandle = nullptr;
    return false;
  }

  sampleRate = pv_sample_rate();
  frameLength = pv_porcupine_frame_length();

  AppLogger::getInstance().info("PorcupineDetector: Porcupine engine initialized. SampleRate=" + std::to_string(sampleRate) +
                                ", FrameLength=" + std::to_string(frameLength));
  return true;
}

bool PorcupineDetector::initializeAudioStream()
{
  AppLogger::getInstance().info("PorcupineDetector: Initializing PortAudio stream...");

  PaError err = Pa_OpenDefaultStream(&paStream,
                                     channels,
                                     0,
                                     paInt16,
                                     sampleRate,
                                     frameLength,
                                     nullptr,
                                     nullptr);

  if (err != paNoError)
  {
    AppLogger::getInstance().error("PorcupineDetector: Failed to open PortAudio stream: " + std::string(Pa_GetErrorText(err)));
    paStream = nullptr;
    return false;
  }

  err = Pa_StartStream(paStream);
  if (err != paNoError)
  {
    AppLogger::getInstance().error("PorcupineDetector: Failed to start PortAudio stream: " + std::string(Pa_GetErrorText(err)));
    cleanupAudioStream();
    return false;
  }

  AppLogger::getInstance().info("PorcupineDetector: PortAudio stream started successfully.");
  return true;
}

// --- Cleanup PortAudio Stream ---
void PorcupineDetector::cleanupAudioStream()
{
  if (paStream)
  {
    PaError err = Pa_StopStream(paStream);
    if (err != paNoError)
    {
      AppLogger::getInstance().error("PorcupineDetector: Warning: Failed to stop PortAudio stream: " + std::string(Pa_GetErrorText(err)));
    }

    err = Pa_CloseStream(paStream);
    if (err != paNoError)
    {
      AppLogger::getInstance().error("PorcupineDetector: Warning: Failed to close PortAudio stream: " + std::string(Pa_GetErrorText(err)));
    }
    paStream = nullptr;
    initializedStream = false;
    AppLogger::getInstance().info("PorcupineDetector: PortAudio stream cleaned up.");
  }
}

// --- Cleanup Porcupine Engine ---
void PorcupineDetector::cleanupPorcupine()
{
  if (porcupineHandle)
  {
    pv_porcupine_delete(porcupineHandle);
    porcupineHandle = nullptr;
    initializedPorcupine = false;
    AppLogger::getInstance().info("PorcupineDetector: Porcupine engine cleaned up.");
  }
}

// --- Main Wake Word Detection Loop ---
void PorcupineDetector::run(const std::function<void()> &onWakeWord)
{
  std::vector<int16_t> pcmBuffer(frameLength);

  AppLogger::getInstance().info("PorcupineDetector: Listening for wake word...");

  while (true)
  {
    if (!overallInitialized)
    {
      AppLogger::getInstance().error("PorcupineDetector: Not initialized. Attempting re-initialization...");
      cleanupAudioStream();
      cleanupPorcupine();

      initializedPorcupine = initializePorcupine();
      if (initializedPorcupine)
      {
        initializedStream = initializeAudioStream();
      }
      overallInitialized = initializedPorcupine && initializedStream;

      if (!overallInitialized)
      {
        AppLogger::getInstance().error("PorcupineDetector: Re-initialization failed. Retrying in 5 seconds...");
        std::this_thread::sleep_for(std::chrono::seconds(5));
        continue;
      }
      else
      {
        AppLogger::getInstance().info("PorcupineDetector: Re-initialization successful. Resuming listening.");
      }
    }

    if (overallInitialized)
    {
      PaError err = Pa_ReadStream(paStream, pcmBuffer.data(), frameLength);
      if (err != paNoError)
      {
        AppLogger::getInstance().error("PorcupineDetector: PortAudio read error: " + std::string(Pa_GetErrorText(err)));
        cleanupAudioStream();
        initializedStream = initializeAudioStream();
        overallInitialized = initializedPorcupine && initializedStream;

        if (!initializedStream)
        {
          AppLogger::getInstance().error("PorcupineDetector: Failed to recover audio stream. Waiting to retry...");
          std::this_thread::sleep_for(std::chrono::seconds(3));
        }
        continue;
      }

      int32_t keywordIndex = -1;
      pv_status_t status = pv_porcupine_process(porcupineHandle, pcmBuffer.data(), &keywordIndex);
      if (status != PV_STATUS_SUCCESS)
      {
        AppLogger::getInstance().error("PorcupineDetector: Error processing audio frame: " + std::string(pv_status_to_string(status)));
        AppLogger::getInstance().error("PorcupineDetector: Porcupine processing error. Attempting full re-initialization.");
        overallInitialized = false;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        continue;
      }

      if (keywordIndex != -1)
      {
        AppLogger::getInstance().info("PorcupineDetector: Wake word detected (keyword index: " + std::to_string(keywordIndex) + ")!");
        onWakeWord();
        AppLogger::getInstance().info("PorcupineDetector: Resuming listening for wake word...");
      }
    }
  }
}