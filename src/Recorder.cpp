#include "Recorder.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"

VoiceActivatedRecorder::VoiceActivatedRecorder()
    : audioBuffer(nullptr),
      bufferIndex(0),
      is_recording(false),
      recordStartTime(0),
      lastSoundTime(0),
      lastSampleTime(0),
      hasDetectedVoice(false),
      adc_chars(nullptr),
      initialFreeHeap(0),
      initialFreePSRAM(0)
{
}

VoiceActivatedRecorder::~VoiceActivatedRecorder()
{
    if (audioBuffer)
    {
        free(audioBuffer);
    }
    if (adc_chars)
    {
        free(adc_chars);
    }
}

void VoiceActivatedRecorder::checkADCSetup()
{
    DEBUG_PRINTF("ADC Pin Configuration: Channel %d, GPIO %d\n", ADC_MIC_CHANNEL, ADC_MIC_GPIO_NUM);

    gpio_num_t gpio_num = static_cast<gpio_num_t>(ADC_MIC_GPIO_NUM);
    if (adc1_pad_get_io_num(ADC_MIC_CHANNEL, &gpio_num) != ESP_OK)
    {
        DEBUG_PRINT("ERROR: Invalid ADC channel configuration!");
    }
}
void VoiceActivatedRecorder::setupADC()
{
    DEBUG_PRINT("Setting up ADC...");

    // Configure ADC
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_12));
    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC_MIC_CHANNEL, ADC_ATTEN_DB_11));

    // Check ADC setup
    checkADCSetup();

    // Allocate and initialize ADC characteristics structure
    adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
    if (adc_chars == nullptr)
    {
        DEBUG_PRINT("Failed to allocate ADC characteristics structure");
        return;
    }

    // Characterize ADC
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
        ADC_MIC_UNIT,
        ADC_ATTEN_DB_11,
        ADC_WIDTH_BIT_12,
        ADC_VREF,
        adc_chars);

    // Log ADC characterization results
    switch (val_type)
    {
    case ESP_ADC_CAL_VAL_EFUSE_VREF:
        DEBUG_PRINT("ADC characterized using eFuse Vref");
        break;
    case ESP_ADC_CAL_VAL_EFUSE_TP:
        DEBUG_PRINT("ADC characterized using Two Point Value");
        break;
    default:
        DEBUG_PRINT("ADC characterized using Default Vref");
        break;
    }

    // Print initial ADC reading
    uint32_t raw = adc1_get_raw(ADC_MIC_CHANNEL);
    uint32_t voltage = esp_adc_cal_raw_to_voltage(raw, adc_chars);
    DEBUG_PRINTF("Initial ADC Reading - Raw: %d, Voltage: %dmV\n", raw, voltage);
}

void VoiceActivatedRecorder::printADCInfo()
{
    if (!adc_chars)
    {
        DEBUG_PRINT("ADC not initialized!");
        return;
    }

    uint32_t raw = adc1_get_raw(ADC_MIC_CHANNEL);
    uint32_t voltage = esp_adc_cal_raw_to_voltage(raw, adc_chars);
    DEBUG_PRINTF("ADC Reading - Raw: %d, Voltage: %dmV\n", raw, voltage);
}

void VoiceActivatedRecorder::printBufferStatus()
{
    DEBUG_PRINTF("Buffer Status:\n");
    DEBUG_PRINTF("Total Size: %d bytes\n", BUFFER_SIZE);
    DEBUG_PRINTF("Used: %d bytes\n", bufferIndex);
    DEBUG_PRINTF("Free: %d bytes\n", BUFFER_SIZE - bufferIndex);
    DEBUG_PRINTF("Progress: %.1f%%\n", (float)bufferIndex / BUFFER_SIZE * 100);
}

void VoiceActivatedRecorder::printRecordingStatus()
{
    if (is_recording)
    {
        unsigned long duration = (millis() - recordStartTime) / 1000;
        DEBUG_PRINTF("Recording Status:\n");
        DEBUG_PRINTF("Duration: %lu seconds\n", duration);
        DEBUG_PRINTF("Voice Detected: %s\n", hasDetectedVoice ? "Yes" : "No");
        DEBUG_PRINTF("Last Sound: %lu ms ago\n", millis() - lastSoundTime);
    }
}

