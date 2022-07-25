const unsigned long DEBOUNCE_DELAY = 100;
const unsigned long HOLD_DELAY = 250;
const unsigned long DELAY_BETWEEN_HOLD_SIGNALS = 150;

#define OUTPUTPIN A3
#define NEC_TIME 560
#define DEBUG

enum class KenwoodButton {
  None = -1,
  Src = 0x13,
  VolumeUp = 0x14,
  VolumeDown = 0x15,
  Back = 0x85,
  Att = 0x16, //mute
  Aud = 0x17,
  DirectAndOk = 0xF,
  StarAndAm = 0xC,
  HashAndFm = 0xD,
  One = 0x1,
  Two = 0x2,
  Three = 0x3,
  Four = 0x4,
  Five = 0x5,
  Six = 0x6,
  Seven = 0x7,
  Eight = 0x8,
  Nine = 0x9,
  Nought = 0x0,
  Answer = 0x92,
  Reject = 0x88,
  Up = 0x87,
  Down = 0x86,
  Rewind = 0xA,
  Forward = 0xB,
  EnterPlayPause = 0xE
};

void logDebug(String string) {
  #ifdef DEBUG
  Serial.println(string);
  #endif
}

void logDebug(int number) {
  #ifdef DEBUG
  Serial.println(number);
  #endif
}

void sendKenwoodCommand(KenwoodButton button);

class Button {
  public:
    String text;
    KenwoodButton button;
  private: 
    long startedHoldAt;
    bool isHeld;
    bool wasPressed;
    bool pressed;
    bool lastState;
    long lastDebounceTime;

  public:
    Button(String text, KenwoodButton button) {
      this->text = text;
      this->button = button;
    }
  
  protected:
    void onPress();
    void onHold();

    bool checkDelay(long delay);
    bool checkDelay(long delay, long start);
  
    void toggleState(bool currentState);

  public: 
    void updateState(int currentState);

    bool isNewlyPressed();

    bool isBeingHeld();
};

bool Button::isBeingHeld() {
      return isHeld;
    }

void Button::toggleState(bool currentState) {
  wasPressed = pressed;
  
  if (currentState != lastState) {
    lastDebounceTime = millis();
  }

  if (checkDelay(DEBOUNCE_DELAY)) {
    if (currentState != this->pressed) {
      pressed = currentState;
    }
  }

  if (currentState && checkDelay(HOLD_DELAY)) {
    if (!isHeld) {
      startedHoldAt = lastDebounceTime;
    }
    isHeld = true;
  } else {
    isHeld = false;
    startedHoldAt = 0;
  };
  
  lastState = currentState;

  if (isNewlyPressed()) {
    onPress();
  } else if (isBeingHeld()) {
    onHold();
  }
}

bool Button::checkDelay(long delay, long start) {
  return (millis() - start) > delay;
}

bool Button::checkDelay(long delay) {
  return checkDelay(delay, lastDebounceTime);
}

bool Button::isNewlyPressed() {
  return !wasPressed && pressed;
}

void Button::onHold() {
  if (checkDelay(DELAY_BETWEEN_HOLD_SIGNALS, startedHoldAt)) {
    logDebug("HELD " + text);
    sendKenwoodRepeatCommand();
    startedHoldAt = millis();
  }
}

void Button::onPress() {
  if (button != KenwoodButton::None) {
    sendKenwoodCommand(button);
  }
}

class DigitalButton : public Button {
  public:
    int inputPin;
  
    DigitalButton(int inputPin, String text, KenwoodButton button = KenwoodButton::None) : Button(text, button) {
       this->inputPin = inputPin;
    }

    void updateState(int currentValue) {
      toggleState(!((bool)currentValue)); 
    }
};

class AnalogButton : public Button {    
  public:
    int expectedValue;
  
  private: 
    int lowerBoundary;
    int upperBoundary;
  
    bool isValueWithinRange(int value) {
      return lowerBoundary < value && value < upperBoundary;
    }

  public:
    AnalogButton(int expectedValue, String text, KenwoodButton button = KenwoodButton::None) : Button(text, button) {
       this->expectedValue = expectedValue;
    }

    void setLowerBoundary(int allowedDeviation) {
      this->lowerBoundary = expectedValue - allowedDeviation;
    }

    void setUpperBoundary(int allowedDeviation) {
      this->upperBoundary = expectedValue + allowedDeviation;
    }
    
    void updateState(int currentValue) {
      bool isPressed = isValueWithinRange(currentValue);
      
      toggleState(isPressed); 
    }
};

class AnalogButtonGroup {
  public:
    int inputPin;

  protected:
    template<int N> void updateButtonStates(AnalogButton* (&buttons)[N]) {
      int analogInput = analogRead(inputPin);
  
      for (AnalogButton* button : buttons) {
        button->updateState(analogInput);
        if (button->isNewlyPressed()) {
           logDebug(button->expectedValue);
           logDebug(button->text);
        }
      }
    }
  
    template<int N> void initButtons(AnalogButton* (&buttons)[N], int mode = INPUT_PULLUP) {
      pinMode(inputPin, mode);
      
      AnalogButton* previousButton = NULL;
    
      for (AnalogButton* button : buttons) {
        int diff = previousButton == NULL
          ? button->expectedValue
          : (button->expectedValue - previousButton->expectedValue) / 2;

        button->setLowerBoundary(diff);

        if (previousButton != NULL) {
          previousButton->setUpperBoundary(diff);
        }
    
        previousButton = button;
      }

      previousButton->setUpperBoundary(150);
    }
    
