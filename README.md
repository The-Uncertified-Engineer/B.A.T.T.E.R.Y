# B.A.T.T.E.R.Y.

### **B**asically **A** **T**oy **T**hat **E**mbodies **R**echargeable **Y**et-again-batteries

> A battery-powered device whose sole purpose in life is to judge the worthiness of *other* batteries. A Russian nesting doll of batteries. A tiny blinking philosopher sitting on your desk, asking the only question that matters: *are you, too, full of energy — or are you empty inside?*

### "Its a battery, powered by a Battery, made to test other Batteries"
---

## Watch the Demo

[![B.A.T.T.E.R.Y. — Demo Video](https://img.youtube.com/vi/byK34mqk1mE/0.jpg)](https://youtu.be/byK34mqk1mE?si=hTBztpc-X5ed7hkX)

**[▶ Watch on YouTube](https://youtu.be/byK34mqk1mE?si=hTBztpc-X5ed7hkX)**

---

## Read the Full Build Article

The complete step-by-step write-up — including the backstory, the wiring guide, the code walkthrough, and the testing logs — is available as a PDF:

**[📄 Read the Article]()**

---


## What It Does

Drop a single AA battery into the external holder. The ESP32 reads the voltage through a calibrated ADC pipeline, smooths it with a moving-average filter, and renders status on a 128×64 OLED that represents the battery's health:

| Voltage Range | Status |
|---|---|
| `< 0.2V` | No Battery |
| `0.2V – 1.05V` | Dead / Replace |
| `1.05V – 1.35V` | Low Power |
| `1.35V – 1.6V+` | Good / Healthy |

The UI includes a 28-frame charging-battery pixel-art animation rendered via the U8g2 library.

---

## Hardware

### Bill of Materials

| Component | Purpose | Est. Cost |
|---|---|---|
| ESP32 dev board (any variant) | The brain — built-in 12-bit ADC, 3.3V logic | $4–8 |
| 0.96" I2C OLED (SSD1306, 128×64) | The face | $3–5 |
| 9V battery + snap clip | Internal system power (the battery that powers the battery tester) | $2–4 |
| AA battery holder (single cell) | External testing slot | $0.75 |
| 0.1µF ceramic capacitor | **Usefull** — kills ADC noise on fresh batteries (I made a working version without it, that one worked but not trustable sometimes, the readings were flickering) | $0.10 |
| Jumper wires | Breadboard assembly | $1 |
| Cardboard + colored paper | The housing (yes, really) | free |

**Total: ~$15**

### Wiring

| Signal | From | To (ESP32) | Notes |
|---|---|---|---|
| OLED SDA | OLED module | GPIO 21 | I2C data |
| OLED SCL | OLED module | GPIO 22 | I2C clock |
| OLED VCC | OLED module | 3.3V pin | Do NOT use 5V |
| OLED GND | OLED module | GND | Common ground |
| AA holder + | AA holder (red wire) | GPIO 34 | Through 0.1µF cap to GND |
| AA holder − | AA holder (black wire) | GND | **CRITICAL — common ground** |
| 9V clip + | 9V snap (red) | VIN | Raw system power, 7–12V |
| 9V clip − | 9V snap (black) | GND | Common ground |
| 0.1µF cap | Between GPIO 34 and GND | — | Decouples ADC sampling spikes |

### The One Rule You Cannot Break

**The internal 9V system battery, the external AA being tested, and the ESP32 itself must all share a common ground.** Voltage is always measured as a difference between two points. If the AA's negative terminal isn't tied to the ESP32's ground, the reading will be garbage.

---

## Software

### Architecture

```
┌──────────────────────────────────────────────────────────┐
│                    ESP32 (BATTERY.ino)                             │
│                                                                    │
│  ┌─────────────────┐     ┌──────────────────────────────┐ │
│  │    ADC Pipeline    │───►│  BatteryData struct                │ │
│  │                    │     │  • voltage (float)                 │ │
│  │  • esp_adc_cal     │     │  • percent (int)                   │ │
│  │  • eFuse Vref      │     │  • status (enum)                   │ │
│  │  • 32x oversamp    │     │  • voltageStr, percentStr,         │ │
│  │  • min/max reject  │     │    statusStr (pre-formatted)       │ │
│  │  • MA filter(10)   │     └──────────────┬───────────────┘ │
│  └─────────────────┘                       │                    │
│                                               ▼                    │
│                          ┌──────────────────────────┐         │
│                          │  U8g2 Render Loop             │         │
│                          │  (page-buffer mode)           │         │
│                          │                               |         │
│                          │  • Lopaka UI layout           │         │
│                          │  • 28-frame charging          │         │
│                          │    battery animation          │         │
│                          │  • Real-time data overlay     │         │
│                          └──────────────────────────┘         │
└──────────────────────────────────────────────────────────┘
```

### Key Engineering Choices

1. **`esp_adc_cal` with eFuse Vref** — Reads the factory-burned calibration data on your specific ESP32 chip. Falls back to a user-tunable `VOLTAGE_CALIBRATION` multiplier if eFuse is empty (rare on legit Espressif silicon, common on clones).

2. **32× oversampling with min/max rejection** — Takes 32 ADC readings, throws out the highest and lowest, averages the remaining 30. Kills transient spikes dead.

3. **10-sample moving average** — On top of the batched reads, smooths the output so the display doesn't jitter.

4. **U8g2 page-buffer mode** — Memory-efficient render loop compatible with Lopaka-generated UI code.

5. **Frame-stable animation** — The animation frame index advances *before* `firstPage()`, so it stays constant across all page iterations of a single render (prevents glitching).

### Required Libraries

Install via Arduino Library Manager:

- **U8g2** — by Oliver Kraus (display driver)

The ESP32 ADC calibration library (`esp_adc_cal`) is built into the ESP32 Arduino core — no install needed.

### File Structure

```
BATTERY/
├── BATTERY.ino                          # Main sketch (ADC, calibration, render loop)
├── charging_battery_64_64_28f.h         # Animation (28 frames, 64x64 XBM)
└── README.md                            # This file
```

---

## Getting Started

### 1. Clone the Repo

```bash
git clone https://github.com/The-Uncertified-Engineer/BATTERY.git
cd BATTERY
```

### 2. Open in Arduino IDE

- Open `BATTERY.ino` in the Arduino IDE
- Select your ESP32 board (e.g., `ESP32 Dev Module`)
- Install the **U8g2** library via Tools → Manage Libraries

### 3. Wire It Up

Follow the wiring table above. Don't forget the 0.1µF capacitor on GPIO 34.

### 4. Upload

- Connect via USB-A
- Click Upload
- Open Serial Monitor at **115200 baud** to see ADC calibration status

### 5. Power It Portably

Disconnect USB. Connect the 9V battery to the snap clip. The ESP32 boots from VIN, the OLED comes alive, and you're ready to judge batteries.

---

## Calibration & Troubleshooting

### Verifying ADC Calibration

On boot, open the Serial Monitor at 115200 baud. You'll see one of:

```
ADC: calibrated using eFuse Vref          ← good, factory calibration present
ADC: calibrated using eFuse Two-Point     ← excellent, dual-point calibration present
ADC: no eFuse calibration - using default Vref.  ← cheap clone board, see below
```

### If Readings Are Still Off

If a fresh battery still reads weirdly high after building this:

1. **Did you actually solder the 0.1µF cap?** Check continuity with a multimeter.
2. **Does your ESP32 have eFuse calibration?** Some cheap clone boards don't. If the boot message says "default Vref", tune the `VOLTAGE_CALIBRATION` constant in the code against a known-good DMM reading:

   ```cpp
   // If DMM says 1.55V but display shows 1.62V:
   #define VOLTAGE_CALIBRATION  (1.55f / 1.62f)  // ≈ 0.957
   ```

### Status String Customization

Edit the `strncpy` calls in `updateBatteryData()` to change the status words (currently `Good` / `Low` / `Replace` / `No Battery`).

---

## How It Tests

| Test Case | Expected Voltage | Expected % | Expected Status |
|---|---|---|---|
| Empty slot | ~0.00V | 0% | No Battery |
| Dead AA (corroded, old) | 0.7–1.0V | 0% | Replace |
| Mid-life AA (used remote) | 1.2–1.3V | 30–50% | Low |
| Fresh AA (new pack) | 1.55–1.62V | 85–100% | Good |

---

## Future Improvements

- [ ] Second AA slot for head-to-head battery racing
- [ ] Piezo buzzer that plays a sad trombone when a battery tests as "Dead"
- [ ] Internal LiPo + charging circuit (replace the 9V)
- [ ] Bluetooth Low Energy logging to phone
- [ ] More information about battery and more supported battery types

---

## License

MIT — go forth and judge batteries.

Built with sarcasm, solder, and a 0.1µF capacitor.
