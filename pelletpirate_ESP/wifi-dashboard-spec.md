# PelletPirate V2 — Local Wi-Fi Dashboard Spec (amended 2026-07-14)

You are implementing a lightweight local web interface for the existing
PelletPirate V2 firmware.

## Hardware constraint

Target hardware:

* **ESP32-DevKitC-32E (plain ESP32)** — the S3 is the V3 target, not this board
* 4 MB flash — **the binding constraint is the app partition, not the chip**:
  after the partition change (step 0) the app partition is **1.5 MB**, with the
  current firmware at ~824 KB. All size budgets are measured against 1.5 MB.
* Existing physical 320 × 480 LCD (HX8357D + LVGL)
* Existing rotary encoder (PCNT driver) and pushbutton
* Existing grill-control firmware (grill_state + actuator state machine)

Flash is limited. Keep the implementation small and avoid unnecessary libraries.

## Prerequisites (complete before the development loop)

0. **Partition table change (DONE first, separately):** custom partitions.csv —
   nvs 24K / phy 4K / factory 1.5 MB / littlefs ~2.4 MB. The LittleFS partition
   is carved now (for future cook logging) even though logging is out of scope,
   so the table only changes once. NVS offset/size are unchanged, so saved
   settings survive.
1. **Alarm engine (small, shared-benefit prerequisite):** probe alarm targets
   are configured and stored today, but nothing fires, displays, or
   acknowledges alarms. Build the minimal alarm engine in grill_state first —
   fire on probe/grill threshold, expose active-alarm state, acknowledge
   function — so both the LCD and the web page mirror the same alarm state.
   The web layer must not own alarm logic.
2. **Wi-Fi provisioning decision (resolved):** SSID/password stored in NVS,
   set initially via menuconfig default; Settings menu gains a WI-FI entry
   showing connection status, RSSI, and the device IP address. The IP display
   doubles as the mDNS fallback (see below).

## Goal

Add a local Wi-Fi web server and a responsive browser page that mirrors the
existing V2 controller interface.

The web page should provide only the current grill and cook controls already
available through the physical LCD and rotary encoder (plus the alarm engine
from prerequisite 1).

Do not redesign the product, add new cooking features, or create a broader IoT
platform.

## Required web interface

The page should display:

* Current grill temperature
* Target grill temperature
* Grill operating mode
* Grill probe status
* Meat probe 1–4 temperature and target
* Fan status
* Auger status
* Igniter status
* Elapsed cook time (ET) and estimated remaining (EST) where available
* Current warning or fault (from the alarm engine)
* Wi-Fi connection status

The page should allow the user to perform only the controls supported by the
existing V2 interface:

* Change target grill temperature
* Set or clear meat-probe target temperatures
* Select an existing cooking mode
* Start ignite / stop ignite
* Start Keep Warm
* Start the existing controlled shutdown sequence
* Acknowledge an active alarm (via the alarm engine)

**Deferred (feature does not exist in the firmware):** user-settable timer.
Do not add a timer endpoint until the LCD has the feature.

Do not create direct manual controls for the fan, auger, or igniter.
(Structurally guaranteed: the actuator task reads only grill_state; the web
layer has no other lever. Keep it that way.)

## Required stack

Use the lightest practical stack compatible with the existing repository:

* ESP32 Wi-Fi station mode (esp_wifi)
* esp_http_server (built into ESP-IDF; native WebSocket support)
* Plain HTML / plain CSS / vanilla JavaScript
* Compact REST endpoints
* One WebSocket connection for live status updates
* mDNS hostname `pelletpirate.local` (official espressif/mdns managed
  component) — **plus the device IP shown on the LCD Settings→WI-FI screen,
  because Android frequently fails to resolve .local names**
* Web assets gzipped and embedded in the app image via EMBED_FILES

Do not use: React, Vue, Angular, Bootstrap, Node.js, npm packages, external
fonts, CDNs, cloud services, cook logging, charts, analytics, HTTPS, OTA,
MQTT, or large image assets.

**Security posture (conscious deferral):** no authentication in v1 — anyone on
the local network can control the grill. Acceptable for demo scope. Backlog
item: optional 4-digit PIN on state-changing endpoints.

