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
static lv_coord_t current_x = 0;
static lv_coord_t current_y = 0;

bool isShaking = false;
bool isTransitioningToCenter = false;
bool isRecording = false;
unsigned long lastShakeTime = 0;
unsigned long phraseStartTime = 0;
unsigned long responseDisplayStart = 0;
const int SHAKE_THRESHOLD = 20;
const int RESPONSE_DISPLAY_DURATION = 3000; // How long to show response in center
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
    "Concentrate and ask again"};

// Animation callbacks for transitioning to center
static void anim_x_cb(void *var, int32_t v)
{
  current_x = v;
  lv_obj_set_x((lv_obj_t *)var, v);
  lv_obj_align_to(triangle_label, triangle, LV_ALIGN_CENTER, 0, 0);
}

static void anim_y_cb(void *var, int32_t v)
{
  current_y = v;
  lv_obj_set_y((lv_obj_t *)var, v);
  lv_obj_align_to(triangle_label, triangle, LV_ALIGN_CENTER, 0, 0);
}

static void anim_ready_cb(lv_anim_t *a)
{
  isTransitioningToCenter = false;
}
const float MOTION_SMOOTHING = 0.95f; // Higher = smoother (0.0 to 1.0)
const float GYRO_SENSITIVITY = 0.08f; // Reduced from 0.5f for gentler motion
const float IDLE_AMPLITUDE = 15.0f;   // Maximum idle movement in pixels
const float IDLE_SPEED = 0.001f;      // Speed of idle animation
const float SCREEN_MARGIN = 0.75f;    // How much of triangle can go off screen (0.0 to 1.0)

// Motion state variables
static float velocity_x = 0.0f;
static float velocity_y = 0.0f;
static float target_x = 0.0f;
static float target_y = 0.0f;
static float idle_time = 0.0f;

// Add these new functions for motion control:

void updateIdleMotion()
{
  // Create a smooth, water-like motion using sine waves
  idle_time += IDLE_SPEED;

  float offset_x = sin(idle_time) * cos(idle_time * 0.7f) * IDLE_AMPLITUDE;
  float offset_y = cos(idle_time * 1.3f) * sin(idle_time * 0.5f) * IDLE_AMPLITUDE;

  target_x += offset_x * 0.01f; // Small influence from idle motion
  target_y += offset_y * 0.01f;
}

void moveToCenter()
{
  if (!isTransitioningToCenter)
  {
    isTransitioningToCenter = true;

    // Calculate position in bottom third of screen
    target_x = (screenWidth - triangleSize) / 2;
    target_y = (screenHeight * 2 / 3) - (triangleSize / 2); // Position in bottom third

    // Reset velocities
    velocity_x = 0;
    velocity_y = 0;

    // Set up X animation
    lv_anim_set_var(&anim_x, triangle);
    lv_anim_set_values(&anim_x, current_x, target_x);
    lv_anim_set_time(&anim_x, 800); // Slower animation
    lv_anim_set_exec_cb(&anim_x, anim_x_cb);
    lv_anim_set_path_cb(&anim_x, lv_anim_path_ease_out);
    lv_anim_start(&anim_x);

    // Set up Y animation
    lv_anim_set_var(&anim_y, triangle);
    lv_anim_set_values(&anim_y, current_y, target_y);
    lv_anim_set_time(&anim_y, 800); // Slower animation
    lv_anim_set_exec_cb(&anim_y, anim_y_cb);
    lv_anim_set_path_cb(&anim_y, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&anim_y, anim_ready_cb);
    lv_anim_start(&anim_y);
  }
}

// Update position based on gyroscope data
void updateTrianglePosition()
{
  if (!triangle || isShaking || isTransitioningToCenter)
    return;

  QMI8658_read_xyz(acc, gyro, &tim_count);

  // Detect if device is relatively still
  bool is_still = abs(gyro[0]) < 0.5f && abs(gyro[1]) < 0.5f;

  if (is_still)
  {
    updateIdleMotion();
  }
  else
  {
    // Update target position based on gyro with reduced sensitivity
    target_x += gyro[1] * GYRO_SENSITIVITY;
    target_y += gyro[0] * GYRO_SENSITIVITY;
  }

  // Calculate screen boundaries with margins
  float margin_x = triangleSize * (1.0f - SCREEN_MARGIN);
  float margin_y = triangleSize * (1.0f - SCREEN_MARGIN);

  // Constrain target positions
  target_x = constrain(target_x, -margin_x, screenWidth - triangleSize + margin_x);
  target_y = constrain(target_y, -margin_y, screenHeight - triangleSize + margin_y);

  // Smooth velocity changes
  float dx = target_x - current_x;
  float dy = target_y - current_y;

  velocity_x = velocity_x * MOTION_SMOOTHING + dx * (1.0f - MOTION_SMOOTHING);
  velocity_y = velocity_y * MOTION_SMOOTHING + dy * (1.0f - MOTION_SMOOTHING);

  // Update position with velocity
  current_x += velocity_x * 0.1f; // Further smooth the motion
  current_y += velocity_y * 0.1f;

  // Update triangle position
  lv_obj_set_pos(triangle, (int16_t)current_x, (int16_t)current_y);
  lv_obj_align_to(triangle_label, triangle, LV_ALIGN_CENTER, 0, 0);
}

void initTrianglePosition()
{
  current_x = (screenWidth - triangleSize) / 2;
  current_y = (screenHeight - triangleSize) / 2;
  lv_obj_set_pos(triangle, current_x, current_y);
  lv_obj_align_to(triangle_label, triangle, LV_ALIGN_CENTER, 0, 0);
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
  lv_init();
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
  triangle_label = lv_label_create(triangle);
  lv_obj_set_style_text_color(triangle_label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_align(triangle_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(triangle_label, "DISCONNECTED");
  lv_obj_align(triangle_label, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_long_mode(triangle_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(triangle_label, triangleSize - 20);

  // Initialize animations
  lv_anim_init(&anim_x);
  lv_anim_init(&anim_y);
  // Initialize triangle position
  initTrianglePosition();

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
  unsigned long currentTime = millis();

  if (currentTime - lastShakeCheck >= 16)
  { // ~60fps update rate
    lastShakeCheck = currentTime;

    if (checkForShake() && !isShaking)
    {
      isShaking = true;
      lastShakeTime = currentTime;
      responseDisplayStart = 0;

      lv_obj_set_style_bg_color(triangle, lv_color_make(255, 0, 0), LV_PART_MAIN);
      lv_label_set_text(triangle_label, "Shaking...");
      moveToCenter();
    }
    else if (isShaking && (currentTime - lastShakeTime > 2000))
    {
      if (responseDisplayStart == 0)
      {
        responseDisplayStart = currentTime;
        int responseIndex = random(0, sizeof(responses) / sizeof(responses[0]));
        lv_obj_set_style_bg_color(triangle, lv_color_make(0, 0, 255), LV_PART_MAIN);
        lv_label_set_text(triangle_label, responses[responseIndex]);
      }

      if (currentTime - responseDisplayStart >= RESPONSE_DISPLAY_DURATION)
      {
        isShaking = false;
        // Reset velocities when returning to normal motion
        velocity_x = 0;
        velocity_y = 0;
      }
    }

    if (!isShaking && !isTransitioningToCenter)
    {
      updateTrianglePosition();
    }
  }

  wifiManager.process();
  lv_timer_handler();
  delay(3); // Slightly reduced delay for smoother motion
}