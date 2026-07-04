# Device Naming Conventions

## Overview

The cocktail machine system consists of two distinct hardware entities with well-defined roles.
Each entity has a canonical name used consistently across hardware documentation, software
identifiers, protocol definitions, and API boundaries.

---

## Station

**The Station** is the central unit of the system. It houses the Raspberry Pi, the user-facing
display, and the glass placement area. It acts as the orchestrator of the entire machine:
it runs the main application, hosts the UI, manages connected Pods, and coordinates
dispensing sequences.

There is exactly **one Station** per machine.

**Responsibilities:**
- Runs the host application (UI, recipe logic, session management)
- Maintains USB connections to all Pods
- Sends dispensing commands and receives status/events from Pods
- Owns the domain model (recipes, ingredients, calibration state)

**Usage in code:**

```cpp
class Station { ... };          // top-level application context
StationConfig                   // configuration struct
StationStatus                   // runtime state
```

---

## Pod

**A Pod** is a self-contained dispensing module. Each Pod holds one or more bottles and
contains the embedded board, stepper motors, servos, valves, and load cells required to
measure and dispense its ingredients autonomously. A Pod exposes a USB CDC interface
and communicates with the Station via a defined serial protocol.

A machine can have **one or more Pods**, each identified by a unique pod ID assigned
at connection time.

**Responsibilities:**
- Controls its actuators (steppers, servos, valves) on command
- Reports weight measurements via load cells
- Responds to Station requests following the request–response protocol
- Emits events (e.g. calibration done, error conditions)

**Usage in code:**

```cpp
class Pod { ... };              // represents one connected Pod
PodId                           // strong type for pod identity
PodStatus                       // runtime state per pod
PodDescriptor                   // static metadata (capabilities, slot count)
```

---

## Relationship

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

The Station holds a registry of connected Pods. Each Pod is an independent device;
the Station is the sole initiator of communication.

---

## Naming Rules

| Context         | Station                  | Pod                        |
|-----------------|--------------------------|----------------------------|
| C++ class       | `Station`                | `Pod`                      |
| Config/type     | `StationConfig`          | `PodConfig`, `PodId`       |
| Protocol prefix | `station_`               | `pod_`                     |
| File names      | `station.cpp/.h`         | `pod.cpp/.h`               |
| Log tag         | `[station]`              | `[pod:<id>]`               |

Compound names always place the entity name first: `StationManager`, `PodDescriptor`,
`PodCalibrationState` — not `ManagerStation` or `DescriptorPod`.
