#ifndef RECORDER_H
#define RECORDER_H

#include "AudioTools.h"

// Audio configurations
#define SAMPLE_RATE 16000    // 16KHz
#define CHANNELS 1           // Mono
#define BITS_PER_SAMPLE 16   // 16-bit audio
#define MAX_RECORD_SECONDS 10 // Maximum recording duration
#define SILENCE_TIMEOUT 3000 // Silence detection timeout in ms
#define AUDIO_THRESHOLD 1000 // Voice detection sensitivity
#define WAV_HEADER_SIZE 44   // Standard WAV header size

// Calculate buffer size 
#define BUFFER_SIZE (MAX_RECORD_SECONDS * SAMPLE_RATE * CHANNELS * (BITS_PER_SAMPLE / 8))

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
  float getRecordingProgress(); // New method to get recording progress

private:
  // I2S pins
  static const int i2s_bck = 44;
  static const int i2s_ws = -1;
  static const int i2s_data = 3;

  // Audio handling
  I2SStream i2s;
  uint8_t *audioBuffer;
  size_t bufferIndex;

  // State tracking
  bool is_recording;
  unsigned long recordStartTime;
  unsigned long lastSoundTime;
  uint32_t silenceCounter;
  bool hasDetectedVoice;

  // Memory monitoring
  size_t initialFreeHeap;
  size_t initialFreePSRAM;

  // Private methods
  void writeWAVHeader();
  void updateWAVHeader();
  bool detectVoiceActivity(uint8_t *buffer, size_t size);
  void monitorMemory(); // New method for memory monitoring
};

#endif // RECORDER_H