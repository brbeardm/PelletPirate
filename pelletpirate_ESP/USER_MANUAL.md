# PelletPirate V2 Controller — User's Manual

**Firmware:** esp32-v2 branch, build `9be74ae` or later
**Applies to:** PelletPirate V2 control board (ESP32-DevKitC), 3.5" LCD + rotary encoder, web dashboard
**Document revision:** 1.0 — July 2026

---

## Contents

1. [Important Safety Information](#1-important-safety-information)
2. [Meet Your Controller](#2-meet-your-controller)
3. [Quick Start — Your First Cook](#3-quick-start--your-first-cook)
4. [Temperature Probes](#4-temperature-probes)
5. [The Controls](#5-the-controls)
6. [Cook Modes Explained](#6-cook-modes-explained)
7. [The Dashboards](#7-the-dashboards)
8. [Alarms and Alerts](#8-alarms-and-alerts)
9. [Cook Programs (Automated Cooking)](#9-cook-programs-automated-cooking)
10. [Cook Profiles (Saved Setups)](#10-cook-profiles-saved-setups)
11. [Cook Logs and Graphs](#11-cook-logs-and-graphs)
12. [Settings Reference](#12-settings-reference)
13. [Phone Alerts](#13-phone-alerts)
14. [Wi-Fi Setup](#14-wi-fi-setup)
15. [Firmware Updates](#15-firmware-updates)
16. [Built-In Safety Systems](#16-built-in-safety-systems)
17. [Power Outages](#17-power-outages)
18. [Troubleshooting](#18-troubleshooting)
19. [Specifications](#19-specifications)

---

## 1. Important Safety Information

Read this section before operating the grill.

- **This controller operates a live fire appliance.** Never leave a burning
  grill unattended for extended periods without phone alerts configured
  (Section 13) and never rely solely on any automatic system to prevent a
  fire. The built-in safety systems (Section 16) reduce risk; they do not
  eliminate it.
- **Mains voltage is present** inside the controller enclosure (fan, auger,
  and igniter circuits). Never open the enclosure while it is plugged in.
- **The igniter element gets extremely hot.** The controller automatically
  disables it above 115 °F grill temperature, but treat the firepot as hot
  at all times during and after a cook.
- **Shutdown mode exists for a reason.** Always end a cook with SHUTDOWN
  rather than pulling power. Shutdown runs the fan for 15 minutes with no
  new fuel, burning off pellets remaining in the firepot. Cutting power
  with a live fire leaves smoldering pellets without airflow.
- **Use PT100 probes only.** See Section 4 — a PT1000 probe physically
  cannot be read by this controller and will show as a fault.
- **Empty hopper hazard:** running the hopper dry mid-cook causes a
  flame-out. The controller detects and safely shuts down (Section 16),
  but check pellet levels before overnight cooks.

---

## 2. Meet Your Controller

The PelletPirate V2 is a pellet grill controller with two complete user
interfaces:

- **The panel:** a 3.5" color LCD with a click-and-turn rotary encoder.
  Everything needed to run a cook is available here, with no phone
  required — designed for greasy-hands operation at the grill.
- **The web dashboard:** every phone, tablet, or computer on your Wi-Fi
  network can open the controller's web page. The dashboard mirrors the
  panel completely and adds conveniences that suit a screen: tap-to-type
  temperatures, live graphs, cook log downloads, program authoring, and
  firmware updates. Most owners find the web dashboard becomes their
  daily driver, with the panel as the at-the-grill fallback.

The two interfaces are always in sync: a target changed on the phone
appears on the LCD within a second, and vice versa.

**Finding the web dashboard:** once Wi-Fi is configured (Section 14),
browse to the controller's IP address, shown on the panel under
**Settings → WI-FI** (e.g., `http://192.168.1.178`). On Apple devices,
`http://pelletpirate.local` usually works too.

### What's plugged into it

| Connection | Purpose |
|---|---|
| Probe jacks P1–P5 (3.5 mm) | Five PT100 temperature probes: one for the pit (grill) temperature, four for meat |
| Fan | Combustion air blower |
| Auger | Pellet feed motor |
| Igniter | Hot-rod ignition element |
| Mains inlet | 120 V AC power |

One probe jack is designated the **grill probe** — it measures pit
temperature and drives all temperature control and safety logic. By
default this is jack **P1**, but it is assignable (Section 12.4). The
remaining four jacks are **meat probes 1–4**.

---

## 3. Quick Start — Your First Cook

1. **Fill the hopper** with pellets. Check it again if the last cook was
   long.
2. **Plug in the grill probe** (jack P1 by default) and clip it to the
   grate. Plug meat probes into any of the other jacks and insert them
   into the meat.
3. **Set up your probes** (either UI): give each meat probe its meat
   type, a **goal** temperature (when it's done), and optionally an
   **alarm** temperature with an action (e.g., "Wrap at 165").
4. **Set the grill target** (e.g., 225 °F).
5. **Start:** on the panel, choose **Start Now – Ignite** from the main
   menu and press **Start Ignite**. On the web, tap **START · IGNITE**.
   The fan starts, then the auger, then the igniter (staggered start).
6. **Ignition happens automatically.** When the pit passes 115 °F the
   controller declares the fire lit and switches itself to **Cook**
   mode, holding your target with its PID control loop.
7. **Cook.** Watch from the couch. Change targets any time. Alarms and
   goals will tell you when to act (and buzz your phone if Section 13 is
   set up).
8. **Finish:** when the meat is done, select **SHUTDOWN**. The auger
   stops, the fan runs 15 minutes to burn off remaining pellets, and the
   grill turns itself Off.

> **Tip:** For the classic low-and-slow flavor sequence, start in
> **Super Smoke** while the meat is cold (it absorbs the most smoke
> below ~140 °F internal), then switch to **Cook** for the remainder.
> Or let a **Cook Program** (Section 9) run the whole sequence for you.

---

## 4. Temperature Probes

### 4.1 Probe type — PT100 only

The controller is built for **3-wire PT100 RTD probes** (100 Ω at
32 °F). This is a hardware property of the measurement circuit and
cannot be changed in software.

> ⚠️ **PT1000 probes do not work.** They look identical and fit the
> jack, but read ~1,100 Ω at room temperature — beyond the circuit's
> measurement ceiling. A PT1000 shows as a fault (0 °F / "--") or, worse,
> occasionally produces a plausible-looking wrong temperature during
> partial contact. **Check the listing when buying probes: it must say
> PT100.** Verify with a multimeter: a PT100 reads **108–110 Ω** across
> tip-to-sleeve at room temperature; a PT1000 reads ~1,080–1,120 Ω.

### 4.2 Wiring a probe plug (3.5 mm TRS)

3-wire PT100 probes have two wires of one color (commonly red) and one
of another (commonly white). The two same-colored wires connect to the
same end of the sensing element.

| Plug contact | Wire |
|---|---|
| **Tip** | Red #1 (either red) |
| **Ring** | Red #2 (the other red) |
| **Sleeve** (barrel/body lug) | White |

The two reds are interchangeable. **Verify before soldering:** the two
same-end wires read ~0–1 Ω between each other; either of them to the
third wire reads ~108–110 Ω at room temperature. Whichever pair reads
near zero goes to tip + ring.

### 4.3 Probe care

- **Moisture is the #1 probe killer.** After long cooks, grease and
  condensation can wick into the probe sheath or the jack, causing
  erratic readings or fault (0 °F). A wet probe can often be recovered
  by drying it in a kitchen oven at ~250 °F for 2 hours.
- Keep the jack area clean; a drop of isopropyl alcohol on a plug
  cleans oxidation and grease.
- Seat plugs fully — a half-inserted plug reads as a short.
- Avoid hot-plugging probes mid-cook when practical; if you must, a
  brief FAULT/OK blip in the log is normal.
- Don't let probe cables touch the firepot or slide under the grease
  tray.

### 4.4 Goals vs. alarms — two different things

Each meat probe has two independent temperature triggers:

- **Goal** — the done temperature (e.g., brisket 203 °F). Drives the
  time-remaining estimate, the progress bar, and the graphs' dashed
  goal lines. When crossed, a **GREEN banner** announces the meat is
  done (and pushes to your phone).
- **Alarm** — an *action* temperature with a label (e.g., **Wrap** at
  165 °F, **Baste**, **Beer Me**). When crossed, a **RED banner** tells
  you to go do the thing.

A brisket typically has both: alarm "Wrap" at 165 on the way to a goal
of 203. Both alerts are **one-shot**: once you acknowledge them, they
stay quiet for the rest of the cook (pulling the probe out to wrap the
meat will not re-ring an alarm you already handled). Every new cook
re-arms them fresh.

---

## 5. The Controls

### 5.1 The rotary encoder (panel)

| Gesture | Action |
|---|---|
| **Rotate** | Move focus between items / change a value being edited |
| **Click** (short press) | Select the focused item / confirm a value |
| **Long press** (~1 s) | Context back-out (e.g., dashboard → main menu) |
| **Hold ~1.2 s while an alert banner is showing** | Acknowledge (silence) the alert |

Notes on the alert hold: while a banner is on screen and the button is
held, the encoder belongs entirely to the acknowledge gesture — rotation
is ignored and the button release does not "click through" into whatever
was behind the banner. After the banner clears there is a ~1.5 second
settle period before the encoder resumes normal control.

### 5.2 Screen dimming

After **5 minutes** without encoder input, the LCD dims to 25 %
brightness. Any encoder touch restores full brightness — that first
touch only wakes the screen and is not passed through as a command. The
display never turns fully off; it stays glanceable from across the
patio.

### 5.3 The web dashboard

- **Target temperature:** tap **+5/−5**, or tap the number itself to
  type a value (100–499 °F).
- **Mode chips:** the row of mode buttons below the target. Chips are
  dimmed when not applicable (e.g., you can't jump straight from Off to
  Cook — ignite first).
- **Probe cards:** tap any probe card to open its setup editor (goal,
  meat type, alarm, alarm action).
- **Alert banner:** appears at the top — red or green. **Tap it to
  silence.**
- Everything else on the page — graphs, logs, programs, profiles,
  phone alerts, Wi-Fi, firmware — is described in its own section
  below.

---

## 6. Cook Modes Explained

| Mode | What the controller does | When to use it |
|---|---|---|
| **Off** | All outputs off. | Between cooks. |
| **Ignite** (Start) | Fan on; auger feeds at a fixed starting cycle; igniter element on. Automatically promotes itself to **Cook** the moment the pit passes 115 °F. If the grill is *already* above 115 °F (e.g., restarting after a power blip), pressing Start skips ignition entirely and goes straight to Cook — the button reads **Resume Cook**. | Beginning a cook; restarting a warm grill. |
| **Smoke** | Feast/famine smoke cycle — see below. | Maximum smoke flavor phase. |
| **Super Smoke** | A deeper feast/famine cycle: longer pauses, more smolder, more smoke, lower and swingier temperature. | The heaviest smoke, typically the first hours of a big cut. |
| **Cook** | Full PID temperature control to your target. The auger duty adjusts every 20 s; the fan modulates in gentle 2-second bursts when the pit is within ±10 °F of target, scaling air with fuel. | The workhorse mode — holding any temperature 100–499 °F. |
| **Keep Warm** | Same control loop as Cook; conventionally used at a low target (~165 °F) to hold finished food. | Resting/holding after the meat is done. |
| **Shutdown** | Auger off (no new fuel), fan on for a 15-minute pellet burn-off, then Off. | Always end cooks this way. |
| **Re-Ignite** | Manual re-ignition sequence (same behavior as Ignite). | Relighting after a flame-out. |

### About Smoke and Super Smoke — read this once

Smoke modes are **not temperature-controlled the way Cook is**, and
that's deliberate. Smoke flavor is produced when fresh pellets land on
dying embers and *smolder* before catching fire; a steadily-burning
pellet fire is too efficient and burns the flavor compounds up. Smoke
modes therefore cycle the fire on purpose: feed a burst of pellets, let
the fire fall back to embers, feed again. The oscillating temperature
*is* the smoke machine.

What the target means in smoke modes: **a band center, not a setpoint.**
The controller nudges the pause length (one step every 90 seconds)
to keep the pit inside roughly **target ±15 °F** while preserving the
smolder cycle. Practical guidance:

- For classic heavy-smoke temperatures (150–180 °F), **set the target
  low — around 170 °F** — before entering a smoke mode.
- Setting a higher target (e.g., 225) in Smoke mode gives you a warmer,
  lighter-smoke compromise.
- If you want a *held* temperature, use **Cook** — that's what the PID
  is for. Smoke modes will always wander within their band.

Meat takes on most of its smoke early, while the surface is cold and
moist (smoke ring formation stops around 140 °F internal). Spending the
first 2–4 hours in a smoke mode and the rest in Cook captures most of
the available benefit.

---

## 7. The Dashboards

### 7.1 The LCD cook dashboard

Top to bottom:

- **Cook Mode** (top-left) — the current mode.
- **FAN AUG IGN** (top-right) — live actuator status. Green = running
  (igniter shows red when energized), gray = off. The auger indicator
  blinking on and off during Cook is normal — that's the duty cycle.
- **CURRENT** — pit temperature, large.
- **TARGET** — the setpoint. Click it (when focused) to adjust in ±5°
  steps; click again to confirm.
- **Probe rows** (up to 4): number + meat type, current temperature,
  the alarm (red) and goal (green) temperatures, and a progress bar
  showing elapsed time (ET) vs. estimated time remaining (EST).
  Click a probe row to open its setup.
- **Bottom-right:**
  - **Next: P2 0:45** — the *soonest upcoming goal* across all probes:
    which meat finishes next and roughly when.
  - **Start 7/17 11:19 AM** — when this cook began. This timestamp is
    fixed at ignition and never changes with mode switches — total cook
    duration is always Start → Shutdown. It even survives power
    outages.

**About EST (estimated time remaining):** it is a projection from the
last hour's climb rate. During a **stall** (the hours-long plateau big
cuts hit around 150–170 °F internal), a rate projection would produce
absurd numbers, so the display honestly reads **"EST: stall"** instead.
When the stall breaks, the estimate returns and tightens rapidly.

### 7.2 The web dashboard

The same information plus:

- **ET · Start** line under the mode, and the **Next goal** line.
- **GRAPH** — a rolling one-hour chart of pit, target, and all probes,
  with dashed goal lines. Updates every 30 seconds.
- **COOK PROGRAMS / COOK PROFILES / COOK LOGS / PHONE ALERTS / WI-FI**
  sections — Sections 9–14.

---

## 8. Alarms and Alerts

All alerts appear as a **banner**: across the top of the LCD (blinking)
and at the top of the web page. **Green banners are good news** (a goal
was reached). **Red banners need you** (an action or a problem).

To silence: **hold the encoder button ~1.2 s**, or **tap the banner on
the web page**. If phone alerts are configured, every one of these also
buzzes your phone the moment it fires.

| Alert | Color | Meaning | After you acknowledge |
|---|---|---|---|
| **P2 Pork Butt 165°F – Wrap!** | Red | A meat probe hit its action alarm. | One-shot: stays quiet for the rest of this cook. |
| **Brisket DONE! P1 at 203°F** | Green | A meat probe reached its goal. | One-shot: stays quiet for the rest of this cook. |
| **GRILL TEMP DROP** | Red | The pit fell 30 °F+ below target for over a minute *after* having reached temperature. Wind, an open lid, or a dying fire. | Re-arms automatically once the pit recovers into band. Clears itself if the pit recovers before you even get there, and also clears if it fired only because you raised the target (a raise is a climb, not a drop). |
| **CHECK PELLETS – auger max, temp falling** | Red | Fuel starvation signature: the auger is running flat-out while the pit dives. Empty hopper, bridged pellets, or a jammed auger. You have roughly 10 minutes before the flame-out shutdown takes over. | Re-arms if the condition returns. |
| **GRILL SENSOR FAULT – SHUTTING DOWN** | Red | The pit probe went dead mid-cook; the controller is blind and has forced a shutdown burn-off. | Investigate the grill probe/jack. |
| **WRAP NOW** (or other gate text) | Red | A running Cook Program has reached a human step and is holding until you act. **Acknowledging it tells the program to continue.** | Program advances to the next step. |
| **BRISKET DONE** (program completion) | Green | A Cook Program reached its final holding step. | Informational. |
| **PROFILE HOLD – driver probe fault** | Red | A running program lost its driver probe signal and is holding the current step. | Clears automatically when the probe returns. |

Alarm hygiene built into the firmware, so you don't have to think about
it: probe alarms and goals **stand down automatically outside of
cooking modes** — probes dangling in hot pit air during Shutdown will
not ring alarms for meat that's already on the cutting board.

---

## 9. Cook Programs (Automated Cooking)

A **Cook Program** runs an entire cook for you as a sequence of steps:
*"Super Smoke until the brisket hits 140. Then Cook at 225 until 160.
Then tell me to wrap and wait. Then Cook at 250 until 203. Then hold
warm and tell me it's done."*

### 9.1 The model: one driver, many passengers

The grill has one fire, so a program follows exactly **one probe — the
driver** (you pick which when you start it; usually the most important
meat on the grill). Every temperature step in the program reads the
driver.

All other probes are **passengers**: their own goals and alarms keep
working normally (green DONE banners, wrap alarms, phone pushes), but
they never influence the pit. This matches how pitmasters actually run
mixed cooks — the brisket gets the pit it wants; the chickens come off
when their alarm says so.

### 9.2 Running a program

On the web dashboard, **COOK PROGRAMS** section:

1. Tap a program (★ marks the built-in factory programs).
2. Choose the **driver probe** (1–4).
3. Confirm. If the grill is Off, the program ignites it first; if it's
   already running, the program takes over from the current state.

The status card shows the program name, driver, and current step
(e.g., *"Step 2/5: cook 225 until P1 ≥ 160"*). Every step transition is
recorded in the cook log and pushed to your phone.

**Gates:** when the program reaches a human step (wrapping, saucing),
it raises a red banner with your instruction and **waits** — the pit
holds its current behavior, nothing advances until you acknowledge (on
either UI, or from the phone by opening the dashboard). Do the deed,
silence the banner, and the program moves on. Probe readings dipping
while you handle the meat cannot confuse it — a step once completed
stays completed.

**You always outrank the program.** If you manually change the mode or
target mid-program, the program notices and **pauses** itself, showing
"PAUSED (manual control)". Tap **RESUME** to hand control back (it
re-asserts the current step), or **STOP PROGRAM** to dismiss it — the
pit simply stays in whatever mode it's in. Turning the grill Off or
starting Shutdown always ends the program.

**Power outages:** a running program survives them. When power returns
and the grill auto-resumes the cook (Section 17), the program picks up
at the same step. Time-based steps restart their clock; temperature
steps just keep watching the driver.

### 9.3 The factory programs

Four ★ factory programs ship with the firmware. They're read-only —
open one with **VIEW**, then save it under a new name to customize.

**★ Brisket_Boss** — the canonical low-and-slow journey:
1. Super Smoke until driver ≥ 140 °F (maximum smoke while the meat can take it)
2. Cook @ 225 until driver ≥ 160
3. **GATE: WRAP NOW** (butcher paper — acknowledge to continue)
4. Cook @ 250 until driver ≥ 203
5. Keep Warm @ 165, green **BRISKET DONE** — holds until you're ready

**★ PorkButt_Solo** — same shape, more forgiving numbers:
1. Super Smoke until ≥ 150 · 2. Cook @ 250 until ≥ 165 ·
3. GATE: WRAP · 4. Cook @ 275 until ≥ 203 · 5. Keep Warm + PORK DONE

**★ Ribs_Champ** — the 3-2-1 clock cook (ribs are too thin for probe
temps, so the steps run on time):
1. Smoke for 3 hours · 2. GATE: WRAP + LIQUID · 3. Cook @ 225 for
2 hours · 4. GATE: UNWRAP + SAUCE · 5. Cook @ 250 for 1 hour ·
6. Green **RIBS DONE – check bend** (no hold)

**★ Chicken_Crispy** — hot and fast, skin first:
1. Smoke for 45 minutes (all the smoke poultry needs) ·
2. Cook @ 375 until driver ≥ 165 · 3. Green **PULL NOW** — deliberately
no Keep Warm, because holding steams the skin soft.

### 9.4 Writing your own programs

Tap **NEW PROGRAM** (or VIEW a factory one and rename). A program is a
few plain-text lines:

```
ver=1
step=supersmoke,0,temp,140,
step=cook,225,temp,160,
step=gate,0,ack,0,WRAP NOW
step=cook,250,temp,203,
step=keepwarm,165,end,0,DONE
```

Each `step=` line is `mode,target,exit,value,note`:

| Field | Options | Meaning |
|---|---|---|
| mode | `supersmoke` `smoke` `cook` `keepwarm` `gate` | What the pit does. `gate` leaves the pit as-is and raises the note as a banner. |
| target | °F (0 for gates; band center for smoke modes) | Pit setpoint for this step. |
| exit | `temp` `time` `ack` `end` | What ends the step: driver reaching a temperature, elapsed minutes, your acknowledgment (gates only), or never (`end` = terminal hold). |
| value | °F for `temp`, minutes for `time`, 0 otherwise | The exit threshold. |
| note | free text | Banner text for gates and terminal steps; shows in logs. |

Rules enforced by the controller: up to 8 steps; `gate` steps must use
`ack` (and only they may); temperature exits 50–400 °F; time exits
1–1440 minutes. A program that parses to zero valid steps is rejected
at save.

Program files live on the controller's storage and **survive firmware
updates**. They are deleted by saving an empty body under the same
name.

---

## 10. Cook Profiles (Saved Setups)

Separate from programs: a **profile** is a snapshot of your *setup* —
grill target plus all four probes' meat types, goals, and alarms.
Save the current setup with **SAVE CURRENT COOK**, and load it before a
future cook to restore the whole configuration in one tap. Loading a
profile does not start the grill or run any sequence — it just fills in
the settings. (Think of a profile as *what* you're cooking; a program
as *how the pit behaves over time.*)

Profiles can be saved/loaded/deleted from both UIs (LCD: **COOK
PROFILES** in the main menu).

---

## 11. Cook Logs and Graphs

The controller keeps a flight recorder for every cook.

- **One CSV file per cook**, named by start time
  (`cook_20260717_1603.csv`), created when the grill leaves Off and
  closed when it returns.
- A **sample row** is written every 30 s (or 10 s — Section 12.2) with
  the full state: pit temp, target, control output, fan/auger/igniter
  state, all probe temps, Wi-Fi signal.
- **Event rows** are written immediately for everything notable, with
  the source attributed: mode changes (`lcd`/`web`/`auto`), target
  changes, probe configuration, alarms, goals, program steps and gates,
  probe faults and recoveries, safety actions.
- **Every row is committed to flash the moment it's written.** A power
  cut loses at most the last 30 seconds — and when the cook auto-
  resumes after an outage, logging **continues in the same file** with
  a `POWER LOSS - resumed` marker, so one cook stays one file.

**Viewing:** the web page's **COOK LOGS** section lists all logs with a
**GRAPH** button (full-cook chart with goal lines) and a download link
(the CSV opens directly in any spreadsheet). The **GRAPHS** item on the
LCD main menu shows the live rolling hour.

**Capacity:** storage holds roughly 6–7 long cooks; when space runs
low the oldest logs are deleted automatically. Download anything you
want to keep forever.

---

## 12. Settings Reference

Panel: **Main Menu → SETTINGS**. Each row is focused by rotating and
activated by clicking.

### 12.1 BRIGHTNESS

Display backlight level, **10–100 %** in 10 % steps. Click the row to
enter adjustment (value turns green), rotate to change — the backlight
updates live as you turn — and click again to save. The saved level
persists across reboots and is the level the screen returns to when it
wakes from idle dimming (Section 5.2).

### 12.2 COOK LOG

Sampling interval for the cook log. Clicking cycles:

- **30s** (default) — one sample every 30 seconds. A 14-hour cook is
  about 120 KB. Right for almost everyone.
- **10s** — three times the detail, three times the file size. Useful
  when studying PID behavior.
- **OFF** — no cook files are created at all (events included).

This setting only affects logging; it changes nothing about control.

### 12.3 WI-FI (status) and WI-FI SETUP

The status block shows the connected network, signal strength, and the
dashboard address — this is where you look up the controller's IP.
**WI-FI SETUP** scans for networks and lets you join one using the
encoder (see Section 14 for the full Wi-Fi story, including phone-based
setup).

### 12.4 GRILL PROBE JACK

Selects **which physical probe jack (P1–P5) carries the pit/grill
probe.** All five jacks are electrically identical; this setting is
purely an assignment. The remaining four jacks become meat probes 1–4,
numbered in ascending jack order.

Clicking the row cycles P1 → P2 → … → P5. Two things to know:

- **The change is only accepted while the grill is Off.** The grill
  probe drives the PID, the igniter lockout, and the sensor-fault
  shutdown; reassigning it mid-fire will be refused (the value shows
  "(run)" as a reminder).
- The default is **P1**. Most owners never need to change this; it
  exists so a damaged jack never ends a cookout — move the pit probe to
  any healthy jack and reassign.

---

## 13. Phone Alerts

Every alarm, goal, program gate, and safety event can buzz your phone —
strongly recommended for overnight cooks. The controller uses
**ntfy.sh**, a free push service that requires no account.

**One-time setup (about two minutes):**

1. Install the **ntfy** app on your phone (App Store / Play Store), or
   just keep `https://ntfy.sh/your-topic` open in a browser.
2. Invent a **topic name** — treat it like a password, since anyone who
   knows it can see your alerts. Something like `pirate-grill-x7k2`.
   Letters, digits, `-` and `_` only.
3. In the app, **subscribe** to that topic.
4. On the web dashboard, **PHONE ALERTS** section: enter the same topic
   and tap **SAVE & TEST**. Your phone should buzz within seconds
   ("Notifications enabled — you're connected!").

That's it. From then on: *"P2 Pork Butt 165F - Wrap!"*, *"Brisket DONE!
P1 at 203F"*, *"CHECK PELLETS - auger max, temp falling"*, *"WRAP NOW"*
— wherever you are, including 4 AM. Clear the topic field and save to
disable. Multiple phones can subscribe to the same topic; the whole
family can follow the brisket.

---

## 14. Wi-Fi Setup

The controller joins your home Wi-Fi as a normal device.

**First-time setup (no network configured):** about 25 seconds after
power-up, the controller raises its own open hotspot named
**PelletPirate-Setup**. Join it with your phone, browse to
`http://192.168.4.1`, tap **CHANGE WI-FI**, pick your network, enter
the password. The controller joins your network and drops the hotspot;
its new address appears on the panel under Settings → WI-FI.

**From the panel:** Settings → **WI-FI SETUP** scans networks; pick one
with the encoder and enter the password on the on-screen keyboard
(rotate to a key, click to type).

**Changing networks later:** either method works any time; credentials
are stored and survive reboots and firmware updates.

Notes: the grill never *depends* on Wi-Fi — every control function
works from the panel with the network down, and the controller
reconnects automatically when the network returns.
`http://pelletpirate.local` works from Apple devices; Android and
Windows are happier with the IP address.

---

## 15. Firmware Updates

Updates install **over Wi-Fi** — no cables, no disassembly.

1. Obtain the new firmware file (`pelletpirate.bin`).
2. Make sure the grill is **Off** (an update reboots the controller).
3. Browse to **`http://<controller-ip>/update`**.
4. Choose the file and tap **Upload**. Progress shows as a percentage;
   at 100 % the controller flashes the new firmware and reboots. It's
   back on the network in about 30 seconds.

**What survives an update:** everything — Wi-Fi credentials, all
settings, probe setups, saved profiles, your custom programs, and cook
logs. Factory programs update *with* the firmware.

**If an update goes wrong:** the controller keeps the previous firmware
in a second slot. A new firmware must run healthily for 60 seconds to
become permanent; if it crashes before that, the next boot
**automatically rolls back** to the previous version.

**Updating mid-cook** is refused by default. The page offers a "force
while cooking" override: the controller reboots for ~30 seconds and
relies on the auto-resume system (Section 17) to pick the cook back up.
Use it only when a fix genuinely can't wait.

---

## 16. Built-In Safety Systems

These run always, in every mode, regardless of what any program or
person asks for:

| System | Behavior |
|---|---|
| **Boot-safe outputs** | Fan, auger, and igniter lines are driven OFF in the first instants of every boot, before anything else initializes. |
| **Igniter lockout** | The igniter element is physically inhibited whenever the pit is above 115 °F, in every mode. |
| **Igniter timeout** | If the igniter runs 20 minutes without the pit reaching 115 °F, the controller gives up and forces Shutdown (no endless glowing element over a pile of pellets). |
| **Combustion air interlock** | The auger and igniter can never run without the fan. |
| **Staggered start** | Fan, then auger (+2 s), then igniter (+4 s) — motor inrush currents never stack. |
| **Sensor-fault shutdown** | If the pit probe reads as dead for 30 s while running, the controller stops feeding pellets and forces a Shutdown burn-off — it will not fly blind. |
| **Flame-out shutdown** | After the pit has reached temperature: if it falls 60 °F+ below target and stays there 10 minutes (igniter assist having had its chance), the fire is presumed dead — forced Shutdown rather than piling fuel into a dead pot. |
| **Pellet starvation warning** | ~10 minutes *before* flame-out would trigger: auger pegged + temperature diving raises **CHECK PELLETS** (and a phone push). A fast hopper refill can often save the fire. |
| **Watchdog** | If the control software ever hangs, the hardware watchdog reboots the controller within 10 s — and a reboot lands in boot-safe state. |
| **Shutdown burn-off** | 15 minutes of fan-only operation empties the firepot of burning pellets before power-down. |

---

## 17. Power Outages

The controller is designed so a power blip is a non-event:

- **While cooking, the running mode and target are saved continuously.**
  When power returns, the controller waits for a valid pit reading and
  then decides:
  - Pit still at/above 115 °F → **the cook resumes automatically** in
    the same mode at the same target. No buttons.
  - Power was lost during **Shutdown** → the burn-off **always
    resumes** (fan on, no fuel) regardless of temperature — safety
    first.
  - Pit has gone cold → the grill stays **Off**. A cold grill never
    relights itself unattended; that decision needs a human. Walk out
    and press **Resume Cook** (the ignite screen's hot-restart is not
    needed when cold — a normal Start Ignite relights it).
- **The cook log continues in the same file** with a
  `POWER LOSS - resumed` marker; elapsed time and the Start timestamp
  carry through as if nothing happened.
- **A running Cook Program resumes at its current step.**
- If the outage killed the fire but left the pit warm (a long outage),
  the resumed cook may flame-out-shutdown shortly after — that's the
  safety net working. Relight manually after checking the firepot.

---

## 18. Troubleshooting

**A probe reads 0 °F / "--" (fault):**
- Nothing plugged into that jack → normal, that's how empty reads.
- Plug not fully seated → push it home.
- New probe → is it actually a **PT100**? (Section 4.1 — a PT1000
  reads as fault/garbage.) Check with a multimeter: ~109 Ω tip-to-
  sleeve at room temperature.
- After a long/greasy cook → moisture in probe or jack. Clean the jack
  (isopropyl), dry the probe (oven, 250 °F, 2 h).
- Reads fine in a different jack → the original jack needs cleaning; or
  reassign jacks (Section 12.4) and cook on.

**Probe reads a plausible but wrong temperature:** partially seated
plug, moisture leakage (reads *low*), or wrong probe type making
intermittent contact. Reseat, dry, verify type.

**"GRILL TEMP DROP" nags during normal operation:** it now requires a
sustained (60 s+) drop and clears itself on recovery or on target
raises. If it still fires often, your pit may genuinely be
under-recovering — check pellet quality (moist pellets burn weak) and
wind exposure.

**Smoke mode won't hold the temperature I set:** by design — see
Section 6. Use Cook for held temperatures; set a low target (~170) in
smoke modes for classic smoke-range temps.

**EST says "stall":** normal for big cuts between ~150–170 °F internal.
Not a malfunction — the estimate resumes when the meat starts climbing
again. (Wrapping shortens the stall.)

**Web page unreachable:** check Settings → WI-FI on the panel for the
IP and connection status. Use the IP rather than `pelletpirate.local`
on Android/Windows. If the controller lost your network it will retry
forever on its own; if it was never configured, look for the
**PelletPirate-Setup** hotspot (Section 14).

**Phone alerts not arriving:** confirm the app is subscribed to
*exactly* the topic saved on the dashboard (SAVE & TEST should buzz);
confirm the controller's network has internet access, not just LAN.

**The fan sounds different / pulses:** in-band fan modulation runs
2-second bursts by design. If your particular fan hums or surges
objectionably, report it — a one-line firmware change restores
continuous fan.

**Alarm keeps re-ringing after I handled it:** it shouldn't — probe
action alarms and goals are one-shot per cook on this firmware. A
*grill temp drop* or *check pellets* alarm re-arms by design because
those are ongoing conditions, not actions.

**The encoder did something I didn't intend right after silencing an
alarm:** it shouldn't on this firmware (the ack gesture is isolated
and followed by a 1.5 s settle). If you can reproduce it, note what
screen you were on.

---

## 19. Specifications

| Item | Value |
|---|---|
| Grill target range | 100–499 °F |
| Probe goal/alarm range | 100–499 °F (0 = off) |
| Temperature probes | 5 × 3-wire PT100 RTD, 3.5 mm TRS jacks |
| Pit control | PID, 20 s cycle, auger duty 15–100 % |
| Smoke cycle | 15 s feed / 55–105 s pause, self-adjusting (target ±15 °F band) |
| Fan modulation | 2 s burst cycle within ±10 °F of target; always-on otherwise |
| Ignite → Cook threshold | 115 °F |
| Igniter safety timeout | 20 minutes |
| Shutdown burn-off | 15 minutes |
| Sensor-fault shutdown | after 30 s of dead pit probe |
| Flame-out shutdown | target −60 °F sustained 10 min (after reaching band) |
| Temp-drop alarm | target −30 °F sustained 60 s (after reaching band) |
| Starvation warning | auger ≥95 % + pit −20 °F in 4 min |
| Cook log | CSV, 30 s or 10 s samples, ~6–7 long cooks retained |
| Cook programs | up to 8 steps; temp/time/gate/terminal exits |
| Display idle dim | 25 % after 5 min, wake on encoder |
| Alert acknowledge | hold encoder ~1.2 s / tap web banner |
| Connectivity | Wi-Fi 2.4 GHz; web dashboard + WebSocket live updates; ntfy.sh push |
| Firmware update | over-the-air via `/update`, automatic rollback on failed boot |
| Power-loss resume | automatic when pit ≥115 °F; Shutdown always resumes |

---

*PelletPirate V2 — built by hand, hardened by brisket.*