  public: 
    virtual void init();

    AnalogButtonGroup(int inputPin) {
      this->inputPin = inputPin;
    }

    virtual void updateStates();
};

AnalogButton* stockKeypadButtons[] = { 
  //new AnalogButton(14, "bal", KenwoodButton::Att),
  new AnalogButton(65, "load", KenwoodButton::EnterPlayPause),
  new AnalogButton(220, "eject", KenwoodButton::Back),
  new AnalogButton(354, "two", KenwoodButton::Two),
  new AnalogButton(448, "one", KenwoodButton::One),
  new AnalogButton(573, "three", KenwoodButton::Three),
  //bad wiring - new AnalogButton(627, "any"),
  //new AnalogButton(655, "tone", KenwoodButton::Aud)
};

class StockKeypad : public AnalogButtonGroup {    
  public:
    StockKeypad(int inputPin) : AnalogButtonGroup(inputPin) {
    }

    void init() override {
      initButtons(stockKeypadButtons);
    }

    void updateStates() override {
      updateButtonStates(stockKeypadButtons);
    }
};

AnalogButton* steeringWheelButtons[] = { 
  new AnalogButton(22, "mute", KenwoodButton::Att),
  new AnalogButton(84, "vol+", KenwoodButton::VolumeUp),
  new AnalogButton(170, "vol-", KenwoodButton::VolumeDown),
  new AnalogButton(271, "mode", KenwoodButton::Src),
  new AnalogButton(418, "seek+", KenwoodButton::Forward),
  new AnalogButton(614, "seek-", KenwoodButton::Rewind)
  //,  new AnalogButton(930, "none", KenwoodButton::None)
};

class SteeringWheel : public AnalogButtonGroup {    
  public:
    SteeringWheel(int inputPin) : AnalogButtonGroup(inputPin) {
    }

    void init() override {
      initButtons(steeringWheelButtons);
    }

    void updateStates() override {
      updateButtonStates(steeringWheelButtons);
    }
};


StockKeypad* stockKeypad = new StockKeypad(A5);
SteeringWheel* steeringWheel = new SteeringWheel(A2);

DigitalButton* digitalButtons[] = {
  new DigitalButton(2, "up", KenwoodButton::Up),
  new DigitalButton(3, "rwd", KenwoodButton::Rewind),
  new DigitalButton(4, "fwd", KenwoodButton::Forward),
  new DigitalButton(5, "six", KenwoodButton::Six),
  new DigitalButton(6, "down", KenwoodButton::Down),
  new DigitalButton(7, "ti", KenwoodButton::Reject),
  new DigitalButton(8, "news", KenwoodButton::Answer),
  new DigitalButton(9, "five", KenwoodButton::Five),
  new DigitalButton(10, "rpt", KenwoodButton::HashAndFm),
  new DigitalButton(11, "rdm", KenwoodButton::StarAndAm),
  new DigitalButton(13, "four", KenwoodButton::Four),
  new DigitalButton(A1, "bal", KenwoodButton::Aud),
  new DigitalButton(A4, "scan", KenwoodButton::EnterPlayPause)
};

void initDigitalButtons() {
  for (DigitalButton* button : digitalButtons) {
    pinMode(button->inputPin, INPUT_PULLUP);
  }
}

void setup() {
  #ifdef DEBUG
  Serial.begin(9600);
  #endif  
  initDigitalButtons();
  stockKeypad->init();
  steeringWheel->init();
}

void loop() {
  stockKeypad->updateStates();
  steeringWheel->updateStates();
  
  for (DigitalButton* button : digitalButtons) {
    int digitalInput = digitalRead(button->inputPin);
    
    button->updateState(digitalInput);
    if (button->isNewlyPressed()) {
        logDebug(button->inputPin);
        logDebug(button->text);
    }
  }
}

void sendNecMessage(int highBits, int lowBits)
{
    digitalWrite(OUTPUTPIN, HIGH);
    for (int i = 0; i < highBits; i++) {
        delayMicroseconds (NEC_TIME);
    }
    
    digitalWrite(OUTPUTPIN, LOW);
    for (int i = 0; i < lowBits; i++) { 
        delayMicroseconds(NEC_TIME);
    }
}

void sendNecCommandStart()
{
  sendNecMessage(16, 8);
}

void sendNecCommandEnd()
{
  sendNecMessage(1, 0);
}

void sendNecOne()
{
  sendNecMessage(1, 3);
}

void sendNecZero()
{
  sendNecMessage(1, 1);
}

void sendNec8Bits(int *bits)
{
  for (int i = 0; i < 8; i++) {
    if(*bits & 0x01)
      sendNecOne();
    else
      sendNecZero();
    *bits = *bits >> 1;
  }
}

void sendNecData(int address, int data)
{
  int invertedAddress = ~address;
  int invertedData  = ~data;
  sendNec8Bits(&address);
  sendNec8Bits(&invertedAddress);
  sendNec8Bits(&data);
  sendNec8Bits(&invertedData);
}

void sendKenwoodRepeatCommand() {
  sendNecMessage(16, 4);
  sendNecCommandEnd();
}

void sendKenwoodCommand(KenwoodButton button) {
  sendNecCommandStart();
  sendNecData(0xb9, (int)button);
  sendNecCommandEnd();
}
