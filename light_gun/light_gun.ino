// LightGun

// Based on
// https://www.dfrobot.com/product-1088.html
// https://github.com/samuelballantyne/IR-Light-Gun

// Reciever led very fast alternate blink: camera is disconnected
// Reciever led slow alternate blink: LightGun is disconnected

// Side leds: red and blue for two detected points
// Side leds very fast alternate blink: camera is disconnected
// Side button: pause
// Side button long press: print help
// Side button press while hold up button: calibrate
// Side button press while hold left button: reset

// Press up button during startup for enable logs

// Controls: WASD, trigger: left mouse button (on screen) or right mouse button (off screen)

#include <Keyboard.h>
#include <AbsMouse.h>
#include <EEPROM.h>
#include <DFRobotIRPosition.h>
#include <SerialTransfer.h>
#include "buttonDebounce.h"
#include "blinker.h"

#define bitset(byte, nbit, val) (val ? ((byte) |=  (1<<(nbit))) : ((byte) &= ~(1<<(nbit))))
#define bitcheck(byte, nbit) (!!((byte) & (1<<(nbit))))
#define mapf(val, in_min, in_max, out_min, out_max) (float)(val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min

const bool isHost = true;     // true to compile host (usb dongle) firmware
// const bool isHost = false; // false to compile light_gun (with ir camera) firmware
                              // light_gun support both wire and wireless (due usb dongle) connection
                              
bool isWireConnected = false;

Blinker blinker;

enum State {
  mainState = 0,
  calibrationState1 = 1,
  calibrationState2 = 2,
  pauseState = 3,
  noConnectionState = 4
};
State state = noConnectionState;
State previousStateForInputEnabled = mainState;

// Log timeout
const unsigned long logMessageTimeout = 0;
unsigned long logMessagePreviousTime = 0;

bool isLogEnabledByKey = false;

// screen resolution
const int res_x = 1920;
const int res_y = 1080;
// int res_x = 1280;
// int res_y = 720;

// calibration offsets
const int xCalibrationOffset = 300;
const int yCalibrationOffset = 200;
const int xCalibrationPoint2 = 0 + xCalibrationOffset;
const int yCalibrationPoint2 = 0 + yCalibrationOffset;
const int xCalibrationPoint1 = res_x - xCalibrationOffset;
const int yCalibrationPoint1 = res_y - yCalibrationOffset;

// calibration defaults
const float xCalibration2Default = 0.3;
const float yCalibration2Default = -0.5;
const float xCalibration1Default = 0.8;
const float yCalibration1Default = 0.5;

// calibration points
float xCalibration2 = xCalibration2Default;
float yCalibration2 = yCalibration2Default;
float xCalibration1 = xCalibration1Default;
float yCalibration1 = yCalibration1Default;

// RAW Sensor Values
int rawX[4];
int rawY[4];

const int triggerButtonPin = A0;
const int upButtonPin = 6;
const int downButtonPin = 7;
const int leftButtonPin = 8;
const int rightButtonPin = 5;
const int reloadButtonPin = 9;

const int ledG = isHost ? 15 : 12;
const int ledR = 14;
const int ledB = isHost ? 12 : 15;

// button states
int triggerButtonState = HIGH;
int upButtonState = HIGH;
int downButtonState = HIGH;
int leftButtonState = HIGH;
int rightButtonState = HIGH;
int reloadButtonState = HIGH;

bool isCamAvailable = false;

// detected ir-light spots in different orders
int topX = 0;
int topY = 0;
int bottomX = 0;
int bottomY = 0;

int leftX = 0;
int leftY = 0;
int rightX = 0;
int rightY = 0;

// calculated point
float finalX = 0;
float finalY = 0;

int onScreenX_unconstrained = 0;
int onScreenY_unconstrained = 0;

int onScreenX = 0;
int onScreenY = 0;

int mouseMoveX = 0;
int mouseMoveY = 0;

ButtonDebounce triggerButton;
ButtonDebounce upButton;
ButtonDebounce downButton;
ButtonDebounce leftButton;
ButtonDebounce rightButton;
ButtonDebounce reloadButton;

const bool isOffScreenEnabled = true;
bool isOffScreen = false;
bool isOffScreenButtonPressed = true;

const int mouseGuardPin = 4;
bool isInputEnabledByMouseGuard = false;