void VoiceActivatedRecorder::monitorMemory()
{
    size_t currentHeap = esp_get_free_heap_size();
    size_t currentPSRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    DEBUG_PRINTF("Memory Usage - Heap: %d bytes free (%d used), PSRAM: %d bytes free (%d used)\n",
                 currentHeap,
                 initialFreeHeap - currentHeap,
                 currentPSRAM,
                 initialFreePSRAM - currentPSRAM);
}

bool VoiceActivatedRecorder::begin()
{
    DEBUG_PRINT("Initializing Voice Activated Recorder...");

    // Record initial memory state
    initialFreeHeap = esp_get_free_heap_size();
    initialFreePSRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    // Try to allocate buffer in PSRAM if available
    if (psramFound())
    {
        audioBuffer = (uint8_t *)ps_malloc(BUFFER_SIZE);
        if (audioBuffer)
        {
            DEBUG_PRINTF("Using PSRAM for audio buffer (%d bytes)\n", BUFFER_SIZE);
        }
    }

    if (!audioBuffer)
    {
        DEBUG_PRINT("PSRAM allocation failed, recording won't be possible");
        return false;
    }

    // Setup ADC for microphone input
    setupADC();
    if (!adc_chars)
    {
        DEBUG_PRINT("ADC setup failed");
        return false;
    }
    DEBUG_PRINTF("MAX9814 Setup - DC Bias: %dmV, Max Vpp: %dmV\n",
                 DC_OFFSET, MAX9814_VPP);
    monitorMemory();
    return true;
}

float VoiceActivatedRecorder::getRecordingProgress()
{
    if (!is_recording)
        return 0.0f;
    return std::min(1.0f, (float)bufferIndex / (float)BUFFER_SIZE);
}

void VoiceActivatedRecorder::writeWAVHeader()
{
    if (!audioBuffer)
        return;

    uint8_t header[WAV_HEADER_SIZE] = {
        'R', 'I', 'F', 'F', // RIFF chunk ID
        0, 0, 0, 0,         // Chunk size (to be filled later)
        'W', 'A', 'V', 'E', // Format
        'f', 'm', 't', ' ', // Subchunk1 ID
        16, 0, 0, 0,        // Subchunk1 size (16 for PCM)
        1, 0,               // Audio format (1 = PCM)
        CHANNELS, 0,        // Number of channels
        // Sample rate
        (uint8_t)(SAMPLE_RATE & 0xFF),
        (uint8_t)((SAMPLE_RATE >> 8) & 0xFF),
        (uint8_t)((SAMPLE_RATE >> 16) & 0xFF),
        (uint8_t)((SAMPLE_RATE >> 24) & 0xFF),
        // Byte rate
        0, 0, 0, 0,         // To be filled
        CHANNELS * 2, 0,    // Block align
        16, 0,              // Bits per sample (16-bit)
        'd', 'a', 't', 'a', // Subchunk2 ID
        0, 0, 0, 0          // Subchunk2 size (to be filled later)
    };

    // Calculate byte rate
    uint32_t byteRate = SAMPLE_RATE * CHANNELS * 2; // 2 bytes per sample
    header[28] = byteRate & 0xFF;
    header[29] = (byteRate >> 8) & 0xFF;
    header[30] = (byteRate >> 16) & 0xFF;
    header[31] = (byteRate >> 24) & 0xFF;

    memcpy(audioBuffer, header, WAV_HEADER_SIZE);
    bufferIndex = WAV_HEADER_SIZE;
}

