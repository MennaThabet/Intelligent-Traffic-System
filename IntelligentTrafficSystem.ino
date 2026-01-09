/*
  IntelligentTrafficSystem.ino

  Arduino implementation of the prototype Intelligent Traffic System
  - Two roads (A and B)
  - Each road: 3 LEDs (RED, YELLOW, GREEN)
  - Two ultrasonic sensors (HC-SR04) to count passing vehicles per 120s cycle
  - Two emergency buttons (one per road) to immediately give green to an emergency vehicle

  Behavior:
  - System continuously counts vehicles during each 120-second cycle.
  - After a cycle finishes, it computes green time for Road A using:
      t(x) = (x / (x + y + 0.01)) * 60 + 30
    then clamps t(x) to [30, 90]. Road B gets (120 - t(x)).
  - If an emergency button is pressed at any time, the system immediately gives green to that road
    for EMERGENCY_GREEN_TIME and then restarts a fresh counting cycle (counts reset).

  Notes:
  - Used non-blocking timing with millis() so buttons and sensors remain responsive.
  - Ultrasonic counting uses a simple state-machine per sensor to detect "a vehicle passed" and
    avoid multiple counts for the same vehicle.
  - Serial debug prints are included for easier testing.

  Wiring (example):
    // Road A LEDs
    pin 2  -> RED_A
    pin 3  -> YELLOW_A
    pin 4  -> GREEN_A

    // Road B LEDs
    pin 5  -> RED_B
    pin 6  -> YELLOW_B
    pin 7  -> GREEN_B

    // Ultrasonic A (HC-SR04)
    pin 8  -> TRIG_A
    pin 9  -> ECHO_A

    // Ultrasonic B (HC-SR04)
    pin 10 -> TRIG_B
    pin 11 -> ECHO_B

    // Emergency buttons
    pin 12 -> EMER_BUTTON_A  (use INPUT_PULLUP; button connects to GND)
    pin 13 -> EMER_BUTTON_B  (use INPUT_PULLUP; button connects to GND)
*/

// *** Configuration / Pins ***
// LED pins
const uint8_t RED_A = 2;
const uint8_t YELLOW_A = 3;
const uint8_t GREEN_A = 4;

const uint8_t RED_B = 5;
const uint8_t YELLOW_B = 6;
const uint8_t GREEN_B = 7;

// Ultrasonic sensor pins (HC-SR04)
const uint8_t TRIG_A = 8;
const uint8_t ECHO_A = 9;

const uint8_t TRIG_B = 10;
const uint8_t ECHO_B = 11;

// Emergency buttons (active LOW with INPUT_PULLUP)
const uint8_t EMER_BUTTON_A = 12;
const uint8_t EMER_BUTTON_B = 13;

// Timing constants
const unsigned long CYCLE_TOTAL_MS = 120UL * 1000UL; // 120 seconds total cycle
const unsigned long SENSOR_POLL_MS = 60;            // how often to poll ultrasonic (ms)
const unsigned long EMERGENCY_GREEN_MS = 10UL * 1000UL; // emergency green time (10s)

// Ultrasonic / counting tuning
const unsigned int DIST_THRESHOLD_CM = 60;   // distance below which we consider "presence"
const unsigned long PRESENCE_DEBOUNCE_MS = 80; // how long presence must be stable to consider it present
const unsigned long ABSENCE_DEBOUNCE_MS = 120; // how long absence must be stable to count a pass

// *** Internal state variables ***
unsigned long lastSensorPoll = 0;
unsigned long cycleStartTime = 0; // millis at start of current 120s cycle

// counters for this cycle
volatile unsigned int countA = 0;
volatile unsigned int countB = 0;

// Sensor presence state machines
struct SensorState {
  bool present = false;             // currently considered presence
  unsigned long lastChange = 0;     // last time presence changed
  bool waitingForAbsence = false;   // true when we've seen a presence and are waiting for vehicle to leave to increment count
};

SensorState sA, sB;

// Road light states
enum LightPhase {PHASE_RED, PHASE_YELLOW, PHASE_GREEN};

// Current traffic execution state machine
enum SystemState {STATE_COUNTING, STATE_EXECUTING_TIMES, STATE_EMERGENCY};
SystemState sysState = STATE_COUNTING;

// Execution-phase variables
unsigned long execPhaseStart = 0; // when current green started
unsigned long greenDurationA = 60 * 1000UL; // milliseconds
unsigned long greenDurationB = 60 * 1000UL; // milliseconds
bool executingAfirst = true; // which road has green first in the execution phase

// Emergency info
bool emerActive = false;
unsigned long emerStart = 0;
uint8_t emerRoad = 0; // 0=none, 1=A, 2=B