const unsigned long inputTimeout = 1000;
unsigned long lastRemoteReadTime = 0;
bool isInputEnabledByTimeout = true;

bool buttonsReleased = false;
bool stateResetted = false;

unsigned long firstRemoteReadTime = 0;

// getPosition() method variables
const int nX = 512;
const int nY = 384;
int nDeltaX = 0;
int nDeltaY = 0;
int a = 0;
int b = 0;
int c = 0;
int d = 0;

struct RemoteData {
  int rawX[2];
  int rawY[2];
  unsigned int states;
};
struct RemoteData remoteData;

struct StorageData {
  float xCalibration2;
  float yCalibration2;
  float xCalibration1;
  float yCalibration1;
  int hash;
  int calcHash() {
    return (int)(xCalibration2 * 10) + (int)(yCalibration2 * 10) + (int)(xCalibration1 * 10) + (int)(yCalibration1 * 10) + 12;
  }
  void updateHash() {
    hash = calcHash();
  }
  bool checkHash() {
    return hash == calcHash();
  }
};
struct StorageData storageData;
const int storageDataAddress = 16;

SerialTransfer myTransfer;

DFRobotIRPosition myDFRobotIRPosition;

void setup() {
  pinMode(mouseGuardPin, INPUT_PULLUP);
  // pinMode(LED_BLINK, OUTPUT);

  pinMode(triggerButtonPin, INPUT_PULLUP);
  pinMode(upButtonPin, INPUT_PULLUP);
  pinMode(downButtonPin, INPUT_PULLUP);
  pinMode(leftButtonPin, INPUT_PULLUP);
  pinMode(rightButtonPin, INPUT_PULLUP);
  pinMode(reloadButtonPin, INPUT_PULLUP);

  pinMode(ledR, OUTPUT);
  pinMode(ledG, OUTPUT);
  pinMode(ledB, OUTPUT);

  checkLogEnabledOnStartup();

  powerOnBlink();

  if (!isHost) {
    myDFRobotIRPosition.begin();            // Start IR Camera
  }

  delayBlink(2000);

  checkWireConnected();

  Serial.begin(115200);
  Serial1.begin(115200);

  if (isHost || !isWireConnected) {
    myTransfer.begin(Serial1, false);
  }

  if (isHost || isWireConnected) {
    AbsMouse.init(res_x, res_y);
  }
  getInputEnabledByMouseGuard();
  getStorageData();

  delayBlink(500);

  if ((isHost || isWireConnected) && isInputEnabledByMouseGuard) {
    AbsMouse.move((res_x / 2), (res_y / 2));
    delayBlink(500);
  }

  startupBlink();
  lastRemoteReadTime = millis();
}

void loop() {
  if (isHost || isWireConnected) {
    loopWired();
  } else {
    loopWireless();
  }
}

void loopWireless() {
  getButtons();
  getCamData();
  updateButtons();

  PrintResultsRemote();

  blinkerUpdate();
  delay(2);
}

void loopWired() {
  // return;

  getDataWired();
  getInputEnabledByMouseGuard();
  checkInputEnabled();
  getPosition();
  updateButtons();

  switch (state) {
    case noConnectionState: {
        if (isInputEnabledByTimeout) {
          state = previousStateForInputEnabled;
          firstRemoteReadTime = millis();
        }
      }
      break;
    case pauseState: {
        if (isCalibrateButtonsReleased()) {
          releaseButtons();
          state = calibrationState2;
        } else if (reloadButton.isBtnReleased || triggerButton.isBtnPressed || isRegularButtonPressed()) {
          releaseButtons();
          state = mainState;
        }
      }
      break;
    case calibrationState2: {
        mouseMove(xCalibrationPoint2, yCalibrationPoint2);
        if (reloadButton.isBtnReleased || isRegularButtonPressed()) {
          releaseButtons();
          state = mainState;
        }
        if (triggerButton.isBtnPressed && !isOffScreen) {
          releaseButtons();
          xCalibration2 = finalX;
          yCalibration2 = finalY;
          state = calibrationState1;
        }
      }
      break;
    case calibrationState1: {
        mouseMove(xCalibrationPoint1, yCalibrationPoint1);
        if (reloadButton.isBtnReleased || isRegularButtonPressed()) {
          releaseButtons();
          resetCalibration();
          state = mainState;
        }
        if (triggerButton.isBtnPressed && !isOffScreen) {
          releaseButtons();
          xCalibration1 = finalX;
          yCalibration1 = finalY;
          putStorageData();
          state = mainState;
        }
      }
      break;
    case mainState: {
        setButtons();
        if (!isOffScreen) {
          mouseMove(onScreenX, onScreenY);
        }
        if (isCalibrateButtonsReleased()) {
          releaseButtons();
          state = calibrationState2;
        } else if (reloadButton.isBtnReleased) {
          releaseButtons();
          state = pauseState;
        }
      }
      break;
  }

  if (isResetButtonsReleased()) {
    resetAll();
  }

  if (isHelpButtonsReleased()) {
    printHelp();
  }

  blinkerUpdate();

  PrintResults();
  // delay(100);
}

