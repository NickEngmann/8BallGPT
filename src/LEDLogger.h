#ifndef LED_LOGGER_H
#define LED_LOGGER_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

class LEDLogger
{
public:
  // Standard system states that could apply to any device
  enum class SystemState
  {
    STARTUP,     // Blue pulse - System is starting up
    NORMAL,      // Steady green - Everything is working normally
    WARNING,     // Yellow pulse - Non-critical issues
    ERROR,       // Red pulse - Critical issues that need attention
    MAINTENANCE, // Purple pulse - System needs maintenance
    UPDATING,    // White pulse - System is updating
    CONNECTING,  // Cyan pulse - Establishing connection
    BUSY,        // Orange pulse - System is processing something
    STANDBY,     // Dim white - System is idle
    CUSTOM       // For device-specific states
  };

  // Patterns that can be applied to any color
  enum class LEDPattern
  {
    SOLID,        // Steady color
    PULSE,        // Smooth breathing effect
    BLINK,        // On/off blinking
    FAST_BLINK,   // Rapid blinking for urgent states
    DOUBLE_BLINK, // Two blinks then pause
    RAINBOW       // Cycle through colors
  };

  LEDLogger(uint8_t pin, uint8_t brightness = 30);
  void begin();
  void update();
  void setState(SystemState state, LEDPattern pattern = LEDPattern::SOLID);
  void setCustomState(uint32_t color, LEDPattern pattern = LEDPattern::SOLID);
  void setBrightness(uint8_t brightness);
  void clear();

  SystemState getCurrentState() const { return currentState; }
  LEDPattern getCurrentPattern() const { return currentPattern; }

private:
  Adafruit_NeoPixel pixel;
  SystemState currentState;
  LEDPattern currentPattern;
  unsigned long lastUpdate;
  float pulseValue;
  bool increasing;
  uint32_t currentColor;
  uint8_t brightness;

  // Color definitions
  static constexpr uint32_t COLOR_STARTUP = 0x0000FF;     // Blue
  static constexpr uint32_t COLOR_NORMAL = 0x00FF00;      // Green
  static constexpr uint32_t COLOR_WARNING = 0xFFFF00;     // Yellow
  static constexpr uint32_t COLOR_ERROR = 0xFF0000;       // Red
  static constexpr uint32_t COLOR_MAINTENANCE = 0xFF00FF; // Purple
  static constexpr uint32_t COLOR_UPDATING = 0xFFFFFF;    // White
  static constexpr uint32_t COLOR_CONNECTING = 0x00FFFF;  // Cyan
  static constexpr uint32_t COLOR_BUSY = 0xFFA500;        // Orange
  static constexpr uint32_t COLOR_STANDBY = 0x101010;     // Dim white

  // Timing constants
  static constexpr unsigned long PULSE_INTERVAL = 30;
  static constexpr unsigned long BLINK_INTERVAL = 500;
  static constexpr unsigned long FAST_BLINK_INTERVAL = 100;

  void updatePattern();
  uint32_t getColorForState(SystemState state);
  void setPixel(uint32_t color);
  uint32_t dimColor(uint32_t color, float factor);
};

#endif // LED_LOGGER_H