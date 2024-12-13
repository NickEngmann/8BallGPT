#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include "AudioTools.h"
#include <QMI8658.h>
#include <DEV_Config.h>

// Display configuration
static const uint16_t screenWidth = 240;
static const uint16_t screenHeight = 240;
static const uint16_t triangleSize = screenWidth / 2;

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

// WiFi objects
WiFiManager wifiManager;

// Display buffer configuration
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];
TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);

// Triangle object and animation
lv_obj_t *triangle = nullptr;
lv_obj_t *triangle_label = nullptr;
lv_anim_t anim_x;
lv_anim_t anim_y;

// State variables
bool isShaking = false;
bool isRecording = false;
unsigned long lastShakeTime = 0;
unsigned long phraseStartTime = 0;
const int SHAKE_THRESHOLD = 20;
const char *currentState = "NORMAL"; // NORMAL, SHAKING, SHOWING_PHRASE
float acc[3], gyro[3], totalAccel, totalGyro; // Arrays to store accelerometer and gyroscope data
unsigned int tim_count = 0;          // Timestamp counter
const float ACCEL_THRESHOLD = 3000.0f; // Threshold for shake detection
unsigned long lastShakeCheck = 0;       // For rate limiting shake checks

// Magic 8 ball responses
const char *responses[] = {
    "It is certain",
    "Reply hazy, try again",
    "Ask again later",
    "Don't count on it",
    "My reply is no",
    "Outlook not so good",
    "Very doubtful",
    "Concentrate and ask again"
};

// Animation callbacks
static void anim_x_cb(void *var, int32_t v)
{
  lv_obj_set_x((lv_obj_t *)var, v);
  lv_obj_align_to(triangle_label, triangle, LV_ALIGN_CENTER, 0, 0);
}

static void anim_y_cb(void *var, int32_t v)
{
  lv_obj_set_y((lv_obj_t *)var, v);
  lv_obj_align_to(triangle_label, triangle, LV_ALIGN_CENTER, 0, 0);
}

static void anim_ready_cb(lv_anim_t *a)
{
  lv_coord_t x = lv_obj_get_x(triangle);
  lv_coord_t y = lv_obj_get_y(triangle);

  lv_coord_t new_x = random(0, screenWidth - triangleSize);
  lv_coord_t new_y = random(0, screenHeight - triangleSize);

  uint32_t duration = sqrt(pow(new_x - x, 2) + pow(new_y - y, 2)) * 5;
  duration = constrain(duration, 1000, 3000);

  lv_anim_set_var(&anim_x, triangle);
  lv_anim_set_values(&anim_x, x, new_x);
  lv_anim_set_time(&anim_x, duration);
  lv_anim_start(&anim_x);

  lv_anim_set_var(&anim_y, triangle);
  lv_anim_set_values(&anim_y, y, new_y);
  lv_anim_set_time(&anim_y, duration);
  lv_anim_start(&anim_y);
}

// Audio functions
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

bool checkForShake()
{
  // Read sensor data
  QMI8658_read_xyz(acc, gyro, &tim_count);

  // Calculate total acceleration magnitude
  totalAccel = sqrt((acc[0] * acc[0]) + (acc[1] * acc[1]) + (acc[2] * acc[2]));
  totalGyro = sqrt((gyro[0] * gyro[0]) + (gyro[1] * gyro[1]) + (gyro[2] * gyro[2]));

  // Check if acceleration exceeds threshold
  if (totalAccel > ACCEL_THRESHOLD)
  {
    return true;
  }

  return false;
}

// WiFi status update task
void updateWiFiStatus(void *parameter)
{
  for (;;)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      String ip = WiFi.localIP().toString();
      lv_label_set_text_fmt(triangle_label, "WiFi: Connected\nIP: %s", ip.c_str());
    }
    else
    {
      lv_label_set_text(triangle_label, "DISCONNECTED");
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

// Display flushing
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp_drv);
}

