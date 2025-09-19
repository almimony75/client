#include "recorder.hpp"
#include "client.hpp"
#include "AppLogger.hpp"
#include "wakeword.hpp"
#include "configLoader.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <stdexcept>

void speak_error(const std::string &message)
{
  std::string command = "espeak-ng -v en-US+f3 -s 150 \"" + message + "\" 2>/dev/null";
  AppLogger::getInstance().info("speaking error: \"" + message + "\"");
  if (std::system(command.c_str()) != 0)
  {
    AppLogger::getInstance().error("failed to execute espeak-ng command. Is espeak-ng installed?");
  }
}

bool is_orchestrator_reachable(const std::string &host, int port, const std::string &healthPath, const std::string &authToken)
{
  AppLogger::getInstance().info("Checking orchestrator connectivity...");
  httplib::Client temp_cli(host, port);
  temp_cli.set_connection_timeout(std::chrono::seconds(3));
  temp_cli.set_read_timeout(std::chrono::seconds(3));

  httplib::Headers headers;
  headers.emplace("X-Auth", authToken);
  auto res = temp_cli.Get(healthPath.c_str(), headers);

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

void saveDebugAudioFile(bool shouldSave, const std::vector<int16_t> &audioData, const std::string &filename)
{
  if (shouldSave && !audioData.empty())
  {
    std::ofstream file(filename, std::ios::binary);
    if (file.is_open())
    {
      const int sampleRate = 16000;
      const int numChannels = 1;
      const int bitsPerSample = 16;
      const size_t dataSize = audioData.size() * sizeof(int16_t);
      const size_t fileSize = 36 + dataSize;
      const short blockAlign = numChannels * (bitsPerSample / 8);
      const int byteRate = sampleRate * blockAlign;

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

void saveDebugAudioFile(bool shouldSave, const std::vector<uint8_t> &wavData, const std::string &filename)
{
  if (shouldSave && !wavData.empty())
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
  ConfigLoader config;
  if (!config.loadFromFile("client.conf"))
  {
    speak_error("Configuration file not found or invalid.");
    return 1;
  }

  AppLogger::getInstance().open(config.getString("logFile", "client.log"));
  AppLogger::getInstance().info("Client application starting...");

  std::ios_base::sync_with_stdio(false);
  std::cin.tie(NULL);

  if (config.getBool("saveDebugAudioFiles", false))
  {
    std::error_code ec_dir;
    std::filesystem::create_directories(config.getString("debug.audioDirectory", "audio/"), ec_dir);
    if (ec_dir)
    {
      AppLogger::getInstance().error("Failed to create audio directory: " + ec_dir.message());
      speak_error("Failed to create audio directory. Check permissions.");
    }
  }

  MicrophoneRecorder recorder;
  if (!recorder.isInitialized())
  {
    AppLogger::getInstance().error("PortAudio global initialization failed. This is critical.");
    speak_error("Core audio system failed to initialize. Please check logs.");
    return 1;
  }

  HttpClient http_client(
      config.getString("orchestrator.host", "127.0.0.1"),
      config.getInt("orchestrator.port", 9000),
      config.getString("orchestrator.authToken", ""));

  PorcupineDetector porcupine_detector(
      config.getString("porcupine.accessKey", ""),
      config.getString("porcupine.modelPath", "models/porcupine_params.pv"),
      config.getString("porcupine.keywordPath", ""),
      config.getFloat("porcupine.sensitivity", 0.5f));

  while (true)
  {
    AppLogger::getInstance().info("--- New application cycle initiated ---");

    while (!is_orchestrator_reachable(
        config.getString("orchestrator.host", "127.0.0.1"),
        config.getInt("orchestrator.port", 9000),
        config.getString("orchestrator.healthCheckPath", "/health"),
        config.getString("orchestrator.authToken", "")))
    {
      int delay = config.getInt("retry.networkDelaySeconds", 3);
      AppLogger::getInstance().error("Orchestrator is not reachable. Retrying in " + std::to_string(delay) + " seconds...");
      speak_error("Orchestrator not available. Retrying network.");
      std::this_thread::sleep_for(std::chrono::seconds(delay));
    }

    if (!porcupine_detector.isInitialized())
    {
      int delay = config.getInt("retry.audioInitDelaySeconds", 5);
      AppLogger::getInstance().error("PorcupineDetector is not initialized. Retrying setup.");
      speak_error("Wake word system failed. Retrying.");
      std::this_thread::sleep_for(std::chrono::seconds(delay));
      continue;
    }

    try
    {
      porcupine_detector.run([&]()
                             {
        AppLogger::getInstance().info("Wake word detected! Initiating command processing sequence.");
        std::vector<int16_t> audioData = recorder.recordWithVAD();

        if (audioData.empty())
        {
          AppLogger::getInstance().error("Recording failed or no speech detected. Skipping.");
          speak_error("Could not record your command.");
          return;
        }

        AppLogger::getInstance().info("Voice command recorded: " + std::to_string(audioData.size()) + " samples");
        saveDebugAudioFile(config.getBool("saveDebugAudioFiles", false), audioData, config.getString("debug.outputWavFile", "audio/output.wav"));

        AppLogger::getInstance().info("Sending recorded command audio to orchestrator...");
        int post_retries = 0;
        bool post_success = false;
        const int maxPostRetries = config.getInt("retry.maxPostRetries", 5);
        const auto networkRetryDelay = std::chrono::seconds(config.getInt("retry.networkDelaySeconds", 3));
        const std::string processAudioPath = config.getString("orchestrator.processAudioPath", "/process-audio");

        while (post_retries < maxPostRetries)
        {
          if (http_client.postOrch(processAudioPath, audioData, 16000, 1))
          {
            post_success = true;
            AppLogger::getInstance().info("Command audio successfully sent.");
            break;
          }
          else
          {
            post_retries++;
            AppLogger::getInstance().error("Failed to post command audio (attempt " + std::to_string(post_retries) + "). Retrying...");
            speak_error("Failed to send command. Retrying.");
            std::this_thread::sleep_for(networkRetryDelay);
          }
        }

        if (!post_success)
        {
          AppLogger::getInstance().error("Maximum post retries reached. Command not sent.");
          speak_error("Failed to send command after multiple tries.");
          return;
        }

        AppLogger::getInstance().info("Playing response audio...");
        std::vector<uint8_t> responseAudio = http_client.getLastResponseAudio();

        if (!responseAudio.empty())
        {
          saveDebugAudioFile(config.getBool("saveDebugAudioFiles", false), responseAudio, config.getString("debug.responseWavFile", "audio/response.wav"));
          if (!recorder.playAudioData(responseAudio))
          {
            AppLogger::getInstance().error("Failed to play response audio.");
            speak_error("Failed to play response.");
          }
          else
          {
            AppLogger::getInstance().info("Response audio played successfully.");
          }
        }
        else
        {
          AppLogger::getInstance().error("No response audio received from orchestrator.");
          speak_error("No audio response received.");
        }
        AppLogger::getInstance().info("Command sequence completed."); });

      AppLogger::getInstance().error("PorcupineDetector.run() exited unexpectedly.");
      speak_error("Wake word detection loop stopped. Attempting restart.");
    }
    catch (const std::exception &e)
    {
      AppLogger::getInstance().error("Unhandled exception in main loop: " + std::string(e.what()));
      speak_error("An unexpected critical error occurred. Restarting systems.");
      std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    std::this_thread::sleep_for(std::chrono::seconds(config.getInt("retry.loopIdleDelaySeconds", 1)));
  }

  return 0;
}