void checkLogEnabledOnStartup() {
  if (!isHost) {
    isLogEnabledByKey = !digitalRead(upButtonPin);
  }
}

void checkWireConnected() {
  if (isHost) {
    isWireConnected = true;
  } else {
    isWireConnected = UDADDR & _BV(ADDEN);
  }
}

void powerOnBlink() {
  for (int i = 0; i < 5; i++) {
    unsigned long delayTime = 200;
    digitalWrite(ledB, HIGH);
    digitalWrite(ledG, HIGH);
    digitalWrite(ledR, HIGH);
    delay(delayTime);
    digitalWrite(ledB, LOW);
    digitalWrite(ledG, LOW);
    digitalWrite(ledR, LOW);
    delay(delayTime);
  }
}

void delayBlink(unsigned long delayTime) {
  unsigned long startTime = millis();
  while (millis() - startTime < delayTime) {
    digitalWrite(ledB, HIGH);
    digitalWrite(ledG, HIGH);
    digitalWrite(ledR, HIGH);
    delay(50);
    digitalWrite(ledB, LOW);
    digitalWrite(ledG, LOW);
    digitalWrite(ledR, LOW);
    delay(50);
  }
}

void startupBlink() {
  if (isHost || isWireConnected) {
    for (int i = 0; i < 5; i++) {
      unsigned long delayTime = 100;
      digitalWrite(ledB, HIGH);
      digitalWrite(ledG, HIGH);
      delay(delayTime);
      digitalWrite(ledB, LOW);
      digitalWrite(ledG, LOW);
      delay(delayTime);
      digitalWrite(ledB, HIGH);
      digitalWrite(ledG, HIGH);
      delay(delayTime * 3);
      digitalWrite(ledB, LOW);
      digitalWrite(ledG, LOW);
      delay(delayTime);
    }
  } else {
    for (int i = 0; i < 5; i++) {
      unsigned long delayTime = 100;
      digitalWrite(ledR, HIGH);
      digitalWrite(ledB, LOW);
      delay(delayTime);
      digitalWrite(ledR, LOW);
      digitalWrite(ledB, LOW);
      delay(delayTime);
      digitalWrite(ledR, LOW);
      digitalWrite(ledB, HIGH);
      delay(delayTime);
      digitalWrite(ledR, LOW);
      digitalWrite(ledB, LOW);
      delay(delayTime);
    }
  }
}

