/*
  ============================================================
  Vault Protection System with PIN Authentication
  German International University — Computer Organization
  Spring 2026
  ============================================================

  PIN: Last 4 digits of a team member's ID (change SECRET_PIN below)

  HARDWARE CONNECTIONS:
  ┌──────────────────────────────────────────────────┐
  │ Component          │ Arduino Pin                 │
  ├──────────────────────────────────────────────────┤
  │ Button B0 (digit 0)│ 2                           │
  │ Button B1 (digit 1)│ 3                           │
  │ Button B2 (digit 2)│ 4                           │
  │ Button B3 (digit 3)│ 5                           │
  │ Enter Button       │ 6                           │
  │ Green LED          │ 7  (+ 330Ω to GND)          │
  │ Red LED            │ 8  (+ 330Ω to GND)          │
  │ Buzzer             │ 9                           │
  │ LCD SDA (I2C)      │ A4                          │
  │ LCD SCL (I2C)      │ A5                          │
  └──────────────────────────────────────────────────┘

  Each button: one terminal to Arduino pin, other to GND.
  INPUT_PULLUP is used (no external resistor needed for buttons).
*/

// ── Bonus LCD ──────────────────────────────────────────────
#define LCD_ENABLED 0

#if LCD_ENABLED
  #include <Wire.h>
  #include <LiquidCrystal_I2C.h>
  LiquidCrystal_I2C lcd(0x27, 16, 2);
#endif

// ── Pin Definitions ────────────────────────────────────────
const int BTN_PINS[4]  = {2, 3, 4, 5};   // B0, B1, B2, B3
const int BTN_ENTER    = 6;
const int LED_GREEN    = 7;
const int LED_RED      = 8;
const int BUZZER_PIN   = 9;

// ── Secret PIN ─────────────────────────────────────────────
const int SECRET_PIN[4] = {1, 1, 1, 1};  

// ── System State ───────────────────────────────────────────
enum SystemState {
  STATE_ENTERING,   // Waiting for user to enter PIN digits
  STATE_ACCEPTED,   // Correct PIN — green LED on
  STATE_REJECTED,   // Wrong PIN — red LED blink (< 3 tries)
  STATE_BLOCKED     // 3 wrong tries — red LED + buzzer
};

SystemState currentState = STATE_ENTERING;

// ── PIN Entry Variables ────────────────────────────────────
int enteredPIN[4]       = {0, 0, 0, 0};  
int incorrectTrials     = 0;             

// ── Button Debounce & Hold Logic ───────────────────────────
const unsigned long DEBOUNCE_DELAY     = 50;   
const unsigned long HOLD_INITIAL_DELAY = 600;  
const unsigned long HOLD_REPEAT_DELAY  = 250;  

bool         btnLastStable[4]       = {HIGH, HIGH, HIGH, HIGH};
unsigned long btnDebounceTimer[4]   = {0, 0, 0, 0};
bool         btnHoldActive[4]       = {false, false, false, false};
unsigned long btnHoldTimer[4]       = {0, 0, 0, 0};
unsigned long btnHoldRepeatTimer[4] = {0, 0, 0, 0};

bool         enterLastStable    = HIGH;
unsigned long enterDebounceTimer = 0;
bool         enterPressed       = false;

// ── Blink Variables ────────────────────────────────────────
unsigned long blinkTimer  = 0;
bool         blinkState  = false;
int          blinkCount  = 0;
const unsigned long BLINK_INTERVAL = 300;  

void setup() {
  for (int i = 0; i < 4; i++) {
    pinMode(BTN_PINS[i], INPUT_PULLUP);
  }
  pinMode(BTN_ENTER, INPUT_PULLUP);

  pinMode(LED_GREEN,   OUTPUT);
  pinMode(LED_RED,     OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED,    LOW);
  noTone(BUZZER_PIN);

  Serial.begin(9600);
  Serial.println("Vault System Ready.");
}

