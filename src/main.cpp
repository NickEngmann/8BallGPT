#include <Arduino.h>
#include <lvgl.h>
#include <SPIFFS.h>

#include <Adafruit_LSM6DS3TRC.h>

Adafruit_LSM6DS3TRC lsm6ds3trc;

// uncomment a library for display driver
#define USE_TFT_ESPI_LIBRARY
// #define USE_ARDUINO_GFX_LIBRARY

#include "lv_xiao_round_screen.h"
#include "lv_hardware_test.h"
#include <WiFiManager.h>
#include <WiFi.h>

// Global variables
WiFiManager wifiManager;
bool isShaking = false;
unsigned long lastShakeTime = 0;
unsigned long phraseStartTime = 0;
const int SHAKE_THRESHOLD = 20;      // Adjust this value based on testing
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

// WiFi status label
lv_obj_t *wifi_label;

// Magic 8 Ball label
lv_obj_t *magic_label;

// Function to check if device is being shaken
bool checkShaking(sensors_event_t &accel)
{
  float magnitude = sqrt(
      accel.acceleration.x * accel.acceleration.x +
      accel.acceleration.y * accel.acceleration.y +
      accel.acceleration.z * accel.acceleration.z);
  return abs(magnitude - 9.81) > SHAKE_THRESHOLD;
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
  Serial.begin(115200); // prepare for possible serial debug
  Serial.println("XIAO round screen - LVGL_Arduino");
  while (!Serial)
    delay(10); // will pause Zero, Leonardo, etc until serial console opens

  Serial.println("Adafruit LSM6DS3TR-C test!");

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
  lv_init();

  lv_xiao_disp_init();
  lv_xiao_touch_init();

  // WiFi setup
  wifiManager.setConfigPortalBlocking(false);
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  // Set custom IP for portal
  IPAddress portalIP(8, 8, 4, 4);
  IPAddress gateway(8, 8, 4, 4);
  IPAddress subnet(255, 255, 255, 0);
  wifiManager.setAPStaticIPConfig(portalIP, gateway, subnet);
  wifiManager.autoConnect("Magic8Ball_AP");

  // Create WiFi status label
  wifi_label = lv_label_create(lv_scr_act());
  lv_obj_set_pos(wifi_label, 10, 10);
  lv_label_set_text(wifi_label, "WiFi: Disconnected");

  // Create Magic 8 Ball label
  magic_label = lv_label_create(lv_scr_act());
  lv_obj_align(magic_label, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text(magic_label, "Magic 8 Ball");

  // Start WiFi status monitoring task
  xTaskCreate(
      updateWiFiStatus,
      "WiFiStatus",
      2048,
      NULL,
      1,
      NULL);
}

// Replace your loop() function with:
void loop()
{
  // Handle WiFi configuration portal
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
        // Select random response
        int responseIndex = random(8);
        lv_label_set_text(magic_label, responses[responseIndex]);
      }
    }
    else
    {
      lastShakeTime = currentTime;
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

  lv_timer_handler();
  delay(50); // Small delay to prevent overwhelming the processor
}
