#include "Animations.h"
#include <stdarg.h>

TFT_eSPI *AnimationManager::tft = nullptr;

AnimationManager::AnimationManager(uint16_t screenW, uint16_t screenH)
    : screenWidth(screenW), screenHeight(screenH), triangleSize(screenW / 2), _triangle(nullptr), _label(nullptr), _current_x(0), _current_y(0), _velocity_x(0), _velocity_y(0), _target_x(0), _target_y(0), _idle_time(0), _isShaking(false), _isTransitioningToCenter(false)
{
  _buf = new lv_color_t[screenWidth * screenHeight / 10];
  tft = new TFT_eSPI(screenWidth, screenHeight);
}

AnimationManager::~AnimationManager()
{
  delete[] _buf;
  delete tft;
}

bool AnimationManager::begin()
{
  // Initialize LVGL
  lv_init();
  tft->begin();
  tft->setRotation(0);

  // Initialize display buffer
  lv_disp_draw_buf_init(&_draw_buf, _buf, NULL, screenWidth * screenHeight / 10);

  // Initialize display driver
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = displayFlushCallback;
  disp_drv.draw_buf = &_draw_buf;
  lv_disp_drv_register(&disp_drv);

  // Set black background
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);

  return true;
}

void AnimationManager::initializeTriangle()
{
  // Create triangle
  _triangle = lv_obj_create(lv_scr_act());
  lv_obj_set_size(_triangle, triangleSize, triangleSize);
  lv_obj_set_style_radius(_triangle, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(_triangle, lv_color_make(0, 0, 255), LV_PART_MAIN);
  lv_obj_set_style_transform_angle(_triangle, 450, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(_triangle, LV_OPA_COVER, LV_PART_MAIN);

  // Create label
  _label = lv_label_create(_triangle);
  lv_obj_set_style_text_color(_label, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_align(_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  // Add font size reduction
  lv_obj_set_style_text_font(_label, &lv_font_montserrat_14, LV_PART_MAIN); // Use smaller font
  // Or alternatively, scale the existing font:
  // lv_obj_set_style_text_line_space(_label, -2, LV_PART_MAIN);  // Reduce line spacing

  lv_label_set_text(_label, "DISCONNECTED");
  lv_obj_align(_label, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_long_mode(_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(_label, (triangleSize - 30)); // Reduced width for better edge handling

  // Initialize animations
  lv_anim_init(&_anim_x);
  lv_anim_init(&_anim_y);

  // Set initial position
  _current_x = (screenWidth - triangleSize) / 2;
  _current_y = (screenHeight - triangleSize) / 2;
  updatePosition(_current_x, _current_y);
}

void AnimationManager::displayFlushCallback(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft->startWrite();
  tft->setAddrWindow(area->x1, area->y1, w, h);
  tft->pushColors((uint16_t *)&color_p->full, w * h, true);
  tft->endWrite();

  lv_disp_flush_ready(disp_drv);
}

void AnimationManager::animXCallback(void *var, int32_t v)
{
  lv_obj_t *obj = (lv_obj_t *)var;
  lv_obj_set_x(obj, v);
  lv_obj_t *label = lv_obj_get_child(obj, 0);
  if (label)
  {
    lv_obj_align_to(label, obj, LV_ALIGN_CENTER, 0, 0);
  }
}

void AnimationManager::animYCallback(void *var, int32_t v)
{
  lv_obj_t *obj = (lv_obj_t *)var;
  lv_obj_set_y(obj, v);
  lv_obj_t *label = lv_obj_get_child(obj, 0);
  if (label)
  {
    lv_obj_align_to(label, obj, LV_ALIGN_CENTER, 0, 0);
  }
}

void AnimationManager::animReadyCallback(lv_anim_t *a)
{
  AnimationManager *self = (AnimationManager *)lv_anim_get_user_data(a);
  if (self)
  {
    self->_isTransitioningToCenter = false;
  }
}

void AnimationManager::moveToCenter()
{
  if (!_isTransitioningToCenter)
  {
    _isTransitioningToCenter = true;

    _target_x = 120;
    _target_y = 40;
    // Reset velocities
    _velocity_x = 0;
    _velocity_y = 0;

    // Set up X animation
    lv_anim_set_user_data(&_anim_x, this);
    lv_anim_set_var(&_anim_x, _triangle);
    lv_anim_set_values(&_anim_x, _current_x, _target_x);
    lv_anim_set_time(&_anim_x, 800);
    lv_anim_set_exec_cb(&_anim_x, animXCallback);
    lv_anim_set_path_cb(&_anim_x, lv_anim_path_ease_out);
    lv_anim_start(&_anim_x);

    // Set up Y animation
    lv_anim_set_user_data(&_anim_y, this);
    lv_anim_set_var(&_anim_y, _triangle);
    lv_anim_set_values(&_anim_y, _current_y, _target_y);
    lv_anim_set_time(&_anim_y, 800);
    lv_anim_set_exec_cb(&_anim_y, animYCallback);
    lv_anim_set_path_cb(&_anim_y, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&_anim_y, animReadyCallback);
    lv_anim_start(&_anim_y);
  }
}

void AnimationManager::updateIdleMotion()
{
  _idle_time += IDLE_SPEED;

  float offset_x = sin(_idle_time) * cos(_idle_time * 0.7f) * IDLE_AMPLITUDE;
  float offset_y = cos(_idle_time * 1.3f) * sin(_idle_time * 0.5f) * IDLE_AMPLITUDE;

  _target_x += offset_x * 0.01f;
  _target_y += offset_y * 0.01f;
}

void AnimationManager::updateTrianglePosition(float gyro_x, float gyro_y)
{
  if (!_triangle || _isShaking || _isTransitioningToCenter)
    return;

  bool is_still = abs(gyro_x) < 0.5f && abs(gyro_y) < 0.5f;

  if (is_still)
  {
    updateIdleMotion();
  }
  else
  {
    // Invert gyro inputs to match intuitive direction
    _target_x += gyro_y * GYRO_SENSITIVITY; // Changed from += to -=
    _target_y -= gyro_x * GYRO_SENSITIVITY; // Changed from += to -=
  }

  constrainPosition();

  float dx = _target_x - _current_x;
  float dy = _target_y - _current_y;

  _velocity_x = _velocity_x * MOTION_SMOOTHING + dx * (1.0f - MOTION_SMOOTHING);
  _velocity_y = _velocity_y * MOTION_SMOOTHING + dy * (1.0f - MOTION_SMOOTHING);

  _current_x += _velocity_x * 0.1f;
  _current_y += _velocity_y * 0.1f;

  updatePosition(_current_x, _current_y);
}

void AnimationManager::constrainPosition()
{
  float center_x = screenWidth / 2.0f;
  float center_y = screenHeight / 2.0f;
  float radius = min(screenWidth, screenHeight) / 2.0f;
  float max_distance = radius * SCREEN_MARGIN;

  // Calculate distance from center
  float dx = _target_x + (triangleSize / 2) - center_x;
  float dy = _target_y + (triangleSize / 2) - center_y;
  float distance = sqrt(dx * dx + dy * dy);

  // If outside allowable radius, pull back inside
  if (distance > max_distance)
  {
    float angle = atan2(dy, dx);
    float new_x = center_x + cos(angle) * max_distance - (triangleSize / 2);
    float new_y = center_y + sin(angle) * max_distance - (triangleSize / 2);

    _target_x = constrain(new_x, -triangleSize * EDGE_BUFFER,
                          screenWidth - triangleSize * (1.0f - EDGE_BUFFER));
    _target_y = constrain(new_y, -triangleSize * EDGE_BUFFER,
                          screenHeight - triangleSize * (1.0f - EDGE_BUFFER));
  }
  else
  {
    // Still apply rectangular constraints for extra safety
    _target_x = constrain(_target_x, -triangleSize * EDGE_BUFFER,
                          screenWidth - triangleSize * (1.0f - EDGE_BUFFER));
    _target_y = constrain(_target_y, -triangleSize * EDGE_BUFFER,
                          screenHeight - triangleSize * (1.0f - EDGE_BUFFER));
  }
}

void AnimationManager::updatePosition(int16_t x, int16_t y)
{
  lv_obj_set_pos(_triangle, x, y);
  lv_obj_align_to(_label, _triangle, LV_ALIGN_CENTER, 0, 0);
}

void AnimationManager::setShaking(bool isShaking)
{
  _isShaking = isShaking;
  if (!isShaking)
  {
    _velocity_x = 0;
    _velocity_y = 0;
  }
}

void AnimationManager::setTriangleColor(uint8_t r, uint8_t g, uint8_t b)
{
  lv_obj_set_style_bg_color(_triangle, lv_color_make(r, g, b), LV_PART_MAIN);
}

void AnimationManager::setLabelText(const char *text)
{
  lv_label_set_text(_label, text);
}

void AnimationManager::setLabelTextFormatted(const char *format, ...)
{
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  setLabelText(buffer);
}