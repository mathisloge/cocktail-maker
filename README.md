# Sir-Mix-A-Lot

Sir-Mix-A-Lot is a full-stack hardware/software project: a self-contained kiosk that mixes and
pours cocktails unattended, designed to be reliable and food-safe. The project is developed end to end — embedded
firmware, host application, UI/UX and mechanical/electrical design.


See https://mathisloge.github.io/cocktail-maker/ how this machine is build. 
---

## Vision

| Quality Goal        | What it means for Sir-Mix-A-Lot                                             |
|----------------------|-----------------------------------------------------------------------|
| **Safety**           | No mechanism, electrical fault, or software bug may endanger a user   |
| **Food Safety**      | All guest-contact parts and flow paths use food-safe materials        |
| **Reliability**      | The machine must run unattended for long stretches without failure   |
| **Maintainability**  | Operator can service, refill, and calibrate without deep expertise |
| **Usability**        | Guests with no instructions can order and receive a drink intuitively |

---

## System Architecture

Sir-Mix-A-Lot consists of exactly **one Station** and **one or more Pods**.

```
┌─────────────────────────────────┐
│             Station             │
│  (Raspberry Pi + Display + UI)  │
│                                 │
│   ┌─────────┐   ┌─────────┐    │
│   │  Pod 0  │   │  Pod 1  │... │
│   │ (USB)   │   │ (USB)   │    │
│   └─────────┘   └─────────┘    │
└─────────────────────────────────┘
```

### Station

The **Station** is the central orchestrator of the machine. It houses the Raspberry Pi,
the guest-facing touch display, and the glass placement area. There is exactly one
Station per machine.

- Runs the host application (UI, recipe logic, session management)
- Maintains USB connections to all Pods
- Sends dispensing commands and receives status/events from Pods
- Owns the domain model: recipes, ingredients, calibration state

### Pod

A **Pod** is a self-contained dispensing module holding one or more bottles, along with
the embedded board, stepper motors, servos, valves, and load cells needed to measure and
dispense its ingredients autonomously. Each Pod communicates with the Station over a USB
CDC interface using a defined serial request–response protocol, and is identified by a
unique Pod ID assigned at connection time.

- Controls its own actuators (steppers, servos, valves)
- Reports weight measurements via load cells
- Responds to Station requests
- Emits events (calibration complete, error conditions, etc.)

The Station is the sole initiator of communication; each Pod is an independent device
that the Station tracks.

---

## Naming Conventions

Naming is deliberately consistent across hardware docs, code, protocol definitions, and
API boundaries.

Compound names always place the entity name first — `StationManager`, `PodDescriptor`,
`PodCalibrationState` — never `ManagerStation` or `DescriptorPod`. C++ identifiers use
entity-first compound naming; Slint files/components use kebab-case. All code comments
and in-app text are written in English.

See [docs/naming-conventions.md](docs/naming-conventions.md).

---

## Technology Stack

### Station

- https://github.com/mathisloge/cocktail-maker

#### Backend (C++)

- **C++ Modules** for compilation units (`.cppm` for module interface (partition) units `.cpp` for module implementation partition units and `.test.cpp` for TU test files)
- **Boost.Asio / Boost.Cobalt** for asynchronous I/O and coroutines
- **mp-units** for physical quantities and unit-safe arithmetic
- **libassert** for expressive runtime assertions
- **Catch2** for testing
- **spdlog** for logging, with a custom sink that feeds a ring-buffer `slint::Model<LogEntry>`
  for on-device log display.
- Strong domain types throughout: `PodId`, `DispenserId`, `IngredientId`, `RecipeId`

#### Frontend (Slint)

- A centralized **design color system** — no theme switching, single dark art-deco aesthetic
- Shared components should go into components and never depend on the globals except Layout and Palette.
- `global` singletons used idiomatically to avoid prop-drilling; purely presentational
  components read directly from globals (pages and admin sections).


### Pod (Embedded)

- Zephyr with C++
- **mp-units** for physical quantities and unit-safe arithmetic
- https://github.com/mathisloge/cocktail-maker-embedded

### Station-Pod Protocol

- Based on **comms**
- https://github.com/mathisloge/cocktail-maker-protocol