Target the complete compressed web interface at less than 100 KB.

## Integration requirement

The physical LCD interface and browser interface must operate through the same
existing controller state and command functions (grill_state under its mutex).

Do not create a second grill-control implementation.

Browser commands must request changes through the existing grill-control state
machine. Loss of Wi-Fi or closure of the browser must not interrupt grill
operation. Wi-Fi init failure must not block boot (LCD-first, Wi-Fi async).

## Suggested API

* `GET  /api/status`
* `POST /api/grill-target`
* `POST /api/probe-target`
* `POST /api/mode`
* `POST /api/keep-warm`
* `POST /api/shutdown`
* `POST /api/alarm-ack`
* `/ws` for live status updates (~1 Hz or on meaningful change)

Keep JSON field names compact without making the code unreadable.

## Development loop

Follow this loop. Do not implement everything in one uncontrolled pass.

### 1. Inspect
Read the repository. Identify build framework, LCD menu structure,
grill-control state, encoder commands, temperature/probe variables, timer
logic, operating modes, shutdown sequence, existing networking code, current
compiled firmware size **and current free heap**. Do not modify files yet.

### 2. Map the existing interface
Concise mapping: each physical screen value / menu command → internal
variable or function → browser display or command. Do not invent controls
that do not already exist.

### 3. Propose the minimum stack
Files added/modified, endpoint list, estimated firmware increase, estimated
asset size, expected remaining flash **and heap**, reusable existing
libraries. Prefer built-in ESP-IDF capabilities.

### 4. Implement the smallest working slice
Wi-Fi connection + HTTP server + one HTML page + `GET /api/status` + display
of current and target grill temperature. Build and report firmware size and
free heap.

### 5. Test and correct
Resolve compiler/linker errors, warnings, oversized libraries, excessive
flash use, excessive dynamic allocation. Do not claim hardware behavior has
passed unless the operator provides the hardware test result. Give exact
hardware test instructions and expected results.

### 6. Add one function at a time
1. Live WebSocket updates
2. Target grill-temperature control
3. Meat-probe display
4. Meat-probe target controls
5. Operating-mode selection
6. Keep Warm
7. Controlled shutdown
8. Fan, auger, igniter status indicators
9. Alarm display (from alarm engine)
10. Alarm acknowledgment
11. mDNS hostname + Settings→WI-FI screen with IP display

After every increment: build; report firmware size, asset size, **and free
heap under load**; test; correct failures; confirm the physical interface
still works; stop for approval before the next major increment.

## Flash and RAM discipline

After every build, report:

* Total firmware image size and increase from previous build
* Percentage of the **1.5 MB** app partition used
* Web asset size
* Largest libraries or sections
* **Free DRAM/heap at runtime with Wi-Fi up and a WebSocket client connected**
  (heap exhaustion is the realistic failure mode on this chip — currently
  ~100 KB DRAM free before Wi-Fi; the Wi-Fi/LwIP stack wants 50–70 KB)

Remove unnecessary code before reducing required functionality.
Do not add OTA, logging, graphs, cook history, cloud, or other V3 features.

## Dashboard design

Mobile-first, high contrast, outdoor-readable, one-hand operable, large touch
controls, no decorative animations, no large images or custom fonts. CSS and
small inline SVG only. Visually match the structure of the V2 LCD interface
(same dark theme, PelletPirate orange accent #FF6B35).

## Completion criteria

* ESP32 connects to local Wi-Fi; failure to connect never blocks the grill.
* `pelletpirate.local` serves the page; the LCD shows the IP as fallback.
* The page accurately mirrors live V2 grill state (~1 s updates).
* Existing grill and cook controls work from the page through the same state
  machine as the LCD and encoder; the physical interface continues to work.
* Wi-Fi loss and browser disconnection do not interrupt grill operation.
* No direct fan/auger/igniter control endpoints exist.
* Web assets < 100 KB compressed; firmware fits the 1.5 MB app partition with
  headroom; free heap with an active client stays above ~40 KB.
* Logging and other V3 features have not been added.

Begin with repository inspection and interface mapping only. Do not modify
code until the current V2 interface functions, internal state variables,
build process, compiled firmware size, and free heap are identified.
