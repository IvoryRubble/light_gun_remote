class Blinker {
  public:
    Blinker() {
    }

    int state = LOW;

    void setPeriod(unsigned long highPeriod, unsigned long lowPeriod) {
      _highPeriod = highPeriod;
      _lowPeriod = lowPeriod;
    }
    void update() {
      unsigned long currentTime = millis() % (_highPeriod + _lowPeriod);
      if (currentTime < _highPeriod) {
        state = HIGH;
      } else {
        state = LOW;
      }
    }
  private:
    unsigned long _highPeriod = 500;
    unsigned long _lowPeriod = 500;
};
