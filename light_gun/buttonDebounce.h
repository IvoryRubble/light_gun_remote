class ButtonDebounce {
  public:
    ButtonDebounce() {
    }

    bool isBtnPressed = false;
    bool isBtnReleased = false;
    bool isBtnReleasedLongPress = false;
    bool btnState = false;

    void updateState(bool btnStateInput) {
      btnStateInternal = !btnStateInput; // pull_up buttons
      unsigned long currentTime = millis();
      isBtnPressed = false;
      isBtnReleased = false;
      isBtnReleasedLongPress = false;

      if (!debounceDelayPassed && currentTime - previousStateChangeTime >= debounceDelay) {
        debounceDelayPassed = true;
      }

      if (!longPressTimeoutPassed && currentTime - previousStateChangeTime >= longPressTimeout) {
        longPressTimeoutPassed = true;
      }

      if (btnStateInternal != previousState && debounceDelayPassed) {
        btnState = btnStateInternal;
        isBtnPressed = btnStateInternal;
        isBtnReleased = !btnStateInternal;

        if (isBtnPressed) {
          longPressTimeoutPassed = false;
        }

        if (isBtnReleased && longPressTimeoutPassed) {
          longPressTimeoutPassed = false;
          isBtnReleasedLongPress = true;
        }

        debounceDelayPassed = false;
        previousStateChangeTime = currentTime;
        previousState = btnStateInternal;
      }
    }
  private:
    bool debounceDelayPassed = false;
    unsigned long debounceDelay = 5;

    bool longPressTimeoutPassed = false;
    unsigned long longPressTimeout = 1500;

    uint32_t previousStateChangeTime = 0;

    bool btnStateInternal = false;
    bool previousState = false;
};
