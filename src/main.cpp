#include <Arduino.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include "AudioTools.h"
#include <QMI8658.h>
#include <DEV_Config.h>
#include "Recorder.h"
#include "Animations.h"

// Display configuration
static const uint16_t screenWidth = 240;
static const uint16_t screenHeight = 240;

// Global objects
WiFiManager wifiManager;
VoiceActivatedRecorder recorder;
AnimationManager animations(screenWidth, screenHeight);

// State variables
bool isShaking = false;
unsigned long lastShakeTime = 0;
unsigned long responseDisplayStart = 0;
float acc[3], gyro[3], totalAccel, totalGyro;
unsigned int tim_count = 0;
const float ACCEL_THRESHOLD = 3000.0f;
unsigned long lastShakeCheck = 0;
const int RESPONSE_DISPLAY_DURATION = 3000;

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
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
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

  Serial.println("Initialization Complete!");
}

void loop()
{
  unsigned long currentTime = millis();

  if (currentTime - lastShakeCheck >= 16)
  { // ~60fps update rate
    lastShakeCheck = currentTime;

    if (checkForShake() && !isShaking)
    {
      isShaking = true;
      lastShakeTime = currentTime;
      responseDisplayStart = 0;

      // Start recording when shake is detected
      animations.setTriangleColor(255, 0, 0); // Red
      animations.setLabelText("Speak...");
      animations.moveToCenter();
      animations.setShaking(true);
      recorder.startRecording();
    }
    else if (isShaking && (currentTime - lastShakeTime > 2000))
    {
      if (responseDisplayStart == 0)
      {
        responseDisplayStart = currentTime;
        int responseIndex = random(0, sizeof(responses) / sizeof(responses[0]));
        animations.setTriangleColor(0, 0, 255); // Blue
        animations.setLabelText(responses[responseIndex]);
      }

      if (currentTime - responseDisplayStart >= RESPONSE_DISPLAY_DURATION)
      {
        isShaking = false;
        animations.setShaking(false);
      }
    }

    if (!isShaking && !animations.isTransitioning())
    {
      QMI8658_read_xyz(acc, gyro, &tim_count);
      animations.updateTrianglePosition(gyro[0], gyro[1]);
    }
  }

  // Handle WiFi
  wifiManager.process();
  // Update LVGL
  lv_timer_handler();
  // Handle audio recording
  if (recorder.isRecording())
  {
    recorder.update();

    // If recording stops, process the audio
    if (!recorder.isRecording())
    {
      uint8_t *wavData = recorder.getBuffer();
      size_t wavSize = recorder.getBufferSize();

      // Send to AI service if connected to WiFi
      // sendToAIService(wavData, wavSize);
      animations.setLabelText("Processing");
    }
  }
  else
  {
    delay(3); // Slight delay for smoother motion when not recording
  }
}