void blinkerUpdate() {
  if (isHost) {
    if (state == noConnectionState) {
      blinker.setPeriod(1000, 1000);
      blinker.update();
      digitalWrite(ledB, LOW);
      digitalWrite(ledG, blinker.state);
      digitalWrite(ledR, !blinker.state);
    } else if (isCamAvailable) {
      if (upButton.btnState ||
          downButton.btnState ||
          leftButton.btnState ||
          rightButton.btnState ||
          triggerButton.btnState ||
          reloadButton.btnState) {
        blinker.setPeriod(100, 50);
        blinker.update();
        digitalWrite(ledB, LOW);
        digitalWrite(ledG, LOW);
        digitalWrite(ledR, blinker.state);
      } else if ((rawX[0] == 1023) && (rawX[1] == 1023)) {
        blinker.setPeriod(200, 100);
        blinker.update();
        digitalWrite(ledB, LOW);
        digitalWrite(ledG, blinker.state);
        digitalWrite(ledR, LOW);
      } else {
        blinker.setPeriod(3000, 100);
        blinker.update();
        digitalWrite(ledB, LOW);
        digitalWrite(ledG, blinker.state && (rawX[0] != 1023));
        digitalWrite(ledR, blinker.state && (rawX[1] != 1023));
      }
    } else {
      blinker.setPeriod(100, 100);
      blinker.update();
      digitalWrite(ledB, LOW);
      digitalWrite(ledG, blinker.state);
      digitalWrite(ledR, !blinker.state);
    }

  } else {
    if (isCamAvailable) {
      if (upButton.btnState ||
          downButton.btnState ||
          leftButton.btnState ||
          rightButton.btnState ||
          triggerButton.btnState ||
          reloadButton.btnState) {
        blinker.setPeriod(100, 50);
        blinker.update();
        digitalWrite(ledB, LOW);
        digitalWrite(ledG, LOW);
        digitalWrite(ledR, blinker.state);
      } else if ((rawX[0] == 1023) && (rawX[1] == 1023)) {
        blinker.setPeriod(200, 100);
        blinker.update();
        digitalWrite(ledB, blinker.state);
        digitalWrite(ledG, LOW);
        digitalWrite(ledR, LOW);
      } else {
        blinker.setPeriod(3000, 100);
        blinker.update();
        digitalWrite(ledB, blinker.state && (rawX[0] != 1023));
        digitalWrite(ledG, LOW);
        digitalWrite(ledR, blinker.state && (rawX[1] != 1023));
      }
    } else {
      blinker.setPeriod(100, 100);
      blinker.update();
      digitalWrite(ledB, blinker.state);
      digitalWrite(ledG, LOW);
      digitalWrite(ledR, !blinker.state);
    }
  }
}

void putStorageData() {
  storageData.xCalibration2 = xCalibration2;
  storageData.yCalibration2 = yCalibration2;
  storageData.xCalibration1 = xCalibration1;
  storageData.yCalibration1 = yCalibration1;
  storageData.updateHash();
  EEPROM.put(storageDataAddress, storageData);
}

void getStorageData() {
  EEPROM.get(storageDataAddress, storageData);
  if (storageData.checkHash()) {
    xCalibration2 = storageData.xCalibration2;
    yCalibration2 = storageData.yCalibration2;
    xCalibration1 = storageData.xCalibration1;
    yCalibration1 = storageData.yCalibration1;
  }
}

void getCamData() {
  myDFRobotIRPosition.requestPosition();

  if (myDFRobotIRPosition.available()) {
    isCamAvailable = true;
    for (int i = 0; i < 4; i++) {
      rawX[i] = myDFRobotIRPosition.readX(i);
      rawY[i] = myDFRobotIRPosition.readY(i);
    }
  } else {
    // Serial.println("Device not available!");
    isCamAvailable = false;
  }
}

void getButtons() {
  reloadButtonState = digitalRead(reloadButtonPin);
  triggerButtonState = digitalRead(triggerButtonPin);
  upButtonState = digitalRead(upButtonPin);
  downButtonState = digitalRead(downButtonPin);
  leftButtonState = digitalRead(leftButtonPin);
  rightButtonState = digitalRead(rightButtonPin);
}

void getDataWired() {
  if (isHost) {
    getRemoteDataHost();
  } else {
    getCamData();
    getButtons();
  }
}

void getRemoteDataHost() {
  unsigned long currentTime = millis();
  isInputEnabledByTimeout = (currentTime - lastRemoteReadTime < inputTimeout);

  bool isRead = false;
  while (myTransfer.available()) {
    myTransfer.rxObj(remoteData);
    isRead = true;
  }

  if (isRead) {
    lastRemoteReadTime = currentTime;

    rawX[0] = remoteData.rawX[0];
    rawY[0] = remoteData.rawY[0];
    rawX[1] = remoteData.rawX[1];
    rawY[1] = remoteData.rawY[1];

    triggerButtonState = bitcheck(remoteData.states, 0);
    upButtonState = bitcheck(remoteData.states, 1);
    downButtonState = bitcheck(remoteData.states, 2);
    leftButtonState = bitcheck(remoteData.states, 3);
    rightButtonState = bitcheck(remoteData.states, 4);
    reloadButtonState = bitcheck(remoteData.states, 5);
    isCamAvailable = bitcheck(remoteData.states, 6);
    isLogEnabledByKey = bitcheck(remoteData.states, 7);
  }
}