// --- Helper functions ---

// safe digital write wrapper
inline void setLed(uint8_t pin, bool on) {
  digitalWrite(pin, on ? HIGH : LOW);
}

// Set whole road lights in a simple 3-color way
void setRoadLightsA(LightPhase p) {
  setLed(RED_A, p == PHASE_RED);
  setLed(YELLOW_A, p == PHASE_YELLOW);
  setLed(GREEN_A, p == PHASE_GREEN);
}

void setRoadLightsB(LightPhase p) {
  setLed(RED_B, p == PHASE_RED);
  setLed(YELLOW_B, p == PHASE_YELLOW);
  setLed(GREEN_B, p == PHASE_GREEN);
}

// Measure distance from HC-SR04 (in cm). Returns -1 on timeout.
long measureDistanceCM(uint8_t trigPin, uint8_t echoPin) {
  // trigger pulse
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // measure echo
  long duration = pulseIn(echoPin, HIGH, 30000); // 30 ms timeout (~5 meters)
  if (duration == 0) return -1;

  long distanceCm = duration / 58; // sound speed conversion
  return distanceCm;
}

// Update sensor state-machine and increment counters when a pass is detected
void updateSensor(SensorState &s, uint8_t trigPin, uint8_t echoPin, volatile unsigned int &counter, const char *name) {
  long d = measureDistanceCM(trigPin, echoPin);
  unsigned long now = millis();
  bool isPresent = (d > 0 && d <= DIST_THRESHOLD_CM);

  if (isPresent != s.present) {
    // candidate change
    if (now - s.lastChange >= (isPresent ? PRESENCE_DEBOUNCE_MS : ABSENCE_DEBOUNCE_MS)) {
      // accept change
      s.present = isPresent;
      s.lastChange = now;

      if (isPresent) {
        // a vehicle arrived. mark waitingForAbsence so we count when it leaves
        s.waitingForAbsence = true;
        // debug
        Serial.print(name); Serial.println(" presence ON");
      } else {
        // vehicle left
        if (s.waitingForAbsence) {
          // count a pass
          counter++;
          s.waitingForAbsence = false;
          Serial.print(name); Serial.print(" PASS counted. total=");
          Serial.println(counter);
        }
        Serial.print(name); Serial.println(" presence OFF");
      }
    }
  } else {
    // stable state; update lastChange to keep debouncing timing accurate
    // no-op
  }
}

// Compute t(x) in milliseconds according to formula: t(x) = (x/(x+y+0.01))*60 + 30  (seconds)
unsigned long computeGreenMs(unsigned int x, unsigned int y) {
  // use floats to match formula precisely
  float denom = (float)x + (float)y + 0.01f;
  float ratio = (denom > 0.0f) ? ((float)x / denom) : 0.5f;
  float tsec = ratio * 60.0f + 30.0f;
  if (tsec < 30.0f) tsec = 30.0f;
  if (tsec > 90.0f) tsec = 90.0f;
  return (unsigned long)(tsec * 1000.0f);
}

// Start an execution phase (after counting finished) where we run the green durations
void startExecutionPhase() {
  // calculate green durations using counts collected in the last counting cycle
  greenDurationA = computeGreenMs(countA, countB);
  greenDurationB = CYCLE_TOTAL_MS - greenDurationA;

  // decide which road goes first: the one with more cars gets green first
  executingAfirst = (countA >= countB);

  Serial.println("--- Execution Phase Starting ---");
  Serial.print("countA="); Serial.print(countA);
  Serial.print("  countB="); Serial.println(countB);
  Serial.print("greenA(ms)="); Serial.print(greenDurationA);
  Serial.print("  greenB(ms)="); Serial.println(greenDurationB);

  // change state and record start
  sysState = STATE_EXECUTING_TIMES;
  execPhaseStart = millis();

  // start by giving green to whichever road is first
  if (executingAfirst) {
    setRoadLightsA(PHASE_GREEN);
    setRoadLightsB(PHASE_RED);
  } else {
    setRoadLightsA(PHASE_RED);
    setRoadLightsB(PHASE_GREEN);
  }
}

// Force emergency: immediately grant green to selected road and reset counting cycle after emergency finishes
void triggerEmergency(uint8_t road) {
  emerActive = true;
  emerStart = millis();
  emerRoad = road; // 1=A, 2=B
  sysState = STATE_EMERGENCY;

  Serial.print("*** EMERGENCY on road "); Serial.print((road==1)?"A":"B"); Serial.println(" ***");

  if (road == 1) {
    setRoadLightsA(PHASE_GREEN);
    setRoadLightsB(PHASE_RED);
  } else {
    setRoadLightsA(PHASE_RED);
    setRoadLightsB(PHASE_GREEN);
  }
}