void loop() {
  switch (currentState) {
    case STATE_ENTERING:
      readDigitButtons();
      readEnterButton();

      if (enterPressed) {
        enterPressed = false;
        checkPIN();
      }
      break;

    case STATE_ACCEPTED:
      digitalWrite(LED_GREEN, HIGH);
      digitalWrite(LED_RED,    LOW);
      noTone(BUZZER_PIN);
      break;

    case STATE_REJECTED:
      blinkRedLED();
      break;

    case STATE_BLOCKED:
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_RED,    HIGH);
      tone(BUZZER_PIN, 1000);  
      break;
  }
}

void readDigitButtons() {
  unsigned long now = millis();

  for (int i = 0; i < 4; i++) {
    bool reading = digitalRead(BTN_PINS[i]);

    if (reading != btnLastStable[i]) {
      btnDebounceTimer[i] = now;
      btnLastStable[i]    = reading;
      continue;  
    }

    if ((now - btnDebounceTimer[i]) >= DEBOUNCE_DELAY) {
      if (reading == LOW) {
        if (!btnHoldActive[i] && btnHoldTimer[i] == 0) {
          incrementDigit(i);
          btnHoldTimer[i]       = now;
          btnHoldRepeatTimer[i] = now;
        }
        if (btnHoldTimer[i] > 0 && (now - btnHoldTimer[i] >= HOLD_INITIAL_DELAY)) {
          btnHoldActive[i] = true;
        }
        if (btnHoldActive[i] && (now - btnHoldRepeatTimer[i] >= HOLD_REPEAT_DELAY)) {
          incrementDigit(i);
          btnHoldRepeatTimer[i] = now;
        }
      }

      if (reading == HIGH) {
        btnHoldActive[i] = false;
        btnHoldTimer[i]  = 0;  
      }
    }
  }
}

void incrementDigit(int index) {
  enteredPIN[index] = (enteredPIN[index] + 1) % 10;

  Serial.print("PIN: ");
  for (int j = 3; j >= 0; j--) Serial.print(enteredPIN[j]);
  Serial.println();
}

void readEnterButton() {
  unsigned long now = millis();
  bool reading = digitalRead(BTN_ENTER);

  if (reading != enterLastStable) {
    enterDebounceTimer = now;
    enterLastStable    = reading;
    return;  
  }

  if ((now - enterDebounceTimer) >= DEBOUNCE_DELAY) {
    static bool wasPressed = false;
    if (reading == LOW)  wasPressed = true;
    if (reading == HIGH && wasPressed) {
      enterPressed = true;
      wasPressed   = false;
    }
  }
}

void checkPIN() {
  bool match = true;
  for (int i = 0; i < 4; i++) {
    if (enteredPIN[i] != SECRET_PIN[i]) {
      match = false;
      break;
    }
  }

  if (match) {
    Serial.println("PIN CORRECT — Vault Opened!");
    currentState = STATE_ACCEPTED;
  } else {
    incorrectTrials++;
    Serial.print("Wrong PIN. Trials: ");
    Serial.println(incorrectTrials);

    if (incorrectTrials >= 3) {
      Serial.println("BLOCKED — Alarm triggered!");
      currentState = STATE_BLOCKED;
    } else {
      currentState = STATE_REJECTED;
      blinkTimer   = millis();
      blinkState   = false;
      blinkCount   = 0;
    }
  }
}

const int BLINK_TOTAL = 6;  

void blinkRedLED() {
  unsigned long now = millis();

  if (now - blinkTimer >= BLINK_INTERVAL) {
    blinkTimer = now;
    blinkState = !blinkState;
    digitalWrite(LED_RED, blinkState ? HIGH : LOW);
    blinkCount++;

    if (blinkCount >= BLINK_TOTAL) {
      digitalWrite(LED_RED, LOW);
      blinkCount = 0;
      resetPINEntry();
      currentState = STATE_ENTERING;
    }
  }
}

void resetPINEntry() {
  for (int i = 0; i < 4; i++) enteredPIN[i] = 0;
}
