#include "recorder.hpp"
#include "client.hpp"
#include "AppLogger.hpp"
#include "wakeword.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <stdexcept>

// hardcoded configuration constants
const std::string kLogFile = "client.log";

// orchestrator network details
const std::string kOrchestratorHost = "XXXXX";
const int kOrchestratorPort = 9000;
const std::string kOrchestratorProcessAudioPath = "/process-audio";
const std::string kOrchestratorHealthCheckPath = "/health";
const std::string kAuthToken = "super_secret_token_for_prototype";

// porcupine Wake Word Detector details
const std::string kPorcupineAccessKey = "XXXX";
const std::string kPorcupineModelPath = "models/porcupine_params.pv";
const std::string kPorcupineKeywordPath = "keywords/XXXXXXXXXXX";
const float kPorcupineSensitivity = 0.5f;

// retry delays and attempts for persistent operation
const std::chrono::seconds kNetworkRetryDelay = std::chrono::seconds(3);
const std::chrono::seconds kAudioInitRetryDelay = std::chrono::seconds(5);
const std::chrono::seconds kLoopIdleDelay = std::chrono::seconds(1);
const int kMaxPostRetries = 5;

// debug flag set to true to save audio files for debugging
const bool kSaveDebugAudioFiles = false;

// constants for debug file paths (used only if kSaveDebugAudioFiles is true)
const std::string kAudioDirectory = "audio/";
const std::string kOutputWavFile = kAudioDirectory + "output.wav";
const std::string kResponseWavFile = kAudioDirectory + "response.wav";

// error speaking helper
void speak_error(const std::string &message)
{
  std::string command = "espeak-ng -v en-US+f3 -s 150 \"" + message + "\" 2>/dev/null";
  AppLogger::getInstance().info("speaking error: \"" + message + "\"");
  if (std::system(command.c_str()) != 0)
  {
    AppLogger::getInstance().error("failed to execute espeak-ng command. Is espeak-ng installed?");
  }
}

// orchestrator connectivity Check
bool is_orchestrator_reachable()
{
  AppLogger::getInstance().info("Checking orchestrator connectivity...");
  httplib::Client temp_cli(kOrchestratorHost, kOrchestratorPort);
  temp_cli.set_connection_timeout(std::chrono::seconds(3));
  temp_cli.set_read_timeout(std::chrono::seconds(3));

  httplib::Headers headers;
  headers.emplace("X-Auth", kAuthToken);
  auto res = temp_cli.Get(kOrchestratorHealthCheckPath.c_str(), headers);

  if (res && res->status == 200)
  {
    AppLogger::getInstance().info("Orchestrator is reachable.");
    return true;
  }
  else
  {
    std::string error_msg = "Orchestrator not reachable.";
    if (res)
    {
      error_msg += " Status: " + std::to_string(res->status);
    }
    else
    {
      error_msg += " Error: " + httplib::to_string(res.error());
    }
    AppLogger::getInstance().error(error_msg);
    return false;
  }
}

// optional debug file saving
void saveDebugAudioFile(const std::vector<int16_t> &audioData, const std::string &filename)
{
  if (kSaveDebugAudioFiles && !audioData.empty())
  {
    std::ofstream file(filename, std::ios::binary);
    if (file.is_open())
    {
      // Create a simple WAV header
      const int sampleRate = 16000;
      const int numChannels = 1;
      const int bitsPerSample = 16;
      const size_t dataSize = audioData.size() * sizeof(int16_t);
      const size_t fileSize = 36 + dataSize;
      const short blockAlign = numChannels * (bitsPerSample / 8);
      const int byteRate = sampleRate * blockAlign;

      // Write WAV header
      file << "RIFF";
      file.write(reinterpret_cast<const char *>(&fileSize), 4);
      file << "WAVE";
      file << "fmt ";
      const uint32_t subchunk1Size = 16;
      file.write(reinterpret_cast<const char *>(&subchunk1Size), 4);
      const uint16_t audioFormat = 1;
      file.write(reinterpret_cast<const char *>(&audioFormat), 2);
      file.write(reinterpret_cast<const char *>(&numChannels), 2);
      file.write(reinterpret_cast<const char *>(&sampleRate), 4);
      file.write(reinterpret_cast<const char *>(&byteRate), 4);
      file.write(reinterpret_cast<const char *>(&blockAlign), 2);
      file.write(reinterpret_cast<const char *>(&bitsPerSample), 2);
      file << "data";
      file.write(reinterpret_cast<const char *>(&dataSize), 4);
      file.write(reinterpret_cast<const char *>(audioData.data()), dataSize);
      file.close();
      AppLogger::getInstance().info("Debug audio saved to: " + filename);
    }
    else
    {
      AppLogger::getInstance().error("Failed to save debug audio to: " + filename);
    }
  }
}

void saveDebugAudioFile(const std::vector<uint8_t> &wavData, const std::string &filename)
{
  if (kSaveDebugAudioFiles && !wavData.empty())
  {
    std::ofstream file(filename, std::ios::binary);
    if (file.is_open())
    {
      file.write(reinterpret_cast<const char *>(wavData.data()), wavData.size());
      file.close();
      AppLogger::getInstance().info("Debug WAV saved to: " + filename);
    }
    else
    {
      AppLogger::getInstance().error("Failed to save debug WAV to: " + filename);
    }
  }
}

