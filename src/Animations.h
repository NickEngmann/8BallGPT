#ifndef ANIMATIONS_H
#define ANIMATIONS_H

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>

class AnimationManager
{
public:
  AnimationManager(uint16_t screenW, uint16_t screenH);
  ~AnimationManager();

  // Initialization
  bool begin();
  void initializeTriangle();

  // Animation control
  void moveToCenter();
  void updateIdleMotion();
  void updateTrianglePosition(float gyro_x, float gyro_y);

  // State management
  void setShaking(bool isShaking);
  bool isShaking() const { return _isShaking; }
  bool isTransitioning() const { return _isTransitioningToCenter; }

  // Display methods
  void setTriangleColor(uint8_t r, uint8_t g, uint8_t b);
  void setLabelText(const char *text);
  void setLabelTextFormatted(const char *format, ...);

  // LVGL display handler
  static void displayFlushCallback(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);

  // Public access to display objects (if needed by main app)
  lv_obj_t *getTriangle() { return _triangle; }
  lv_obj_t *getLabel() { return _label; }

private:
  // Display configuration
  const uint16_t screenWidth;
  const uint16_t screenHeight;
  const uint16_t triangleSize;

  // LVGL objects
  static TFT_eSPI *tft;
  lv_obj_t *_triangle;
  lv_obj_t *_label;
  lv_disp_draw_buf_t _draw_buf;
  lv_color_t *_buf;

  // Animation objects
  lv_anim_t _anim_x;
  lv_anim_t _anim_y;

  // Position tracking
  float _current_x;
  float _current_y;
  float _velocity_x;
  float _velocity_y;
  float _target_x;
  float _target_y;
  float _idle_time;

  // State flags
  bool _isShaking;
  bool _isTransitioningToCenter;

  // Animation constants
  static constexpr float MOTION_SMOOTHING = 0.95f;
  static constexpr float GYRO_SENSITIVITY = 0.08f;
  static constexpr float IDLE_AMPLITUDE = 15.0f;
  static constexpr float IDLE_SPEED = 0.001f;
  static constexpr float SCREEN_MARGIN = 0.85f;
  static constexpr float EDGE_BUFFER = 0.15f;    // Maximum amount triangle can go over edge
  static constexpr uint16_t FONT_REDUCTION = 90; // Font size reduction to 90% of original

  // Animation callbacks
  static void animXCallback(void *var, int32_t v);
  static void animYCallback(void *var, int32_t v);
  static void animReadyCallback(lv_anim_t *a);

  // Helper methods
  void updatePosition(int16_t x, int16_t y);
  void constrainPosition();
};

#endif // ANIMATIONS_H