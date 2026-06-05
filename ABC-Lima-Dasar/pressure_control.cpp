/**
 * ============================================================
 *  PRESSURE CONTROL SYSTEM
 *  Based on: CuffnCode System Design Diagram
 * ============================================================
 *  Description:
 *    Automated pneumatic pressure control using ESP32.
 *    Reads 3 pressure sensors and controls a pump +
 *    2 solenoid valves to maintain target pressure range.
 *
 *  Team Members:
 *    - Zaskia Putri Maesa (152024070)
 *    - Depa
 *    - Dapin
 *
 *  Hardware:
 *    - ESP32 Dev Module
 *    - DC Micro Pump (FF-N20 or similar)
 *    - 3x Pressure Sensors (MPXV series / analog output)
 *    - 2x Solenoid Valves (Input + Output)
 *    - 2x Flow Control Valves (passive, no pin needed)
 *
 *  Pressure Thresholds (in raw ADC units, adjust to kPa):
 *    LOW    : pressure < PRESSURE_LOW  --> FILL MODE
 *    NORMAL : PRESSURE_LOW <= p <= PRESSURE_HIGH --> STANDBY
 *    HIGH   : pressure > PRESSURE_HIGH --> RELEASE MODE
 * ============================================================
 */

// ─── PIN DEFINITIONS ────────────────────────────────────────

// Output Pins (actuators)
#define PUMP_PIN          21    // DC Micro Pump
#define INPUT_VALVE_PIN   22    // Solenoid Valve - Input (fills reservoir)
#define OUTPUT_VALVE_PIN  23    // Solenoid Valve - Output (releases air)

// Input Pins (sensors) — ESP32 ADC1 pins (GPIO 32–39)
#define PRESSURE_SENSOR_1 34    // Main pressure sensor
#define PRESSURE_SENSOR_2 35    // Secondary sensor
#define PRESSURE_SENSOR_3 32    // Tertiary sensor

// ─── PRESSURE THRESHOLDS ────────────────────────────────────
// NOTE: These are raw ADC values (0–4095 for 12-bit ESP32 ADC)
// To convert to kPa, use your sensor's datasheet formula.
// Example for MPXV7002: Vout = Vs * (0.057 * P + 0.5)
//   P(kPa) = (ADC_val / 4095.0 * 3.3 - 0.5 * Vs) / (0.057 * Vs)

#define PRESSURE_LOW   500      // Below this → FILL MODE   (~20 kPa approx)
#define PRESSURE_HIGH  2000     // Above this → RELEASE MODE (~80 kPa approx)

// ─── TIMING ─────────────────────────────────────────────────
#define READ_INTERVAL_MS  1000  // Read sensors every 1 second
#define SENSOR_SAMPLES    5     // Average over N samples to reduce noise

// ─── SYSTEM STATE ───────────────────────────────────────────
enum SystemState {
  FILL_MODE,
  STANDBY_MODE,
  RELEASE_MODE
};

SystemState currentState = STANDBY_MODE;

// ─── HELPER: Read averaged ADC value ────────────────────────
int readPressureAvg(int pin) {
  long sum = 0;
  for (int i = 0; i < SENSOR_SAMPLES; i++) {
    sum += analogRead(pin);
    delay(10);
  }
  return (int)(sum / SENSOR_SAMPLES);
}

// ─── HELPER: Set actuator states ────────────────────────────
void setActuators(bool pump, bool inputValve, bool outputValve) {
  digitalWrite(PUMP_PIN,         pump        ? HIGH : LOW);
  digitalWrite(INPUT_VALVE_PIN,  inputValve  ? HIGH : LOW);
  digitalWrite(OUTPUT_VALVE_PIN, outputValve ? HIGH : LOW);
}

// ─── HELPER: Print state label ──────────────────────────────
void printState(SystemState state) {
  switch (state) {
    case FILL_MODE:    Serial.println("Status: FILL MODE    --> Pump ON, Input Valve OPEN");    break;
    case STANDBY_MODE: Serial.println("Status: STANDBY MODE --> All actuators OFF");            break;
    case RELEASE_MODE: Serial.println("Status: RELEASE MODE --> Output Valve OPEN");            break;
  }
}

// ─── SETUP ──────────────────────────────────────────────────
void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);

  // Configure output pins
  pinMode(PUMP_PIN,         OUTPUT);
  pinMode(INPUT_VALVE_PIN,  OUTPUT);
  pinMode(OUTPUT_VALVE_PIN, OUTPUT);

  // Configure ADC resolution (12-bit = 0 to 4095)
  analogReadResolution(12);

  // Safety: start with everything OFF
  setActuators(false, false, false);

  Serial.println("============================================");
  Serial.println("   PRESSURE CONTROL SYSTEM  -- ESP32");
  Serial.println("   Team: Zaskia, Depa, Dapin");
  Serial.println("============================================");
  Serial.println("System initialized. Starting monitoring...");
  Serial.println();
}

// ─── MAIN LOOP ──────────────────────────────────────────────
void loop() {
  // 1. Read pressure from all three sensors (averaged)
  int p1 = readPressureAvg(PRESSURE_SENSOR_1);
  int p2 = readPressureAvg(PRESSURE_SENSOR_2);
  int p3 = readPressureAvg(PRESSURE_SENSOR_3);

  // 2. Use sensor 1 as primary; others for monitoring / redundancy
  int pressure = p1;

  // 3. Determine system state based on pressure
  SystemState newState;

  if (pressure < PRESSURE_LOW) {
    newState = FILL_MODE;
  } else if (pressure > PRESSURE_HIGH) {
    newState = RELEASE_MODE;
  } else {
    newState = STANDBY_MODE;
  }

  // 4. Apply actuator control based on state
  switch (newState) {
    case FILL_MODE:
      // Reservoir is low → pump air in
      setActuators(true, true, false);
      break;

    case RELEASE_MODE:
      // Pressure too high → open exhaust valve
      setActuators(false, false, true);
      break;

    case STANDBY_MODE:
    default:
      // Pressure is in target range → hold
      setActuators(false, false, false);
      break;
  }

  // 5. Update state tracking
  currentState = newState;

  // 6. Print readings to Serial Monitor
  Serial.println("--------------------------------------------");
  Serial.print("Sensor 1 (Primary): "); Serial.print(p1);
  if      (p1 < PRESSURE_LOW)  Serial.println("  [LOW]");
  else if (p1 > PRESSURE_HIGH) Serial.println("  [HIGH]");
  else                          Serial.println("  [NORMAL]");

  Serial.print("Sensor 2           : "); Serial.println(p2);
  Serial.print("Sensor 3           : "); Serial.println(p3);
  Serial.println();
  printState(currentState);
  Serial.print("Pump: ");         Serial.print(digitalRead(PUMP_PIN)         ? "ON  " : "OFF ");
  Serial.print("| Input Valve: "); Serial.print(digitalRead(INPUT_VALVE_PIN)  ? "OPEN   " : "CLOSED ");
  Serial.print("| Output Valve: ");Serial.println(digitalRead(OUTPUT_VALVE_PIN)? "OPEN" : "CLOSED");
  Serial.println("--------------------------------------------");
  Serial.println();

  // 7. Wait before next reading
  delay(READ_INTERVAL_MS);
}
