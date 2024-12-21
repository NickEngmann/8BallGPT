#ifndef RECORDER_H
#define RECORDER_H

#include <Arduino.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <esp_system.h>
#include <algorithm>

// Debug macro
#define DEBUG_PRINT(x) Serial.println(x)
#define DEBUG_PRINTF(format, ...) Serial.printf((format), ##__VA_ARGS__)

// Audio configurations
#define SAMPLE_RATE 8000      // 8KHz - more realistic for analog mic
#define CHANNELS 1            // Mono
#define BITS_PER_SAMPLE 12    // ADC resolution
#define MAX_RECORD_SECONDS 10 // Maximum recording duration
#define SILENCE_TIMEOUT 3000  // Silence detection timeout in ms
#define AUDIO_THRESHOLD 300   // Voice detection sensitivity (adjusted for ADC values)
#define WAV_HEADER_SIZE 44    // Standard WAV header size

// ADC Configuration
#define ADC_MIC_CHANNEL ADC1_CHANNEL_1 // GPIO2
#define ADC_MIC_GPIO_NUM 2    // GPIO pin number
#define ADC_MIC_UNIT ADC_UNIT_1        // ADC 1
#define SAMPLE_BUFFER_SIZE 256         // Size of temporary sample buffer
#define ADC_VREF 3300                  // 3.3V reference voltage
#define DC_OFFSET 1250                 // 1.25V DC bias from MAX9814
#define MAX9814_VPP 2000               // 2V peak-to-peak max


// Calculate total buffer size for the entire recording
#define BUFFER_SIZE (MAX_RECORD_SECONDS * SAMPLE_RATE * CHANNELS * 2) // *2 for 16-bit WAV samples

class VoiceActivatedRecorder
{
public:
  VoiceActivatedRecorder();
  ~VoiceActivatedRecorder();

  bool begin();
  bool startRecording();
  void stopRecording();
  void update();

  uint8_t *getBuffer() { return audioBuffer; }
  size_t getBufferSize() { return bufferIndex; }
  bool isRecording() { return is_recording; }
  float getRecordingProgress();

  // Debug functions
  void printADCInfo();
  void printBufferStatus();
  void printRecordingStatus();
  void debugMicValues(uint16_t *samples, size_t size);

private:
  uint8_t *audioBuffer;          // Main audio buffer
  size_t bufferIndex;            // Current position in buffer
  bool is_recording;             // Recording state
  unsigned long recordStartTime; // When recording started
  unsigned long lastSoundTime;   // Last time voice was detected
  unsigned long lastSampleTime;  // Last time we took a sample
  bool hasDetectedVoice;         // Whether voice has been detected

  // ADC calibration data
  esp_adc_cal_characteristics_t *adc_chars;

  // Memory monitoring
  size_t initialFreeHeap;
  size_t initialFreePSRAM;

  void writeWAVHeader();
  void updateWAVHeader();
  bool detectVoiceActivity(uint16_t *buffer, size_t size);
  void monitorMemory();
  void setupADC();
  int16_t readADCSample();

  // Debug helper functions
  void checkADCSetup();
  void validateBufferSpace();
};

#endif // RECORDER_H