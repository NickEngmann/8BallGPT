#include <Arduino.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <QMI8658.h>
#include <DEV_Config.h>
#include "Recorder.h"
#include "Animations.h"
#include <algorithm>
#include <ArduinoJson.h>
#include "TextStateManager.h"

// Display configuration
static const uint16_t screenWidth = 240;
static const uint16_t screenHeight = 240;

// Global objects
WiFiManager wifiManager;
VoiceActivatedRecorder recorder;
AnimationManager animations(screenWidth, screenHeight);
TextStateManager textManager;

// State variables
bool isShaking = false;
bool recordingTriggered = false;
bool isShowingResponse = false;             // Add this to track response state
unsigned long responseStartTime = 0;        // Rename for clarity
unsigned long lastShakeTime = 0;
unsigned long responseDisplayStart = 0;
float acc[3], gyro[3], totalAccel, totalGyro;
unsigned int tim_count = 0;
const float ACCEL_THRESHOLD = 2000.0f;
unsigned long lastShakeCheck = 0;
const int RESPONSE_DISPLAY_DURATION = 15000;

// Magic 8 ball responses
const char *responses[] = {
    "It is certain",
    "Reply hazy, try again",
    "Ask again later",
    "Don't count on it",
    "My reply is no",
    "Outlook not so good",
    "Very doubtful",
    "Concentrate and ask again"};

bool checkForShake()
{
  QMI8658_read_xyz(acc, gyro, &tim_count);
  totalAccel = sqrt((acc[0] * acc[0]) + (acc[1] * acc[1]) + (acc[2] * acc[2]));
  totalGyro = sqrt((gyro[0] * gyro[0]) + (gyro[1] * gyro[1]) + (gyro[2] * gyro[2]));
  return (totalAccel > ACCEL_THRESHOLD);
}

void uploadWAVFile(uint8_t *buffer, size_t bufferSize)
{
  if (!buffer || bufferSize == 0)
  {
    Serial.println("Invalid buffer for upload");
    return;
  }
  unsigned long lastUIUpdate = millis();
  const int UI_UPDATE_INTERVAL = 500; // Update UI every 100ms

  HTTPClient http;

  // Replace with your val.town function URL - you'll get this after deploying the function
  const char *uploadEndpoint = "https://cyrilengmann-efficientcoppercat.web.val.run";

  Serial.printf("Uploading WAV file (%d bytes) to val.town\n", bufferSize);

  http.begin(uploadEndpoint);

  // Set headers for multipart form data
  String boundary = "AudioBoundary";
  String contentType = "multipart/form-data; boundary=" + boundary;
  http.addHeader("Content-Type", contentType);

  // Create the multipart form data
  String head = "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"file\"; filename=\"recording.wav\"\r\n";
  head += "Content-Type: audio/wav\r\n\r\n";

  String tail = "\r\n--" + boundary + "--\r\n";

  // Calculate total size and allocate buffer
  size_t totalSize = head.length() + bufferSize + tail.length();
  uint8_t *postData = (uint8_t *)malloc(totalSize);

  if (!postData)
  {
    Serial.println("Failed to allocate memory for POST data");
    http.end();
    return;
  }

  // Assemble the POST data
  size_t offset = 0;
  memcpy(postData, head.c_str(), head.length());
  offset += head.length();
  memcpy(postData + offset, buffer, bufferSize);
  offset += bufferSize;
  memcpy(postData + offset, tail.c_str(), tail.length());
  unsigned long currentTime = millis();
  lastUIUpdate = currentTime;
  textManager.update(currentTime);
  animations.setLabelText(textManager.getCurrentText().c_str());
  lv_timer_handler();
  // Send the POST request
  int httpResponseCode = http.POST(postData, totalSize);
  free(postData);
  if (httpResponseCode > 0)
  {
    unsigned long currentTime = millis();
    lastUIUpdate = currentTime;
    textManager.update(currentTime);
    animations.setLabelText(textManager.getCurrentText().c_str());
    lv_timer_handler();
    String response = http.getString();

    // Parse the JSON response using ArduinoJson 7.x
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);

    if (!error)
    {
      unsigned long currentTime = millis();
      lastUIUpdate = currentTime;
      textManager.update(currentTime);
      animations.setLabelText(textManager.getCurrentText().c_str());
      lv_timer_handler();
      // Print debug information
      Serial.println("\n=== API Response Debug Info ===");

      // Check if request was successful
      bool success = doc["success"].as<bool>();
      Serial.printf("Success: %s\n", success ? "true" : "false");

      if (success)
      {
        // Print transcription
        const char *transcription = doc["transcription"].as<const char *>();
        if (transcription)
        {
          Serial.printf("Transcription: %s\n", transcription);
        }

        // Print GPT response
        const char *gptResponse = doc["response"].as<const char *>();
        if (gptResponse)
        {
          Serial.printf("GPT Response: %s\n", gptResponse);
          animations.setTriangleColor(0, 0, 255);
          textManager.setState(TextStateManager::DisplayState::RESPONSE, gptResponse);
        }

        // Print timing information
        JsonObject timings = doc["debug"]["timings"];
        if (!timings.isNull())
        {
          Serial.println("\nAPI Timings:");
          Serial.printf("Whisper Duration: %dms\n", timings["whisperDuration"].as<int>());
          Serial.printf("GPT Duration: %dms\n", timings["gptDuration"].as<int>());
          Serial.printf("Total Duration: %dms\n", timings["totalDuration"].as<int>());
        }

        // Print processing steps
        JsonArray steps = doc["debug"]["steps"];
        if (!steps.isNull())
        {
          Serial.println("\nProcessing Steps:");
          for (JsonVariant step : steps)
          {
            Serial.printf("- %s\n", step.as<const char *>());
          }
        }
      }
      else
      {
        // Handle error case
        const char *errorMsg = doc["error"].as<const char *>();
        Serial.printf("\nError: %s\n", errorMsg ? errorMsg : "Unknown error");

        // Print error details if available
        JsonArray errors = doc["debug"]["errors"];
        if (!errors.isNull())
        {
          Serial.println("\nError Details:");
          for (JsonVariant error : errors)
          {
            Serial.printf("- %s\n", error["message"].as<const char *>());
            Serial.printf("  Time: %s\n", error["timestamp"].as<const char *>());
          }
        }

        // Update display with error message
        animations.setTriangleColor(255, 0, 0); // Red for error
        textManager.setState(TextStateManager::DisplayState::RESPONSE,
                             String("Error: processing request"));
      }
    }
    else
    {
      Serial.printf("JSON parsing failed: %s\n", error.c_str());
      Serial.printf("Raw response: %s\n", response.c_str());

      animations.setTriangleColor(255, 0, 0);
      textManager.setState(TextStateManager::DisplayState::RESPONSE,
                           String("Error: Failed to parse response"));
    }
  }
  else
  {
    Serial.printf("HTTP Error: %s\n", http.errorToString(httpResponseCode).c_str());
    animations.setTriangleColor(255, 0, 0);
    textManager.setState(TextStateManager::DisplayState::RESPONSE,
                         String("Error: Failed to connect"));
  }

  http.end();
}