void getInputEnabledByMouseGuard() {
  if (isHost) {
    isInputEnabledByMouseGuard = true;
    // isInputEnabledByMouseGuard = !digitalRead(mouseGuardPin);
  } else {
    isInputEnabledByMouseGuard = true;
  }
}

void checkInputEnabled() {
  if (!isInputEnabledByTimeout) {
    if (!buttonsReleased) {
      releaseButtons();
      buttonsReleased = true;
    }
    if (!stateResetted) {
      previousStateForInputEnabled = state;
      state = noConnectionState;
      stateResetted = true;
    }
  }
  if (!isInputEnabledByMouseGuard) {
    if (!buttonsReleased) {
      releaseButtons();
      buttonsReleased = true;
    }
  }
  if (reloadButton.btnState) {
    if (!buttonsReleased) {
      releaseButtons();
      buttonsReleased = true;
    }
  }

  if (isInputEnabledByTimeout) {
    stateResetted = false;
  }
  if (isInputEnabledByMouseGuard && isInputEnabledByTimeout && !reloadButton.btnState) {
    buttonsReleased = false;
  }
}

void mouseMove(int x, int y) {
  if (isInputEnabledByMouseGuard && isInputEnabledByTimeout && !reloadButton.btnState && isCamAvailable) {
    AbsMouse.move(x, y);
  }
  mouseMoveX = x;
  mouseMoveY = y;
}

void getPosition() {
  if (isCamAvailable) {
    if (rawX[0] != 1023 && rawX[1] != 1023) {

      if (rawX[0] > rawX[1]) {
        topX = rawX[0];
        topY = rawY[0];
        bottomX = rawX[1];
        bottomY = rawY[1];
      } else {
        topX = rawX[1];
        topY = rawY[1];
        bottomX = rawX[0];
        bottomY = rawY[0];
      }

      if (rawY[0] > rawY[1]) {
        leftX = rawX[0];
        leftY = rawY[0];
        rightX = rawX[1];
        rightY = rawY[1];
      } else {
        leftX = rawX[1];
        leftY = rawY[1];
        rightX = rawX[0];
        rightY = rawY[0];
      }

      nDeltaX = nX - bottomX;
      nDeltaY = nY - bottomY;

      a = topX - bottomX;
      b = topY - bottomY;
      c = -topY + bottomY;
      d = topX - bottomX;

      finalX = ((float)nDeltaX * d - (float)nDeltaY * c) / ((float)a * d - (float)c * b);
      finalY = ((float)nDeltaX * b - (float)nDeltaY * a) / ((float)c * b - (float)a * d);

      // finalX = 512 + cos(atan2(bottomY - topY, bottomX - topX) * -1) * (((topX - bottomX) / 2 + bottomX) - 512) - sin(atan2(bottomY - topY, bottomX - topX) * -1) * (((topY - bottomY) / 2 + bottomY) - 384);
      // finalY = 384 + sin(atan2(bottomY - topY, bottomX - topX) * -1) * (((topX - bottomX) / 2 + bottomX) - 512) + cos(atan2(bottomY - topY, bottomX - topX) * -1) * (((topY - bottomY) / 2 + bottomY) - 384);

      // swap X and Y
      onScreenX_unconstrained = round(mapf(finalY, yCalibration2, yCalibration1, xCalibrationPoint2, xCalibrationPoint1));
      onScreenY_unconstrained = round(mapf(finalX, xCalibration2, xCalibration1, yCalibrationPoint2, yCalibrationPoint1));
      onScreenX = constrain(onScreenX_unconstrained, 0, res_x);
      onScreenY = constrain(onScreenY_unconstrained, 0, res_y);
      isOffScreen = false;
    } else {
      isOffScreen = true;
    }
  }
}

void updateButtons() {
  triggerButton.updateState(triggerButtonState);
  upButton.updateState(upButtonState);
  downButton.updateState(downButtonState);
  leftButton.updateState(leftButtonState);
  rightButton.updateState(rightButtonState);
  reloadButton.updateState(reloadButtonState);
}

void setButton(ButtonDebounce btn, byte val) {
  if (btn.isBtnPressed) {
    Keyboard.press(val);
  } else if (btn.isBtnReleased) {
    Keyboard.release(val);
  }
}