void setup()
{
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  String LVGL_Arduino = "Magic 8GPT Ball - LVGL Version-";
  LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println("Initializing DEV_Module...");
  if (DEV_Module_Init() != 0)
  {
    Serial.println("DEV_Module_Init failed!");
  }
  else{
    Serial.println("DEV_Module_Init successs!");
  }

  Serial.println("Initializing QMI8658_init...");
  if (QMI8658_init())
  {
    Serial.println("QMI8658 init success!");
  }
  else
  {
    Serial.println("QMI8658 init failed!");
  }

  // Initialize LVGL
  Serial.println("Initializing lvgl...");
  lv_init();
  
  // Initialize TFT
  Serial.println("Initializing tft...");
  tft.begin();
  tft.setRotation(0);
  
  // Initialize display buffer
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

  // Initialize display driver
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // Set black background
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);

  // Create triangle
  triangle = lv_obj_create(lv_scr_act());
  lv_obj_set_size(triangle, triangleSize, triangleSize);
  lv_obj_set_style_radius(triangle, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(triangle, lv_color_make(0, 0, 255), LV_PART_MAIN);
  lv_obj_set_style_transform_angle(triangle, 450, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(triangle, LV_OPA_COVER, LV_PART_MAIN);

  // Create WiFi label
  triangle_label = lv_label_create(triangle); // Create label as child of triangle
  lv_obj_set_style_text_color(triangle_label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_align(triangle_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(triangle_label, "DISCONNECTED");

  // Center the label within the triangle
  lv_obj_align(triangle_label, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_long_mode(triangle_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(triangle_label, triangleSize - 20); // Set width with some padding

  // Initialize animations
  lv_anim_init(&anim_x);
  lv_anim_set_exec_cb(&anim_x, anim_x_cb);
  lv_anim_set_path_cb(&anim_x, lv_anim_path_ease_in_out);
  lv_anim_set_ready_cb(&anim_x, anim_ready_cb);

  lv_anim_init(&anim_y);
  lv_anim_set_exec_cb(&anim_y, anim_y_cb);
  lv_anim_set_path_cb(&anim_y, lv_anim_path_ease_in_out);

  // Start initial animation
  anim_ready_cb(&anim_x);
  Serial.println("initializing WiFi...");
  // WiFi setup
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

  Serial.println("...Intialization Complete!!");
}

void loop()
{
  // Add shake detection logic
  unsigned long currentTime = millis();
  if (currentTime - lastShakeCheck >= 50) // Check every 50ms
  { 
    lastShakeCheck = currentTime;

    if (checkForShake() && !isShaking)
    {
      isShaking = true;
      lastShakeTime = currentTime;

      // Change triangle color to indicate shaking
      lv_obj_set_style_bg_color(triangle, lv_color_make(255, 0, 0), LV_PART_MAIN);

      // Display "Shaking..." text
      lv_label_set_text(triangle_label, "Shaking...");
    }
    else if (isShaking && (currentTime - lastShakeTime > 2000))
    {
      // After 2 seconds of shaking, show response
      isShaking = false;

      // Return triangle to original color
      lv_obj_set_style_bg_color(triangle, lv_color_make(0, 0, 255), LV_PART_MAIN);

      // Show random response
      int responseIndex = random(0, sizeof(responses) / sizeof(responses[0]));
      lv_label_set_text(triangle_label, responses[responseIndex]);
    }
    // else {
    //   Serial.printf("\nAcc: X=%.2f Y=%.2f Z=%.2f | Gyro: X=%.2f Y=%.2f Z=%.2f\n Total Acceleration=%.2f\n Total Gyro=%.2F\n", gyro[0], gyro[1], gyro[2], acc[0], acc[1], acc[2], totalAccel, totalGyro);
    // }
  }
  wifiManager.process();
  lv_timer_handler();
  delay(5);
}