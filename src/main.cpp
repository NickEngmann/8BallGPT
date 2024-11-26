/*Using LVGL with Arduino requires some extra steps:
 *Be sure to read the docs here: https://docs.lvgl.io/master/get-started/platforms/arduino.html  */
#include <Arduino.h>
#include <lvgl.h>
// #include <SPIFFS.h>
#include <TFT_eSPI.h>
#include <lv_conf.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include "AudioTools.h"
#include <demos/lv_demos.h>

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
    "Concentrate and ask again"
};

// UI elements
lv_obj_t *wifi_label;
lv_obj_t *magic_label;

void startRecording();
void stopRecording();
void updateWiFiStatus(void *parameter);
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

/*To use the built-in examples and demos of LVGL uncomment the includes below respectively.
 *You also need to copy `lvgl/examples` to `lvgl/src/examples`. Similarly for the demos `lvgl/demos` to `lvgl/src/demos`.
 Note that the `lv_examples` library is for LVGL v7 and you shouldn't install it for this version (since LVGL v8)
 as the examples and demos are now part of the main LVGL library. */

#define EXAMPLE_LVGL_TICK_PERIOD_MS 2

/*Change to your screen resolution*/
static const uint16_t screenWidth = 240;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */

LV_IMG_DECLARE(test1_240_240_4);
LV_IMG_DECLARE(test2);
LV_IMG_DECLARE(test3);
#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char *buf)
{
  Serial.printf(buf);
  Serial.flush();
}
#endif

/* Display flushing */
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

void example_increase_lvgl_tick(void *arg)
{
  /* Tell LVGL how many milliseconds has elapsed */
  // lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static uint8_t count = 0;
void example_increase_reboot(void *arg)
{
  count++;
  if (count % 2)
  {
    // esp_restart();
    lv_obj_t *icon = lv_img_create(lv_scr_act());
    /*From variable*/
    lv_img_set_src(icon, &test1_240_240_4);
  }
  else
  {
    lv_obj_t *icon = lv_img_create(lv_scr_act());
    /*From variable*/

    lv_img_set_src(icon, &test3);
  }
}

void setup()
{
  Serial.begin(115200); /* prepare for possible serial debug */
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  String LVGL_Arduino = "Hello Arduino! ";
  LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

  Serial.println(LVGL_Arduino);
  Serial.println("I am LVGL_Arduinooooooooooo");

  lv_init();
#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

  tft.begin();        /* TFT init */
  tft.setRotation(0); /* Landscape orientation, flipped */

  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /* Create simple label */
  // lv_obj_t *label = lv_label_create( lv_scr_act() );
  // lv_label_set_text( label, "Hello Ardino and LVGL!");
  // lv_obj_align( label, LV_ALIGN_CENTER, 0, 0 );

  lv_obj_t *icon = lv_img_create(lv_scr_act());
  /*From variable*/
  lv_img_set_src(icon, &test1_240_240_4);

  /* Try an example. See all the examples
   * online: https://docs.lvgl.io/master/examples.html
   * source codes: https://github.com/lvgl/lvgl/tree/e7f88efa5853128bf871dde335c0ca8da9eb7731/examples */
  // lv_example_btn_1();

  const esp_timer_create_args_t lvgl_tick_timer_args = {
      .callback = &example_increase_lvgl_tick,
      .name = "lvgl_tick"};

  const esp_timer_create_args_t reboot_timer_args = {
      .callback = &example_increase_reboot,
      .name = "reboot"};

  esp_timer_handle_t lvgl_tick_timer = NULL;
  esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
  esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000);

  esp_timer_handle_t reboot_timer = NULL;
  esp_timer_create(&reboot_timer_args, &reboot_timer);
  esp_timer_start_periodic(reboot_timer, 5000 * 1000);

  /*Or try out a demo. Don't forget to enable the demos in lv_conf.h. E.g. LV_USE_DEMOS_WIDGETS*/
  // lv_demo_widgets();
  // lv_demo_benchmark();
  // lv_demo_keypad_encoder();
  // lv_demo_music();
  // lv_demo_printer();
  // lv_demo_stress();
  // WiFi setup
  WiFi.mode(WIFI_STA);
  IPAddress portalIP(8, 8, 4, 4);
  IPAddress gateway(8, 8, 4, 4);
  IPAddress subnet(255, 255, 255, 0);
  wifiManager.setAPStaticIPConfig(portalIP, gateway, subnet);
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.autoConnect("Magic8Ball_AP");

  Serial.println("Setup done");

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
  unsigned long currentTime = millis();
  lv_timer_handler(); /* let the GUI do its work */
  delay(5);
}