void setButtons() {
  if (isInputEnabledByMouseGuard && isInputEnabledByTimeout && !reloadButton.btnState) {
    if (triggerButton.isBtnPressed) {
      isOffScreenButtonPressed = isOffScreenEnabled && isOffScreen;
      if (isOffScreenButtonPressed) {
        AbsMouse.press(MOUSE_RIGHT);
      } else {
        AbsMouse.press(MOUSE_LEFT);
      }
    } else if (triggerButton.isBtnReleased) {
      if (isOffScreenButtonPressed) {
        AbsMouse.release(MOUSE_RIGHT);
      } else {
        AbsMouse.release(MOUSE_LEFT);
      }
    }

    setButton(upButton, 'w' /*KEY_UP_ARROW*/);
    setButton(downButton, 's' /*KEY_DOWN_ARROW*/);
    setButton(leftButton, 'a' /*KEY_LEFT_ARROW*/);
    setButton(rightButton, 'd' /*KEY_RIGHT_ARROW*/);
  }
}

bool isRegularButtonPressed() {
  return
    upButton.isBtnPressed ||
    downButton.isBtnPressed ||
    leftButton.isBtnPressed ||
    rightButton.isBtnPressed;
}

bool isCalibrateButtonsReleased() {
  return reloadButton.isBtnReleased && upButton.btnState;
}

bool isResetButtonsReleased() {
  return reloadButton.isBtnReleased && leftButton.btnState;
}

bool isHelpButtonsReleased() {
  return reloadButton.isBtnReleasedLongPress && !upButton.btnState && !downButton.btnState && !leftButton.btnState && !rightButton.btnState && !triggerButton.btnState;
}

void resetAll() {
  if (isLogEnabled()) {
    Serial.println("Reset all...");
  }
  storageData.xCalibration2 = 0;
  storageData.yCalibration2 = 0;
  storageData.xCalibration1 = 0;
  storageData.yCalibration1 = 0;
  storageData.hash = 0;
  EEPROM.put(storageDataAddress, storageData);
  setCalibrationToDefault();
  if (isLogEnabled()) {
    Serial.println("Done");
  }
  delay(2000);
}

void printHelp() {
  if (isLogEnabled()) {
    Serial.print(F("Please stand by...\n\n"));

    Serial.print(F("LightGun\n\n"));
    Serial.print(F("Based on:\n"));

    Serial.print(F("https://www.dfrobot.com/product-1088.html\n"));
    Serial.print(F("https://github.com/samuelballantyne/IR-Light-Gun\n\n"));

    Serial.print(F("Reciever led very fast alternate blink: camera is disconnected\n"));
    Serial.print(F("Reciever led slow alternate blink: LightGun is disconnected\n\n"));

    Serial.print(F("Side leds: red and blue for two detected points\n"));
    Serial.print(F("Side leds very fast alternate blink: camera is disconnected\n"));
    Serial.print(F("Side button: pause\n"));
    Serial.print(F("Side button long press: print help\n"));
    Serial.print(F("Side button press while hold up button: calibrate\n"));
    Serial.print(F("Side button press while hold left button: reset\n\n"));

    Serial.print(F("Press up button during startup for enable logs\n\n"));

    Serial.print(F("Resolution: "));
    Serial.print(res_x);
    Serial.print(F("x"));
    Serial.print(res_y);
    Serial.print(F("\n\n"));
    
    Serial.print(F("Controls: WASD, trigger: left mouse button (on screen) or right mouse button (off screen)\n\n"));
    printLogSerial();
  }

  openNotepad();

  Keyboard.print(F("Please stand by...\n\n"));
  delay(2000);
  Keyboard.print(F("LightGun\n\n"));
  Keyboard.print(F("Based on:\n"));

  Keyboard.print(F("https://www.dfrobot.com/product-1088.html\n"));
  Keyboard.print(F("https://github.com/samuelballantyne/IR-Light-Gun\n\n"));

  Keyboard.print(F("Reciever led very fast alternate blink: camera is disconnected\n"));
  Keyboard.print(F("Reciever led slow alternate blink: LightGun is disconnected\n\n"));

  Keyboard.print(F("Side leds: red and blue for two detected points\n"));
  Keyboard.print(F("Side leds very fast alternate blink: camera is disconnected\n"));
  Keyboard.print(F("Side button: pause\n"));
  Keyboard.print(F("Side button long press: print help\n"));
  Keyboard.print(F("Side button press while hold up button: calibrate\n"));
  Keyboard.print(F("Side button press while hold left button: reset\n"));

  Keyboard.print(F("Press up button during startup for enable logs\n\n"));

  Keyboard.print(F("Resolution: "));
  Keyboard.print(res_x);
  Keyboard.print(F("x"));
  Keyboard.print(res_y);
  Keyboard.print(F("\n\n"));
  
  Keyboard.print(F("Controls: WASD, trigger: left mouse button (on screen) or right mouse button (off screen)\n\n"));
  printLogKeyboard();
}

