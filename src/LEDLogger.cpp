#include "LEDLogger.h"

LEDLogger::LEDLogger(uint8_t pin, uint8_t brightness)
    : pixel(1, pin, NEO_GRB + NEO_KHZ400),
      currentState(SystemState::STARTUP),
      currentPattern(LEDPattern::PULSE),
      lastUpdate(0),
      pulseValue(0),
      increasing(true),
      currentColor(COLOR_STARTUP),
      brightness(brightness)
{
}

void LEDLogger::begin()
{
    pixel.begin();
    pixel.setBrightness(brightness);
    pixel.show();
}

void LEDLogger::setState(SystemState state, LEDPattern pattern)
{
    currentState = state;
    currentPattern = pattern;
    currentColor = getColorForState(state);
    pulseValue = 0;
    increasing = true;
    lastUpdate = millis();
}

void LEDLogger::setCustomState(uint32_t color, LEDPattern pattern)
{
    currentState = SystemState::CUSTOM;
    currentPattern = pattern;
    currentColor = color;
    pulseValue = 0;
    increasing = true;
    lastUpdate = millis();
}

void LEDLogger::update()
{
    updatePattern();
    pixel.show();
}

void LEDLogger::updatePattern()
{
    unsigned long now = millis();
    switch (currentPattern)
    {
        case LEDPattern::SOLID:
            setPixel(currentColor);
            break;

        case LEDPattern::PULSE:
            if (now - lastUpdate >= PULSE_INTERVAL)
            {
                if (increasing)
                {
                    pulseValue += 0.05;
                    if (pulseValue >= 1.0)
                    {
                        pulseValue = 1.0;
                        increasing = false;
                    }
                }
                else
                {
                    pulseValue -= 0.05;
                    if (pulseValue <= 0.0)
                    {
                        pulseValue = 0.0;
                        increasing = true;
                    }
                }
                setPixel(dimColor(currentColor, pulseValue));
                lastUpdate = now;
            }
            break;

        case LEDPattern::BLINK:
            if (now - lastUpdate >= BLINK_INTERVAL)
            {
                static bool isOn = false;
                isOn = !isOn;
                setPixel(isOn ? currentColor : 0);
                lastUpdate = now;
            }
            break;

        case LEDPattern::FAST_BLINK:
            if (now - lastUpdate >= FAST_BLINK_INTERVAL)
            {
                static bool isOn = false;
                isOn = !isOn;
                setPixel(isOn ? currentColor : 0);
                lastUpdate = now;
            }
            break;

        case LEDPattern::DOUBLE_BLINK:
            if (now - lastUpdate >= BLINK_INTERVAL)
            {
                static uint8_t blinkState = 0;
                switch (blinkState)
                {
                    case 0: setPixel(currentColor); break;
                    case 1: setPixel(0); break;
                    case 2: setPixel(currentColor); break;
                    case 3: setPixel(0); break;
                    default: blinkState = 0; break;
                }
                blinkState = (blinkState + 1) % 5;
                lastUpdate = now;
            }
            break;

        case LEDPattern::RAINBOW:
            if (now - lastUpdate >= PULSE_INTERVAL)
            {
                static uint16_t hue = 0;
                hue += 256;
                if (hue >= 65536) hue = 0;
                setPixel(pixel.gamma32(pixel.ColorHSV(hue)));
                lastUpdate = now;
            }
            break;
    }
}

void LEDLogger::setBrightness(uint8_t newBrightness)
{
    brightness = newBrightness;
    pixel.setBrightness(brightness);
    pixel.show();
}

void LEDLogger::clear()
{
    pixel.clear();
    pixel.show();
}

uint32_t LEDLogger::getColorForState(SystemState state)
{
    switch (state)
    {
        case SystemState::STARTUP:     return COLOR_STARTUP;
        case SystemState::NORMAL:      return COLOR_NORMAL;
        case SystemState::WARNING:     return COLOR_WARNING;
        case SystemState::ERROR:       return COLOR_ERROR;
        case SystemState::MAINTENANCE: return COLOR_MAINTENANCE;
        case SystemState::UPDATING:    return COLOR_UPDATING;
        case SystemState::CONNECTING:  return COLOR_CONNECTING;
        case SystemState::BUSY:        return COLOR_BUSY;
        case SystemState::STANDBY:     return COLOR_STANDBY;
        default:                       return COLOR_NORMAL;
    }
}

void LEDLogger::setPixel(uint32_t color)
{
    pixel.setPixelColor(0, color);
}

uint32_t LEDLogger::dimColor(uint32_t color, float factor)
{
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    
    r = r * factor;
    g = g * factor;
    b = b * factor;
    
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}