int main()
{
  AppLogger::getInstance().open(kLogFile);
  AppLogger::getInstance().info("Client application starting with in-memory audio processing...");

  std::ios_base::sync_with_stdio(false);
  std::cin.tie(NULL);

  if (kSaveDebugAudioFiles)
  {
    std::error_code ec_dir;
    std::filesystem::create_directories(kAudioDirectory, ec_dir);
    if (ec_dir)
    {
      AppLogger::getInstance().error("Failed to create audio directory " + kAudioDirectory + ": " + ec_dir.message());
      speak_error("Failed to create audio directory. Check permissions.");
    }
  }

  MicrophoneRecorder recorder;
  if (!recorder.isInitialized())
  {
    AppLogger::getInstance().error("PortAudio global initialization failed via MicrophoneRecorder. This is critical.");
    speak_error("Core audio system failed to initialize. Please check logs.");
    return 1;
  }

  HttpClient http_client(kOrchestratorHost, kOrchestratorPort);

  PorcupineDetector porcupine_detector(
      kPorcupineAccessKey,
      kPorcupineModelPath,
      kPorcupineKeywordPath,
      kPorcupineSensitivity);

  while (true)
  {
    AppLogger::getInstance().info("--- New application cycle initiated ---");

    while (!is_orchestrator_reachable())
    {
      AppLogger::getInstance().error("Orchestrator is not reachable. Retrying connectivity check in " +
                                     std::to_string(kNetworkRetryDelay.count()) + " seconds...");
      speak_error("Orchestrator not available. Retrying network.");
      std::this_thread::sleep_for(kNetworkRetryDelay);
    }

    if (!porcupine_detector.isInitialized())
    {
      AppLogger::getInstance().error("PorcupineDetector is not initialized or failed during prior run. Re-attempting setup.");
      speak_error("Wake word system failed. Retrying.");
      std::this_thread::sleep_for(kAudioInitRetryDelay);
      continue;
    }

    try
    {
      porcupine_detector.run([&]() {
        AppLogger::getInstance().info("Wake word detected! Initiating command processing sequence.");

        AppLogger::getInstance().info("Recording voice command in-memory...");
        std::vector<int16_t> audioData = recorder.recordWithVAD();

        if (audioData.empty())
        {
          AppLogger::getInstance().error("Recording failed or no speech detected for command. Skipping command processing.");
          speak_error("Could not record your command.");
          return;
        }

        AppLogger::getInstance().info("Voice command recorded: " + std::to_string(audioData.size()) + " samples");

        saveDebugAudioFile(audioData, kOutputWavFile);

        AppLogger::getInstance().info("Sending recorded command audio to orchestrator (in-memory)...");
        int post_retries = 0;
        bool post_success = false;

        while (post_retries < kMaxPostRetries)
        {
          if (http_client.postOrch(kOrchestratorProcessAudioPath, audioData, 16000, 1))
          {
            post_success = true;
            AppLogger::getInstance().info("Command audio successfully sent and response received (in-memory).");
            break;
          }
          else
          {
            post_retries++;
            AppLogger::getInstance().error("Failed to post command audio to orchestrator (attempt " +
                                           std::to_string(post_retries) + "). Retrying in " +
                                           std::to_string(kNetworkRetryDelay.count()) + " seconds...");
            speak_error("Failed to send command. Retrying.");
            std::this_thread::sleep_for(kNetworkRetryDelay);
          }
        }

        if (!post_success)
        {
          AppLogger::getInstance().error("Maximum post retries reached. Command not sent.");
          speak_error("Failed to send command after multiple tries.");
          return;
        }

        AppLogger::getInstance().info("Playing response audio from memory...");
        std::vector<uint8_t> responseAudio = http_client.getLastResponseAudio();

        if (!responseAudio.empty())
        {
          saveDebugAudioFile(responseAudio, kResponseWavFile);

          if (!recorder.playAudioData(responseAudio))
          {
            AppLogger::getInstance().error("Failed to play response audio from memory.");
            speak_error("Failed to play response.");
          }
          else
          {
            AppLogger::getInstance().info("Response audio played successfully from memory.");
          }
        }
        else
        {
          AppLogger::getInstance().error("No response audio received from orchestrator.");
          speak_error("No audio response received.");
        }

        AppLogger::getInstance().info("Command sequence completed (in-memory). Returning to wake word detection.");
      });

      AppLogger::getInstance().error("PorcupineDetector.run() exited unexpectedly. Re-evaluating.");
      speak_error("Wake word detection loop stopped. Attempting restart.");
    }
    catch (const std::exception &e)
    {
      AppLogger::getInstance().error("Unhandled exception caught in main loop: " + std::string(e.what()));
      speak_error("An unexpected critical error occurred. Restarting systems.");
      std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    std::this_thread::sleep_for(kLoopIdleDelay);
  }

  return 0;
}