void VoiceActivatedRecorder::updateWAVHeader()
{
    if (!audioBuffer || bufferIndex <= WAV_HEADER_SIZE)
        return;

    // Update RIFF chunk size
    uint32_t fileSize = bufferIndex - 8;
    audioBuffer[4] = fileSize & 0xFF;
    audioBuffer[5] = (fileSize >> 8) & 0xFF;
    audioBuffer[6] = (fileSize >> 16) & 0xFF;
    audioBuffer[7] = (fileSize >> 24) & 0xFF;

    // Update data chunk size
    uint32_t dataSize = bufferIndex - WAV_HEADER_SIZE;
    audioBuffer[40] = dataSize & 0xFF;
    audioBuffer[41] = (dataSize >> 8) & 0xFF;
    audioBuffer[42] = (dataSize >> 16) & 0xFF;
    audioBuffer[43] = (dataSize >> 24) & 0xFF;
}
void VoiceActivatedRecorder::debugMicValues(uint16_t *samples, size_t size)
{
    if (!samples || size == 0)
        return;

    // Calculate min, max, and average
    uint16_t min_val = 65535;
    uint16_t max_val = 0;
    uint32_t sum = 0;
    uint16_t midPoint = 2048; // 12-bit ADC midpoint

    for (size_t i = 0; i < size; i++)
    {
        uint16_t val = samples[i];
        min_val = min(min_val, val);
        max_val = max(max_val, val);
        sum += abs((int16_t)val - midPoint);
    }

    uint16_t average = sum / size;
    float peak_to_peak = max_val - min_val;

    DEBUG_PRINTF("Mic Debug - Min: %d, Max: %d, Avg: %d, Peak-to-Peak: %.2f\n",
                 min_val, max_val, average, peak_to_peak);
}

bool VoiceActivatedRecorder::detectVoiceActivity(uint16_t *buffer, size_t size)
{
    if (!buffer || size == 0)
        return false;

    // Track zero crossings and amplitude
    int zero_crossings = 0;
    uint32_t sum_amplitude = 0;
    int16_t last_sample = 0;
    uint16_t max_amplitude = 0;

    for (size_t i = 0; i < size; i++)
    {
        int16_t current = (int16_t)buffer[i] - 2048;

        // Count zero crossings (frequency detection)
        if (last_sample < 0 && current >= 0)
            zero_crossings++;

        // Track amplitude
        uint16_t amplitude = abs(current);
        sum_amplitude += amplitude;
        max_amplitude = max(max_amplitude, amplitude);

        last_sample = current;
    }

    float avg_amplitude = sum_amplitude / (float)size;
    float crossing_rate = (zero_crossings * 1000.0f) / (size / (SAMPLE_RATE / 1000.0f));

    DEBUG_PRINTF("Voice Stats - Avg Amp: %.2f, Max Amp: %d, Crossings/s: %.2f\n",
                 avg_amplitude, max_amplitude, crossing_rate);

    // Based on your logs, adjust these thresholds
    bool is_voice = (avg_amplitude > 2000.0f) || (max_amplitude > 8000);

    return is_voice;
}

bool VoiceActivatedRecorder::startRecording()
{
    if (is_recording || !audioBuffer)
    {
        DEBUG_PRINT("Failed to start recording - already recording or no buffer");
        return false;
    }

    writeWAVHeader();
    is_recording = true;
    recordStartTime = millis();
    lastSoundTime = recordStartTime;
    lastSampleTime = recordStartTime;
    hasDetectedVoice = false;

    DEBUG_PRINT("Started recording - waiting for voice");
    monitorMemory();
    return true;
}

void VoiceActivatedRecorder::stopRecording()
{
    if (!is_recording)
        return;

    is_recording = false;
    updateWAVHeader();

    float duration = (float)(bufferIndex - WAV_HEADER_SIZE) / (SAMPLE_RATE * CHANNELS * 2);
    DEBUG_PRINTF("Recording stopped. Buffer used: %d bytes (%.1f seconds)\n",
                 bufferIndex, duration);
    monitorMemory();
}

int16_t VoiceActivatedRecorder::readADCSample()
{
    if (!adc_chars)
    {
        DEBUG_PRINT("ERROR: ADC not initialized in readADCSample!");
        return 0;
    }

    // Read raw value from ADC
    uint32_t raw = adc1_get_raw(ADC_MIC_CHANNEL);

    // Convert to voltage using calibration
    uint32_t voltage = esp_adc_cal_raw_to_voltage(raw, adc_chars);

    // Center around DC bias (1.25V) and scale appropriately
    // MAX9814 outputs around 1.25V DC bias, so subtract that first
    int16_t centered = voltage - DC_OFFSET;

    // Scale to 16-bit range (-32768 to 32767)
    int16_t sample = (centered * 32768) / (MAX9814_VPP / 2);

    static unsigned long lastSampleDebug = 0;
    if (millis() - lastSampleDebug > 1000)
    { // Debug print once per second
        DEBUG_PRINTF("Sample Details - Raw: %d, Voltage: %dmV, Centered: %d, Final: %d\n",
                     raw, voltage, centered, sample);
        lastSampleDebug = millis();
    }

    return sample;
}

