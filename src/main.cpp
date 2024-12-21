#include <Arduino.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <QMI8658.h>
#include <DEV_Config.h>
#include "Recorder.h"
#include "Animations.h"
#include <algorithm>

// Display configuration
static const uint16_t screenWidth = 240;
static const uint16_t screenHeight = 240;

// Global objects
WiFiManager wifiManager;
VoiceActivatedRecorder recorder;
AnimationManager animations(screenWidth, screenHeight);

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
const int RESPONSE_DISPLAY_DURATION = 4500;

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
  for (;;)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      String ip = WiFi.localIP().toString();
      animations.setLabelTextFormatted("WiFi: Connected\nIP: %s", ip.c_str());
    }
    else
    {
      animations.setLabelText("DISCONNECTED");
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
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
      NULL,
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
}

void loop()
{
  unsigned long currentTime = millis();

  // Check for shake approximately every 16ms (60Hz)
  if (currentTime - lastShakeCheck >= 16)
  {
    lastShakeCheck = currentTime;

    if (checkForShake() && !recordingTriggered && !isShowingResponse)
    {
      // Start new recording session
      recordingTriggered = true;
      isShaking = true;
      animations.setTriangleColor(255, 0, 0); // Red for recording
      animations.setLabelText("Speak now...");
      animations.moveToCenter();
      animations.setShaking(true);

      // Start the voice recording
      if (recorder.startRecording())
      {
        Serial.println("Recording started");
      }
      else
      {
        Serial.println("Failed to start recording");
      }
    }
    else if (recordingTriggered && !recorder.isRecording())
    {
      // Recording has just finished - show response
      if (!isShowingResponse)
      {
        isShowingResponse = true;
        responseStartTime = currentTime;

        // Upload the WAV file
        uint8_t *wavData = recorder.getBuffer();
        size_t wavSize = recorder.getBufferSize();
        uploadWAVFile(wavData, wavSize);

        // Show random response
        int responseIndex = random(0, sizeof(responses) / sizeof(responses[0]));
        animations.setTriangleColor(0, 0, 255);
        animations.setLabelText(responses[responseIndex]);
      }
      // Check if we've shown the response long enough
      else if (currentTime - responseStartTime >= RESPONSE_DISPLAY_DURATION)
      {
        // Reset all states
        recordingTriggered = false;
        isShowingResponse = false;
        isShaking = false;
        animations.setShaking(false);
      }
    }

    // Update triangle position when not shaking or transitioning
    if (!isShaking && !animations.isTransitioning())
    {
      QMI8658_read_xyz(acc, gyro, &tim_count);
      animations.updateTrianglePosition(gyro[0], gyro[1]);
    }
  }

  // Handle ongoing processes
  wifiManager.process();
  lv_timer_handler();

  // Handle audio recording
  if (recorder.isRecording())
  {
    recorder.update();

    // Check if recording just finished
    if (!recorder.isRecording())
    {
      uint8_t *wavData = recorder.getBuffer();
      size_t wavSize = recorder.getBufferSize();

      Serial.printf("Recording finished. Captured %d bytes\n", wavSize);

      // Here you would process the audio or send it to your AI service
      // sendToAIService(wavData, wavSize);
      animations.setLabelText("Processing...");
      delay(1000);
    }
  }

  // Small delay to prevent busy-waiting
  delay(3);
}