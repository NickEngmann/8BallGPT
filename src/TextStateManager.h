// TextStateManager.h
#ifndef TEXT_STATE_MANAGER_H
#define TEXT_STATE_MANAGER_H

#include <Arduino.h>

class TextStateManager
{
public:
  enum class DisplayState
  {
    IDLE,
    RECORDING,
    THINKING,
    RESPONSE,
    ERROR
  };

  TextStateManager() : currentState(DisplayState::IDLE),
                       lastUpdateTime(0),
                       periodCount(0),
                       maxPeriods(10),
                       responseStartTime(0),
                       responseDisplayDuration(5000) // 5 seconds
  {
  }

  void update(unsigned long currentTime)
  {
    switch (currentState)
    {
    case DisplayState::RECORDING:
      updateRecordingText(currentTime);
      break;
    case DisplayState::THINKING:
      updateThinkingText(currentTime);
      break;
    case DisplayState::RESPONSE:
      updateResponseText(currentTime);
      break;
    default:
      break;
    }
  }

  void setState(DisplayState newState, const String &response = "")
  {
    currentState = newState;
    lastUpdateTime = millis();
    periodCount = (newState == DisplayState::RECORDING) ? maxPeriods : 0;

    if (newState == DisplayState::RESPONSE)
    {
      responseText = response;
      responseStartTime = millis();
    }
  }

  DisplayState getState() const
  {
    return currentState;
  }

  String getCurrentText()
  {
    switch (currentState)
    {
    case DisplayState::IDLE:
      return String("Magic\nGPT8\n\nShake me");
    case DisplayState::ERROR:
      return String("Wifi: X");
    case DisplayState::RESPONSE:
      return responseText;
    default:
      return lastGeneratedText;
    }
  }

private:
  DisplayState currentState;
  unsigned long lastUpdateTime;
  int periodCount;
  const int maxPeriods;
  String lastGeneratedText;
  String responseText;
  unsigned long responseStartTime;
  const unsigned long responseDisplayDuration;

  void updateRecordingText(unsigned long currentTime)
  {
    if (currentTime - lastUpdateTime >= 1000)
    { // Update every second
      lastUpdateTime = currentTime;
      periodCount = max(0, periodCount - 1);

      if (periodCount <= 2)
      {
        lastGeneratedText = "Thinking";
      }
      else
      {
        String text = "Speak Now";
        for (int i = 0; i < periodCount; i++)
        {
          text += " .";
        }
        lastGeneratedText = text;
      }
    }
  }

  void updateThinkingText(unsigned long currentTime)
  {
    if (currentTime - lastUpdateTime >= 500)
    { // Update every half second
      lastUpdateTime = currentTime;
      periodCount = (periodCount + 1) % 4; // 0 to 3 periods

      String text = "Thinking";
      for (int i = 0; i < periodCount; i++)
      {
        text += ".";
      }
      lastGeneratedText = text;
    }
  }

  void updateResponseText(unsigned long currentTime)
  {
    if (currentTime - responseStartTime >= responseDisplayDuration)
    {
      setState(DisplayState::IDLE);
    }
  }
};

#endif // TEXT_STATE_MANAGER_H