void VoiceActivatedRecorder::update()
{
    static unsigned long lastDebugPrint = 0;
    const unsigned long DEBUG_INTERVAL = 1000; // Print debug info every second
    if (!is_recording || !audioBuffer || bufferIndex >= BUFFER_SIZE)
    {
        if (bufferIndex >= BUFFER_SIZE)
        {
            DEBUG_PRINT("Buffer full - stopping recording");
            stopRecording();
        }
        else if (!is_recording)
        {
            DEBUG_PRINT("Not recording - stopping update");
        }
        else if (!audioBuffer)
        {
            DEBUG_PRINT("No audio buffer - stopping update");
        }
        return;
    }
    size_t remainingBytes = BUFFER_SIZE - bufferIndex;
    size_t remainingSeconds = remainingBytes / (SAMPLE_RATE * CHANNELS * 2);


    // Periodic debug info
    unsigned long currentTime = millis();
    if (currentTime - lastDebugPrint >= DEBUG_INTERVAL)
    {
        printADCInfo();
        printBufferStatus();
        printRecordingStatus();
        lastDebugPrint = currentTime;
        DEBUG_PRINTF("Remaining buffer: %lu bytes (%lu seconds)\n",
                     remainingBytes, remainingSeconds);
    }
 
    // Check maximum recording time
    if (currentTime - recordStartTime >= MAX_RECORD_SECONDS * 1000)
    {
        DEBUG_PRINTF("Max recording time reached at %lu ms\n", currentTime);
        stopRecording();
        return;
    }

    // Calculate time since last sample
    unsigned long sampleInterval = 1000 / SAMPLE_RATE;

    if (currentTime - lastSampleTime >= sampleInterval)
    {
        // Create a small buffer for voice detection
        uint16_t samples[SAMPLE_BUFFER_SIZE];
        size_t samplesToTake = std::min(
            static_cast<size_t>(SAMPLE_BUFFER_SIZE),
            (BUFFER_SIZE - bufferIndex) / 2);

        if (samplesToTake == 0)
        {
            DEBUG_PRINT("No more buffer space available");
            stopRecording();
            return;
        }

        DEBUG_PRINTF("Taking %d samples...\n", samplesToTake);
        for (size_t i = 0; i < samplesToTake; i++)
        {
            int16_t sample = readADCSample();
            samples[i] = sample + 2048; // Store raw sample for voice detection

            if (hasDetectedVoice)
            {
                // Add buffer overflow protection
                if (bufferIndex + 2 >= BUFFER_SIZE)
                {
                    DEBUG_PRINT("Buffer overflow prevented - stopping recording");
                    stopRecording();
                    return;
                }

                audioBuffer[bufferIndex++] = sample & 0xFF;
                audioBuffer[bufferIndex++] = (sample >> 8) & 0xFF;
            }
        }

        bool hasVoice = detectVoiceActivity(samples, samplesToTake);

        if (hasVoice)
        {
            lastSoundTime = currentTime;
            if (!hasDetectedVoice)
            {
                hasDetectedVoice = true;
                DEBUG_PRINT("Voice detected - recording started");
            }
            DEBUG_PRINTF("Sound detected at %lu ms, buffer at %lu bytes (%lu bytes remaining)\n",
                         currentTime, bufferIndex, BUFFER_SIZE - bufferIndex);
        }
        else if (hasDetectedVoice && (currentTime - lastSoundTime >= SILENCE_TIMEOUT))
        {
            DEBUG_PRINTF("Silence timeout: last sound was %lu ms ago\n",
                            currentTime - lastSoundTime);
            stopRecording();
            return;
        }

        lastSampleTime = currentTime;
        // Debug sample buffer status
        if (hasDetectedVoice)
        {
            DEBUG_PRINTF("Recorded %d bytes. Buffer at %.1f%%\n",
                         bufferIndex,
                         (float)bufferIndex / BUFFER_SIZE * 100);
        }
    }
}