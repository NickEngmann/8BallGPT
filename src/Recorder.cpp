#include "Recorder.h"
#include "esp_heap_caps.h"

VoiceActivatedRecorder::VoiceActivatedRecorder()
    : audioBuffer(nullptr),
    bufferIndex(0),
    is_recording(false),
    recordStartTime(0),
    lastSoundTime(0),
    silenceCounter(0), 
    hasDetectedVoice(false),
    initialFreeHeap(0),
    initialFreePSRAM(0)
{
}

VoiceActivatedRecorder::~VoiceActivatedRecorder() {
    if (audioBuffer) {
        free(audioBuffer);
    }
}

bool VoiceActivatedRecorder::begin() {
    // Record initial memory state
    initialFreeHeap = esp_get_free_heap_size();
    initialFreePSRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    // Try to allocate buffer in PSRAM
    if (psramFound()) {
        audioBuffer = (uint8_t *)ps_malloc(BUFFER_SIZE);
        if (audioBuffer) {
            Serial.printf("Using PSRAM for audio buffer (%d bytes)\n", BUFFER_SIZE);
        }
    }

    // Fallback to regular RAM if PSRAM allocation failed
    if (!audioBuffer) {
        Serial.println("PSRAM allocation failed!");
        return false;
    }

    // Configure I2S for PDM microphone
    auto config = i2s.defaultConfig(RX_MODE);
    config.sample_rate = SAMPLE_RATE;
    config.bits_per_sample = BITS_PER_SAMPLE;
    config.channels = CHANNELS;
    config.signal_type = PDM;
    config.pin_bck = i2s_bck;
    config.pin_ws = i2s_ws;
    config.pin_data = i2s_data;
    config.use_apll = false;

    if (!i2s.begin(config)) {
        Serial.println("Failed to initialize I2S!");
        return false;
    }

    monitorMemory();
    return true;
}

void VoiceActivatedRecorder::monitorMemory() {
    size_t currentHeap = esp_get_free_heap_size();
    size_t currentPSRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    Serial.printf("Memory Usage - Heap: %d bytes free (%d used), PSRAM: %d bytes free (%d used)\n",
                  currentHeap,
                  initialFreeHeap - currentHeap,
                  currentPSRAM,
                  initialFreePSRAM - currentPSRAM);
}

float VoiceActivatedRecorder::getRecordingProgress() {
    if (!is_recording) return 0.0f;
    return min(1.0f, (float)bufferIndex / (float)BUFFER_SIZE);
}

void VoiceActivatedRecorder::writeWAVHeader() {
    if (!audioBuffer) return;

    uint8_t header[WAV_HEADER_SIZE] = {
        'R', 'I', 'F', 'F',
        0, 0, 0, 0,
        'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ',
        16, 0, 0, 0,
        1, 0,
        CHANNELS, 0,
        (uint8_t)(SAMPLE_RATE & 0xFF), (uint8_t)((SAMPLE_RATE >> 8) & 0xFF),
        (uint8_t)((SAMPLE_RATE >> 16) & 0xFF), (uint8_t)((SAMPLE_RATE >> 24) & 0xFF),
        0, 0, 0, 0,
        CHANNELS * (BITS_PER_SAMPLE / 8), 0,
        BITS_PER_SAMPLE, 0,
        'd', 'a', 't', 'a',
        0, 0, 0, 0
    };

    uint32_t byteRate = SAMPLE_RATE * CHANNELS * (BITS_PER_SAMPLE / 8);
    header[28] = byteRate & 0xFF;
    header[29] = (byteRate >> 8) & 0xFF;
    header[30] = (byteRate >> 16) & 0xFF;
    header[31] = (byteRate >> 24) & 0xFF;

    memcpy(audioBuffer, header, WAV_HEADER_SIZE);
    bufferIndex = WAV_HEADER_SIZE;
}

void VoiceActivatedRecorder::updateWAVHeader() {
    if (!audioBuffer || bufferIndex <= WAV_HEADER_SIZE) return;

    uint32_t fileSize = bufferIndex - 8;
    audioBuffer[4] = fileSize & 0xFF;
    audioBuffer[5] = (fileSize >> 8) & 0xFF;
    audioBuffer[6] = (fileSize >> 16) & 0xFF;
    audioBuffer[7] = (fileSize >> 24) & 0xFF;

    uint32_t dataSize = bufferIndex - WAV_HEADER_SIZE;
    audioBuffer[40] = dataSize & 0xFF;
    audioBuffer[41] = (dataSize >> 8) & 0xFF;
    audioBuffer[42] = (dataSize >> 16) & 0xFF;
    audioBuffer[43] = (dataSize >> 24) & 0xFF;
}

bool VoiceActivatedRecorder::detectVoiceActivity(uint8_t *buffer, size_t size) {
    if (size < 2) return false;

    int16_t *samples = (int16_t *)buffer;
    size_t numSamples = size / 2;
    int32_t maxAmplitude = 0;

    for (size_t i = 0; i < numSamples; i++) {
        int32_t amplitude = abs(samples[i]);
        maxAmplitude = max(maxAmplitude, amplitude);
    }

    return (maxAmplitude > AUDIO_THRESHOLD);
}

bool VoiceActivatedRecorder::startRecording() {
    if (is_recording || !audioBuffer) return false;

    writeWAVHeader();
    is_recording = true;
    recordStartTime = millis();
    lastSoundTime = recordStartTime;
    hasDetectedVoice = false;
    silenceCounter = 0;

    Serial.println("Started recording - waiting for voice");
    monitorMemory();
    return true;
}

void VoiceActivatedRecorder::stopRecording() {
    if (!is_recording) return;

    is_recording = false;
    updateWAVHeader();
    Serial.printf("Recording stopped. Buffer used: %d bytes (%.1f seconds)\n",
                  bufferIndex,
                  (float)(bufferIndex - WAV_HEADER_SIZE) / (SAMPLE_RATE * CHANNELS * (BITS_PER_SAMPLE / 8)));
    monitorMemory();
}

void VoiceActivatedRecorder::update() {
    if (!is_recording || !audioBuffer || bufferIndex >= BUFFER_SIZE) {
        if (bufferIndex >= BUFFER_SIZE) {
            Serial.println("Buffer full - stopping recording");
            stopRecording();
        }
        return;
    }

    // Check maximum recording time
    if (millis() - recordStartTime >= MAX_RECORD_SECONDS * 1000) {
        Serial.println("Max recording time reached");
        stopRecording();
        return;
    }

    // Read audio data in chunks
    uint8_t chunk[1024];
    size_t bytesRead = i2s.readBytes(chunk, sizeof(chunk));

    if (bytesRead > 0) {
        bool hasVoice = detectVoiceActivity(chunk, bytesRead);

        if (hasVoice) {
            lastSoundTime = millis();
            if (!hasDetectedVoice) {
                hasDetectedVoice = true;
                Serial.println("Voice detected - recording started");
            }
        }
        else if (hasDetectedVoice) {
            if (millis() - lastSoundTime >= SILENCE_TIMEOUT) {
                Serial.println("Silence detected - stopping recording");
                stopRecording();
                return;
            }
        }

        // Only store audio if we've detected voice and have buffer space
        if (hasDetectedVoice && (bufferIndex + bytesRead) <= BUFFER_SIZE) {
            memcpy(audioBuffer + bufferIndex, chunk, bytesRead);
            bufferIndex += bytesRead;
        }
    }
}