// Reset counting cycle and related counters
void resetCountingCycle() {
  countA = 0; countB = 0;
  sA = SensorState(); sB = SensorState(); // reset sensor states
  cycleStartTime = millis();
  sysState = STATE_COUNTING;
  lastSensorPoll = 0;
  Serial.println("--- New Counting Cycle Started ---");
}


// --- Arduino standard functions ---

void setup() {
  // LEDs
  pinMode(RED_A, OUTPUT);
  pinMode(YELLOW_A, OUTPUT);
  pinMode(GREEN_A, OUTPUT);

  pinMode(RED_B, OUTPUT);
  pinMode(YELLOW_B, OUTPUT);
  pinMode(GREEN_B, OUTPUT);

  // Ultrasonic pins
  pinMode(TRIG_A, OUTPUT);
  pinMode(ECHO_A, INPUT);

  pinMode(TRIG_B, OUTPUT);
  pinMode(ECHO_B, INPUT);

  // Emergency buttons (active LOW)
  pinMode(EMER_BUTTON_A, INPUT_PULLUP);
  pinMode(EMER_BUTTON_B, INPUT_PULLUP);

  Serial.begin(115200);
  delay(100);

  // initialize to a safe state: both roads red
  setRoadLightsA(PHASE_RED);
  setRoadLightsB(PHASE_RED);

  resetCountingCycle();
}

void loop() {
  unsigned long now = millis();

  // Check emergency buttons at high priority (non-blocking)
  // Buttons are wired to GND, so pressed = LOW
  if (digitalRead(EMER_BUTTON_A) == LOW) {
    // small software debounce
    delay(10);
    if (digitalRead(EMER_BUTTON_A) == LOW) {
      triggerEmergency(1);
    }
  }
  if (digitalRead(EMER_BUTTON_B) == LOW) {
    delay(10);
    if (digitalRead(EMER_BUTTON_B) == LOW) {
      triggerEmergency(2);
    }
  }

  // State machine
  switch (sysState) {

    case STATE_COUNTING:
      // Poll sensors periodically to update counts
      if (now - lastSensorPoll >= SENSOR_POLL_MS) {
        lastSensorPoll = now;
        updateSensor(sA, TRIG_A, ECHO_A, countA, "SensorA");
        updateSensor(sB, TRIG_B, ECHO_B, countB, "SensorB");
      }

      // If we've reached the total cycle time, go to execute times
      if (now - cycleStartTime >= CYCLE_TOTAL_MS) {
        startExecutionPhase();
      }
      break;

    case STATE_EXECUTING_TIMES: {
      unsigned long elapsed = now - execPhaseStart;

      if (executingAfirst) {
        if (elapsed < greenDurationA) {
          // A green, B red (already set)
        } else if (elapsed < greenDurationA + 3000UL) {
          // transition: A yellow for 3 seconds
          setRoadLightsA(PHASE_YELLOW);
          setRoadLightsB(PHASE_RED);
        } else if (elapsed < greenDurationA + 3000UL + greenDurationB) {
          // B green
          setRoadLightsA(PHASE_RED);
          setRoadLightsB(PHASE_GREEN);
        } else if (elapsed < greenDurationA + 3000UL + greenDurationB + 3000UL) {
          // B yellow
          setRoadLightsA(PHASE_RED);
          setRoadLightsB(PHASE_YELLOW);
        } else {
          // End of execution: start a fresh counting cycle
          resetCountingCycle();
        }
      } else {
        // B goes first
        if (elapsed < greenDurationB) {
          // B green
        } else if (elapsed < greenDurationB + 3000UL) {
          // B yellow
          setRoadLightsA(PHASE_RED);
          setRoadLightsB(PHASE_YELLOW);
        } else if (elapsed < greenDurationB + 3000UL + greenDurationA) {
          // A green
          setRoadLightsA(PHASE_GREEN);
          setRoadLightsB(PHASE_RED);
        } else if (elapsed < greenDurationB + 3000UL + greenDurationA + 3000UL) {
          // A yellow
          setRoadLightsA(PHASE_YELLOW);
          setRoadLightsB(PHASE_RED);
        } else {
          // End execution
          resetCountingCycle();
        }
      }

      break;
    }

    case STATE_EMERGENCY:
      // Keep emergency green on the requested road for EMERGENCY_GREEN_MS
      if (now - emerStart >= EMERGENCY_GREEN_MS) {
        // end emergency, reset and start fresh counting cycle
        emerActive = false;
        emerRoad = 0;
        Serial.println("*** Emergency ended, restarting counting cycle ***");
        resetCountingCycle();
      }
      break;
  }

  // small background yield
  delay(1);
}
