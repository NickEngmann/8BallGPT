#include <Arduino.h>
#include <lvgl.h>
// #include <SPIFFS.h>
#include <Adafruit_LSM6DS3TRC.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include "AudioTools.h"

// Display includes
#define USE_TFT_ESPI_LIBRARY
#include "lv_xiao_round_screen.h"

// I2S configuration
const int i2s_bck = 44; // BCLK -> GPIO44 (RX)
const int i2s_ws = -1;  // not
const int i2s_data = 3; // DOUT -> GPIO3 (A2)
const int sample_rate = 44100;
const int channels = 2;
const int bits_per_sample = 16;

// Audio objects
I2SStream i2sStream;
CsvOutput<int32_t> csvStream(Serial);
StreamCopy copier(csvStream, i2sStream);
AudioInfo info(sample_rate, channels, bits_per_sample);

// Global objects
Adafruit_LSM6DS3TRC lsm6ds3trc;
WiFiManager wifiManager;

// Global variables
bool isShaking = false;
bool isRecording = false;
unsigned long lastShakeTime = 0;
unsigned long phraseStartTime = 0;
const int SHAKE_THRESHOLD = 20;
const char *currentState = "NORMAL"; // NORMAL, SHAKING, SHOWING_PHRASE

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

// UI elements
lv_obj_t *wifi_label;
lv_obj_t *magic_label;

// Function declarations
bool checkShaking(sensors_event_t &accel);
void startRecording();
void stopRecording();
void updateWiFiStatus(void *parameter);

// Function to check if device is being shaken
bool checkShaking(sensors_event_t &accel)
{
  float magnitude = sqrt(
      accel.acceleration.x * accel.acceleration.x +
      accel.acceleration.y * accel.acceleration.y +
      accel.acceleration.z * accel.acceleration.z);
  return abs(magnitude - 9.81) > SHAKE_THRESHOLD;
}

void startRecording()
{
  if (!isRecording)
  {
    Serial.println("Starting audio recording...");

    auto config = i2sStream.defaultConfig(RX_MODE);
    config.copyFrom(info);
    config.signal_type = PDM;
    config.use_apll = false;
    config.pin_bck = i2s_bck;
    config.pin_ws = i2s_ws;
    config.pin_data = i2s_data;

    i2sStream.begin(config);
    csvStream.begin(info);
    isRecording = true;
  }
}

void stopRecording()
{
  if (isRecording)
  {
    Serial.println("Stopping audio recording...");
    i2sStream.end();
    csvStream.end();
    isRecording = false;
  }
}

// Callback for WiFi status updates
void updateWiFiStatus(void *parameter)
{
  for (;;)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      String ip = WiFi.localIP().toString();
      lv_label_set_text_fmt(wifi_label, "WiFi: Connected\nIP: %s", ip.c_str());
    }
    else
    {
      lv_label_set_text(wifi_label, "WiFi: Disconnected");
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void setup()
{
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  // Initialize accelerometer
  if (!lsm6ds3trc.begin_I2C())
  {
    Serial.println("Failed to find LSM6DS3TR-C chip");
    while (1)
    {
      delay(10);
    }
  }
  Serial.println("LSM6DS3TR-C Found!");
  lsm6ds3trc.configInt1(false, false, true); // accelerometer DRDY on INT1
  lsm6ds3trc.configInt2(false, true, false); // gyro DRDY on INT2
  // Initialize display
  lv_init();
  lv_xiao_disp_init();
  // lv_xiao_touch_init();

  // WiFi setup
  WiFi.mode(WIFI_STA);
  IPAddress portalIP(8, 8, 4, 4);
  IPAddress gateway(8, 8, 4, 4);
  IPAddress subnet(255, 255, 255, 0);
  wifiManager.setAPStaticIPConfig(portalIP, gateway, subnet);
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.autoConnect("Magic8Ball_AP");

  // Create UI elements
  wifi_label = lv_label_create(lv_scr_act());
  lv_obj_set_pos(wifi_label, 10, 10);
  lv_label_set_text(wifi_label, "WiFi: Disconnected");

  magic_label = lv_label_create(lv_scr_act());
  lv_obj_align(magic_label, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text(magic_label, "Magic 8 Ball");

  // Start WiFi monitoring task
  xTaskCreate(
      updateWiFiStatus,
      "WiFiStatus",
      2048,
      NULL,
      1,
      NULL);
}

void loop()
{
  wifiManager.process();

  sensors_event_t accel;
  sensors_event_t gyro;
  sensors_event_t temp;
  lsm6ds3trc.getEvent(&accel, &gyro, &temp);

  unsigned long currentTime = millis();

  // Check device state and update display accordingly
  if (strcmp(currentState, "NORMAL") == 0)
  {
    if (checkShaking(accel))
    {
      currentState = "SHAKING";
      isShaking = true;
      lastShakeTime = currentTime;
      lv_label_set_text(magic_label, "Shaking");
      startRecording();
      lv_timer_handler();
    }
  }
  else if (strcmp(currentState, "SHAKING") == 0)
  {
    if (!checkShaking(accel))
    {
      if (currentTime - lastShakeTime > 5000)
      {
        currentState = "SHOWING_PHRASE";
        phraseStartTime = currentTime;
        stopRecording();
        int responseIndex = random(8);
        lv_label_set_text(magic_label, responses[responseIndex]);
      }
    }
    else
    {
      lastShakeTime = currentTime;
      // Copy audio data while shaking
      if (isRecording)
      {
        copier.copy();
      }
    }
  }
  else if (strcmp(currentState, "SHOWING_PHRASE") == 0)
  {
    if (currentTime - phraseStartTime > 6000)
    {
      currentState = "NORMAL";
      lv_label_set_text(magic_label, "Magic 8 Ball");
    }
  }

  
  if (strcmp(currentState, "SHAKING") != 0){
    lv_timer_handler();
    delay(50);
  }
}