void openNotepad() {
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.write('r');
  Keyboard.release(KEY_LEFT_GUI);
  delay(1000);
  Keyboard.print("notepad");
  Keyboard.press(KEY_RETURN);
  Keyboard.release(KEY_RETURN);
  delay(2000);
}

void printLogSerial() {
  unsigned long currentTime = millis();
  unsigned long remoteUpTime = currentTime - firstRemoteReadTime;

  Serial.println("Telemetry:");
  Serial.print("IsCamAvailable: ");
  Serial.println(isCamAvailable);
  Serial.print("rawPoint1: (");
  if (rawX[0] != 1023) {
    Serial.print(rawX[0]);
    Serial.print(", ");
    Serial.print(rawY[0]);
  } else {
    Serial.print("N/A");
  }
  Serial.println(")");
  Serial.print("rawPoint2: (");
  if (rawX[1] != 1023) {
    Serial.print(rawX[1]);
    Serial.print(", ");
    Serial.print(rawY[1]);
  } else {
    Serial.print("N/A");
  }
  Serial.print(")\n\n");

  Serial.print("UpTime: ");
  Serial.print(currentTime / 1000 / 60 / 60);
  Serial.print(" h ");
  Serial.print(currentTime / 1000 / 60 % 60);
  Serial.print(" min ");
  Serial.print(currentTime / 1000 % 60);
  Serial.print(" sec\n\n");

  if (isHost) {
    Serial.print("Remote UpTime: ");
    Serial.print(remoteUpTime / 1000 / 60 / 60);
    Serial.print(" h ");
    Serial.print(remoteUpTime / 1000 / 60 % 60);
    Serial.print(" min ");
    Serial.print(remoteUpTime / 1000 % 60);
    Serial.print(" sec\n");
  }
}

void printLogKeyboard() {
  unsigned long currentTime = millis();
  unsigned long remoteUpTime = currentTime - firstRemoteReadTime;

  Keyboard.println("Telemetry:");
  Keyboard.print("IsCamAvailable: ");
  Keyboard.println(isCamAvailable ? "true" : "false");
  Keyboard.print("rawPoint1: (");
  if (rawX[0] != 1023) {
    Keyboard.print(rawX[0]);
    Keyboard.print(", ");
    Keyboard.print(rawY[0]);
  } else {
    Keyboard.print("N/A");
  }
  Keyboard.println(")");
  Keyboard.print("rawPoint2: (");
  if (rawX[1] != 1023) {
    Keyboard.print(rawX[1]);
    Keyboard.print(", ");
    Keyboard.print(rawY[1]);
  } else {
    Keyboard.print("N/A");
  }
  Keyboard.print(")\n\n");

  Keyboard.print("UpTime: ");
  Keyboard.print(currentTime / 1000 / 60 / 60);
  Keyboard.print(" h ");
  Keyboard.print(currentTime / 1000 / 60 % 60);
  Keyboard.print(" min ");
  Keyboard.print(currentTime / 1000 % 60);
  Keyboard.print(" sec\n\n");

  if (isHost) {
    Keyboard.print("Remote UpTime: ");
    Keyboard.print(remoteUpTime / 1000 / 60 / 60);
    Keyboard.print(" h ");
    Keyboard.print(remoteUpTime / 1000 / 60 % 60);
    Keyboard.print(" min ");
    Keyboard.print(remoteUpTime / 1000 % 60);
    Keyboard.print(" sec\n");
  }
}

void releaseButtons() {
  AbsMouse.release(MOUSE_RIGHT);
  AbsMouse.release(MOUSE_LEFT);
  Keyboard.releaseAll();
}

void resetCalibration() {
  EEPROM.get(storageDataAddress, storageData);
  if (storageData.checkHash()) {
    xCalibration2 = storageData.xCalibration2;
    yCalibration2 = storageData.yCalibration2;
    xCalibration1 = storageData.xCalibration1;
    yCalibration1 = storageData.yCalibration1;
  } else {
    setCalibrationToDefault();
  }
}