void uploadWAVFileLocal(uint8_t *buffer, size_t bufferSize)
{
  if (!buffer || bufferSize == 0)
  {
    Serial.println("Invalid buffer for upload");
    return;
  }

  HTTPClient http;

  // Configure the upload endpoint - use your computer's IP
  const char *uploadEndpoint = "http://192.168.1.108:5000/upload_wav";

  Serial.printf("Uploading WAV file (%d bytes) to %s\n", bufferSize, uploadEndpoint);

  http.begin(uploadEndpoint);

  // Set headers for multipart form data
  String boundary = "AudioBoundary";
  String contentType = "multipart/form-data; boundary=" + boundary;
  http.addHeader("Content-Type", contentType);

  // Create the multipart form data
  String head = "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"file\"; filename=\"recording.wav\"\r\n";
  head += "Content-Type: audio/wav\r\n\r\n";

  String tail = "\r\n--" + boundary + "--\r\n";

  // Create a temporary buffer for the complete POST data
  size_t totalSize = head.length() + bufferSize + tail.length();
  uint8_t *postData = (uint8_t *)malloc(totalSize);

  if (!postData)
  {
    Serial.println("Failed to allocate memory for POST data");
    http.end();
    return;
  }

  // Copy the parts into the buffer
  size_t offset = 0;

  // Copy header
  memcpy(postData, head.c_str(), head.length());
  offset += head.length();

  // Copy WAV data
  memcpy(postData + offset, buffer, bufferSize);
  offset += bufferSize;

  // Copy tail
  memcpy(postData + offset, tail.c_str(), tail.length());

  // Send the POST request
  int httpResponseCode = http.POST(postData, totalSize);

  // Free the temporary buffer
  free(postData);

  if (httpResponseCode > 0)
  {
    Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    String response = http.getString();
    Serial.println(response);
  }
  else
  {
    Serial.printf("Error on sending POST: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}

// WiFi status update task
void updateWiFiStatus(void *parameter)
{
  TextStateManager *textManager = (TextStateManager *)parameter;
  for (;;)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      textManager->setState(TextStateManager::DisplayState::ERROR);
    }
    else if (textManager->getState() == TextStateManager::DisplayState::ERROR)
    {
      textManager->setState(TextStateManager::DisplayState::IDLE);
    }
    vTaskDelay(pdMS_TO_TICKS(30000));
  }
}

void setup()
{
  Serial.begin(115200);

  if (psramInit())
  {
    Serial.printf("PSRAM initialized. Size: %d MB\n", ESP.getPsramSize() / 1024 / 1024);
    Serial.printf("Free PSRAM: %d KB\n", ESP.getFreePsram() / 1024);
  }
  else
  {
    Serial.println("PSRAM initialization failed!");
  }

  // Initialize hardware
  if (DEV_Module_Init() != 0)
  {
    Serial.println("DEV_Module_Init failed!");
  }
  else
  {
    Serial.println("DEV_Module_Init success!");
  }

  // Initialize sensors
  Serial.println("Initializing QMI8658...");
  if (QMI8658_init())
  {
    Serial.println("QMI8658 init success!");
  }
  else
  {
    Serial.println("QMI8658 init failed!");
  }

  // Initialize display and animations
  if (!animations.begin())
  {
    Serial.println("Failed to initialize display!");
    return;
  }
  animations.initializeTriangle();

  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  IPAddress portalIP(8, 8, 4, 4);
  IPAddress gateway(8, 8, 4, 4);
  IPAddress subnet(255, 255, 255, 0);
  wifiManager.setAPStaticIPConfig(portalIP, gateway, subnet);
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.autoConnect("MagicGPT8Ball");

  // Start WiFi monitoring task
  xTaskCreate(
      updateWiFiStatus,
      "WiFiStatus",
      2048,
      &textManager, // Pass textManager as parameter
      1,
      NULL);

  // Initialize audio recorder
  if (!recorder.begin())
  {
    Serial.println("Failed to initialize audio recorder!");
  }
  else
  {
    Serial.println("Audio recorder initialized successfully");
  }

  // Random seed for responses
  randomSeed(analogRead(2));

  Serial.println("Initialization Complete!");
  textManager.setState(TextStateManager::DisplayState::IDLE);
}

void loop()
{
  unsigned long currentTime = millis();

  // Priority 1: Handle WiFi manager and essential LVGL tasks
  wifiManager.process();
  lv_timer_handler();

  // Priority 2: Handle active recording
  if (recorder.isRecording())
  {
    // When recording, focus on audio capture
    recorder.update();

    // Minimal UI updates during recording (once per second)
    if (currentTime - lastShakeCheck >= 1000)
    {
      lastShakeCheck = currentTime;
      textManager.update(currentTime);
      animations.setLabelText(textManager.getCurrentText().c_str());
    }
    return; // Exit loop to prioritize next recording cycle
  }

  // Normal operation mode (not recording)
  // Update at 60Hz (every ~16ms)
  if (currentTime - lastShakeCheck >= 16)
  {
    lastShakeCheck = currentTime;
    textManager.update(currentTime);

    // Check for shake to start recording
    if (checkForShake() && !recordingTriggered &&
        textManager.getState() == TextStateManager::DisplayState::IDLE)
    {
      // Start new recording session
      recordingTriggered = true;
      isShaking = true;
      animations.setTriangleColor(255, 0, 0);
      textManager.setState(TextStateManager::DisplayState::RECORDING);
      animations.moveToCenter();
      animations.setShaking(true);

      if (recorder.startRecording())
      {
        Serial.println("Recording started");
      }
      else
      {
        Serial.println("Failed to start recording");
      }
    }
    // Check if recording just finished
    else if (recordingTriggered && !recorder.isRecording())
    {
      if (!isShowingResponse)
      {
        isShowingResponse = true;
        responseStartTime = currentTime;
        if (WiFi.status() == WL_CONNECTED)
        {
          // Upload the WAV file
          uint8_t *wavData = recorder.getBuffer();
          size_t wavSize = recorder.getBufferSize();
          Serial.printf("Recording finished. Captured %d bytes\n", wavSize);
          textManager.setState(TextStateManager::DisplayState::THINKING);
          animations.setLabelText(textManager.getCurrentText().c_str());
          lv_timer_handler();
          uploadWAVFile(wavData, wavSize);
        }
        else
        {
          // Show random response
          int responseIndex = random(0, sizeof(responses) / sizeof(responses[0]));
          animations.setTriangleColor(0, 0, 255);
          textManager.setState(TextStateManager::DisplayState::RESPONSE,
                               responses[responseIndex]);
        }
      }
      // Check if we've shown the response long enough
      else if (currentTime - responseStartTime >= RESPONSE_DISPLAY_DURATION)
      {
        // Reset all states
        recordingTriggered = false;
        isShowingResponse = false;
        isShaking = false;
        animations.setShaking(false);
        animations.setTriangleColor(0, 0, 255);
        textManager.setState(TextStateManager::DisplayState::IDLE);
      }
    }

    // Update animations when not recording or transitioning
    if (!isShaking && !animations.isTransitioning())
    {
      QMI8658_read_xyz(acc, gyro, &tim_count);
      animations.updateTrianglePosition(gyro[0], gyro[1]);
    }

    // Update display text
    animations.setLabelText(textManager.getCurrentText().c_str());
  }
}