# Intelligent-Traffic-System
An Arduino-based intelligent traffic management system that dynamically allocates green-light time to reduce congestionand road injuries by real-time vehicle counting and give immediate priority to emergency vehicles.  
This repository contains the prototype Arduino sketch, wiring notes, and documentation.

---

## Table of Contents
- [Project idea & formula](#project-idea--formula)
- [Features](#features)
- [Hardware Used](#hardware-used)
- [How to run](#how-to-run)
- [Future improvements (push-button alternatives)](#future-improvements-push-button-alternatives)
- [Awards](#awards)

---

## Project idea & formula

The system measures the number of cars on two intersecting roads (A and B) during each cycle, and uses those counts to compute how long the green light should stay open for each road. After each 120-second cycle the counts are re-evaluated.

The formula used to compute the green time for Road A (in seconds) is:
**t(x) = ( x / (x + y + 0.01) ) * 60 + 30** 

Where:
- `t(x)` = green time for Road A (seconds)  
- `x` = number of cars detected on Road A during the counting window  
- `y` = number of cars detected on Road B during the same window  
- `+0.01` prevents division-by-zero when both counts are zero
Constraints:
- Minimum `t(x)` = **30 seconds**  
- Maximum `t(x)` = **90 seconds**  
- Total cycle = **120 seconds** (so Road B receives `120 - t(x)` seconds)

This keeps a guaranteed minimum for each direction while letting heavier traffic get more green time.

---

## Features

- Vehicle counting using two ultrasonic sensors (one per road)
- Dynamic green-time allocation using the project formula
- Emergency preemption (prototype uses roadside push-buttons to simulate emergency priority)
- Non-blocking Arduino code (millis-based), with a small state machine for stability
- Low-cost prototype suitable for demonstration and further research

---

## Hardware Used

- Arduino UNO
- 2 × HC-SR04 Ultrasonic sensors
- LEDs for traffic lights (Red / Yellow / Green) × 2 roads
- 2 × Push buttons (emergency override) — wired with `INPUT_PULLUP` (active LOW)
- Breadboard, jumper wires, resistors, power supply

---

## How to run

1. Open `IntelligentTrafficSystem.ino` in the Arduino IDE.  
2. Check and adjust pin assignments at the top of the sketch if your wiring differs.  
3. Connect hardware according to the pin mapping in the top comments of the sketch.  
4. Upload to Arduino UNO.  
5. Open Serial Monitor at `115200` baud to view debug messages and counts.  
6. For testing emergency behavior, press the push button for the road you want to give priority.

---

## Future improvements — **(Push-button alternatives & considerations)**

The push button in the prototype is a **manual simulator** for emergency-priority logic. It is fine for demos, but **not recommended for real-world deployment**.
Below are safer, automatic alternatives with short pros/cons:

1. **RFID / NFC tag on emergency vehicle + reader at junction**
   - *How:* Each ambulance has a secure RFID tag; junction reader detects approaching tag and grants green.
   - *Pros:* Fast, low-power, reliable at short range.
   - *Cons:* Requires equipping vehicles & infrastructure; security controls needed to avoid spoofing.

2. **Siren detection (microphone + DSP / ML)**
   - *How:* Junction audio sensor detects siren pattern and triggers preemption.
   - *Pros:* No vehicle equipment needed.
   - *Cons:* Prone to false positives in noisy environments; requires robust signal processing and testing.

3. **Camera-based emergency vehicle recognition (CV)**
   - *How:* Use camera + ML model to detect vehicle type (ambulance / fire truck) approaching junction.
   - *Pros:* Visual confirmation; can integrate with siren detection.
   - *Cons:* Higher cost, privacy concerns, needs good lighting and compute.

**Recommendation:** For a realistic but low-cost next step, start with **RFID on vehicles** combined with local heuristics to reduce false positives.

---

## Awards

I participated in the **Intel International Science and Engineering Fair (ISEF)** within the **Robotics and Intelligent Machines (ROBO)** category.

The project focused on addressing real-world challenges faced in Egypt by proposing a practical and low-cost intelligent traffic solution using robotics and embedded systems.

As a result of this work, I achieved:
- 🥇 **1st Place — ISEF Qena Fair 2020**
- 🏆 **Award Shield — ISEF Qena Fair 2020**

<p float="left">
  <img src="./1st%20place%20in%20ISEF%20Qena%20Fair%202020.jpeg" width="45%" />
  <img src="./Wining%20a%20shield%20in%20ISEF%20Qena%20Fair%202020.jpeg" width="45%" />
</p>

**Figure:** Left — *1st Place Certificate (ISEF Qena Fair 2020)* | Right — *Award Shield (ISEF Qena Fair 2020)*


