#ifndef VIBRATION_MANAGER_H
#define VIBRATION_MANAGER_H

#include <Arduino.h>

class VibrationManager {
public:
    // Constructor
    VibrationManager(uint8_t motor1Pin = 5, uint8_t motor2Pin = 13) 
        : _motor1Pin(motor1Pin), _motor2Pin(motor2Pin), _isVibrating(false), _startTime(0), _duration(0) {}

    // Initialize the vibration motors
    void begin() {
        pinMode(_motor1Pin, OUTPUT);
        pinMode(_motor2Pin, OUTPUT);
        stop(); // Ensure motors are off initially
    }

    // Start vibration for a specific duration
    void startVibration(unsigned long duration) {
        _isVibrating = true;
        _startTime = millis();
        _duration = duration;
        digitalWrite(_motor1Pin, HIGH);
        digitalWrite(_motor2Pin, HIGH);
    }

    // Stop vibration immediately
    void stop() {
        _isVibrating = false;
        digitalWrite(_motor1Pin, LOW);
        digitalWrite(_motor2Pin, LOW);
    }

    // Update function to be called in the main loop
    void update() {
        if (_isVibrating && (millis() - _startTime >= _duration)) {
            stop();
        }
    }

    // Predefined vibration patterns
    void shortBuzz() {
        startVibration(100); // 100ms vibration
    }

    void mediumBuzz() {
        startVibration(250); // 250ms vibration
    }

    void longBuzz() {
        startVibration(500); // 500ms vibration
    }

    void doubleBuzz() {
        startVibration(100);
        delay(100);
        startVibration(100);
    }

    bool isVibrating() const {
        return _isVibrating;
    }

private:
    uint8_t _motor1Pin;
    uint8_t _motor2Pin;
    bool _isVibrating;
    unsigned long _startTime;
    unsigned long _duration;
};

#endif // VIBRATION_MANAGER_H