void setCalibrationToDefault() {
  xCalibration2 = xCalibration2Default;
  yCalibration2 = yCalibration2Default;
  xCalibration1 = xCalibration1Default;
  yCalibration1 = yCalibration1Default;
}

bool isLogEnabled() {
  return isLogEnabledByKey && isWireConnected && Serial.availableForWrite() >= 32;
}

void PrintResults() {
  if (!isLogEnabled()) {
    return;
  }

  unsigned long currentTime = millis();
  if (currentTime - logMessagePreviousTime > logMessageTimeout) {
    //PrintResultsReadable();
    PrintResultsForProcessing();
    //Serial.flush();
    logMessagePreviousTime = currentTime;
  }
}

void PrintResultsReadable() {    // Print results for debugging
  Serial.print("RAW: ");
  Serial.print(finalX);
  Serial.print(", ");
  Serial.print(finalY);

  Serial.print("\tnD: ");
  Serial.print(nDeltaX);
  Serial.print(", ");
  Serial.print(nDeltaY);
  //
  //  Serial.print("\tabcd: ");
  //  Serial.print(a);
  //  Serial.print(", ");
  //  Serial.print(b);
  //  Serial.print(", ");
  //  Serial.print(c);
  //  Serial.print(", ");
  //  Serial.print(d);

  Serial.print("\tCalibration: ");
  Serial.print(xCalibration2);
  Serial.print(", ");
  Serial.print(yCalibration2);
  Serial.print(", ");
  Serial.print(xCalibration1);
  Serial.print(", ");
  Serial.print(yCalibration1);

  Serial.print("\tPosition: ");
  Serial.print(onScreenX);
  Serial.print(", ");
  Serial.print(onScreenY);

  //  Serial.print("State: ");
  //  Serial.print(state);
  //
  //  Serial.print("\tButton states: ");
  //  Serial.print(triggerButton.btnState);
  //  Serial.print(", ");
  //  Serial.print(reloadButton.btnState);
  //
  //  Serial.print("\tButton pressed event: ");
  //  Serial.print(triggerButton.isBtnPressed);
  //  Serial.print(", ");
  //  Serial.print(reloadButton.isBtnPressed);
  //
  //  Serial.print("\tisInputEnabledByTimeout: ");
  //  Serial.print(isInputEnabledByTimeout);
  //
  //  Serial.print("\tisOffScreen: ");
  //  Serial.print(isOffScreen);

  Serial.println();
}

void PrintResultsForProcessing() {
  for (int i = 0; i < 4; i++) {
    Serial.print(rawY[i]);
    Serial.print( "," );
    Serial.print(rawX[i]);
    Serial.print( "," );
  }

  Serial.print(finalY);
  Serial.print(",");
  Serial.print(finalX);
  Serial.print(",");

  //Serial.print(onScreenX);
  //Serial.print(",");
  //Serial.print(onScreenY);
  //Serial.print(",");

  Serial.print(mouseMoveX);
  Serial.print(",");
  Serial.print(mouseMoveY);
  Serial.print(",");

  Serial.print(state);
  Serial.print(",");

  Serial.print(triggerButtonState);
  Serial.print(",");
  Serial.print(reloadButtonState);
  Serial.print(",");

  Serial.print(xCalibration2);
  Serial.print(",");
  Serial.print(yCalibration2);
  Serial.print(",");
  Serial.print(xCalibration1);
  Serial.print(",");
  Serial.print(yCalibration1);
  Serial.println();
}

void PrintResultsRemote() {
  remoteData.rawX[0] = rawX[0];
  remoteData.rawY[0] = rawY[0];
  remoteData.rawX[1] = rawX[1];
  remoteData.rawY[1] = rawY[1];

  bitset(remoteData.states, 0, triggerButtonState);
  bitset(remoteData.states, 1, upButtonState);
  bitset(remoteData.states, 2, downButtonState);
  bitset(remoteData.states, 3, leftButtonState);
  bitset(remoteData.states, 4, rightButtonState);
  bitset(remoteData.states, 5, reloadButtonState);
  bitset(remoteData.states, 6, isCamAvailable);
  bitset(remoteData.states, 7, isLogEnabledByKey);
  myTransfer.sendDatum(remoteData);

  Serial.flush();
}
