# The show front on System level

Date: 2026-11-21.

## Intent

SYS-016 and the pages around it describe the show front down to its parts: 8 columns, water and propylene glycol, 24 V air pumps, WS2811 LEDs and a WLED controller.
That is design, and it belongs to the Mechanics repo (cocktail-maker-3d).
The System sets the frame that the Station docs and Mechanics build on, top to bottom.
A subproject refers to the System, and the System never refers to a subproject decision.

## Decisions of the user

* Station and Pods are separate, and a Pod joins the Station only by power (IF-03), data (IF-01) and the drink tubes (IF-06).
* The Station is everything the Guest sees, and the Pods stand hidden.
* The Station shows each pour to the Guest with bubbles and light.
  This is a System decision, because it ties the Station software to hardware that Mechanics designs.
* The Station software computes every effect.
  It sends direct commands to the show front: air on or off, and light with colour and brightness, per effect channel.
* Mechanics delivers only hardware and bought parts, no software of its own.
  It states the number of channels, the link to the Station and the load.
* When the Station stops or the link breaks, the show front keeps its last state until someone switches the machine off.
  Air and light are harmless in that state.
* SYS-016 is rewritten in place, because it is from today and not published.

## Goals

* SYS-016 decides only what the Station and Mechanics must both rely on.
* A new contract IF-09 states what crosses between the Station software and the show front hardware.
* No System page names a part, a count, a liquid or a Mechanics ADR of the show front.

## Non-goals

* The mechanical design of the show front, its parts and its liquid.
* The effects and the animations.
* The changes in the cm3d repo.
  Mechanics refers to SYS-016 and IF-09 on its own.

## Approaches

1. **Rewrite SYS-016 in place.**
   Chosen by the user.
   The decision stays the same, only its level of detail changes, and nobody has read the old version.
2. A new SYS-018 that supersedes SYS-016.
   Rejected: a superseded ADR of the same day with wrong parts would only add noise.

## Changes

### SYS-016: The Station shows the pours with bubbles and light

* Context: the Pods stand hidden (SYS-015), so the Guest no longer sees an ingredient flow.
  A Pod joins the Station only by power, data and the drink tubes.
* Decision: the Station is everything the Guest sees.
  It shows each pour with bubbles and light, and the Station software computes every effect and sends direct commands to the show front (IF-09).
  No Pod knows about the effects, and the protocol of IF-01 does not change.
  How the show front is built is left to Mechanics.
* Alternative: effects on each Pod, rejected because the look would depend on the number and the place of the Pods, and a Pod would need a second job besides pouring.
* Alternative: a controller with its own effect logic, rejected because Mechanics delivers no software, and the effects follow the Station events most directly in the Station software.
* Consequences: the Station gets one more output for the effects, the show front may hold a liquid next to the Station electronics, an effect lags the pour by the link, and the show front keeps its last state after a fault.
* No parts, counts, voltages, liquids, controller favourites or Mechanics references.

### IF-09: Station software to show front

* Sides: the Station software (Station docs) and the show front hardware (Mechanics docs).
* Kind: data, and power through IF-02.
* What crosses: direct commands per effect channel, air on or off and light with colour and brightness.
* Guarantees and limits: the hardware has no effect logic, and Mechanics states the number of channels, the link and the load.
  The show front keeps its last state when the commands stop, until the machine is switched off.
* Requirements:
  * R01: The Station software computes every effect, and the show front only carries out commands (QG-06, SYS-016).
  * R02: A command to the show front or a fault of it never stops or delays a drink and never blocks the Station execution context (QG-03, SYS-016 and STA-005).
  * R03: The load of the show front at full effect stays inside the power budget (QG-03, open, see IF-03-R01 and QS-21).
  * R04: A liquid of the show front never reaches the electronics and never touches the flow path of a drink (QG-01, QG-02, open).
* Open questions: how fast an effect follows a pour, the link, and the leak protection of R04.
  The time of an effect gets no requirement, because the show front is a product wish that no quality goal covers.

### Other pages

* Glossary: "Show front" becomes "The part of the Station that shows each pour to the Guests with bubbles and light, and never touches a drink."
  "Show column" leaves the System glossary and the plugin copy.
* `building-blocks.adoc`: the show front box loses "show columns", the edge from the Station carries "IF-09 · sends effect commands", and the bullet says that IF-09 is its contract.
* `hardware.adoc`: one box "Show Front · bubbles and light" inside the Station, with an IF-09 data edge from the Raspberry Pi and a power edge from the power supply.
  No show controller, pumps or LEDs.
* `contracts.adoc`: IF-02 says only that the show front draws from the same power supply.
  IF-07-R04 drops "from a show column", because IF-09-R04 covers it.
* `quality-requirements.adoc`: the QS-21 stimulus says "while the show front runs at full effect".
* `safety.adoc`: the row points at IF-09-R04 instead of naming a show column.
* `power.adoc`: no change, because the show front draws from the 24 V rail of the power supply like every other consumer.
  How Mechanics converts it inside the show front is its own design.
* Station docs: "Show Front Control" in the building blocks refers to IF-09, the context table names IF-09 as the partner interface, and the risks page loses "water and propylene glycol".

## Left to the builder

* The wording, following `writing-style` and `writing-adrs`.
* Diagram widths at most 1400 px, checked through Kroki.
