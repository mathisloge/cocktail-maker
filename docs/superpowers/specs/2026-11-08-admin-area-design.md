# Admin area redesign

## Intent

The admin area ("Wartung") looks clunky and overloaded today: three English tabs, a Pod list on the left, a
dispenser list on the right, and many framed buttons. It is organised the way the hardware is built (Pod, then
dispenser), not the way people work with it.

Two groups use it, always by touch and without a laptop:

- **Bar staff during service:** refill, check the status, clean pumps. This must work without searching.
- **The owner during setup:** assign drinks to pumps, calibrate. This may sit one or two taps deeper.

There are no roles. Everyone can do everything. The operator PIN stays, so guests cannot get in.

## Goals

- A calm, light look in the existing art-deco theme: fewer frames, more space, German labels.
- Errors are visible at first glance.
- Refill is one tap from the entry page. Cleaning is two taps away (tile, then "Reinigen").
- Actions whose backend is still missing are already in place: clean (run until stopped), run backwards,
  restart Pod, reconnect Pod.

## Non-goals

- Login, roles or different views per user.
- A guided cleaning process for all pumps at once. Cleaning is an action per pump.
- Backend work for clean, backwards, restart and reconnect. These are open points in
  `docs/station/modules/ROOT/pages/risks-and-technical-debt.adoc`.
- New functions for glasses and calibration. They keep their behaviour and only get the new look.
- A dispense history per pump. The backend has none today.

## Anti-goals

- Buttons that look active but do nothing. A foreseen action is visibly unavailable.
- A technician's view (Pod IDs, ports, protocol details) on the entry page.

## Approaches

1. **Rebuild with prepared commands (chosen).** New pages and widgets, new callbacks in the globals and
   C++ bridges. The emergency stop works right away. Clean, backwards, restart and reconnect are dimmed
   buttons with "noch nicht verfügbar", and the C++ side exists as a stub. When the backend arrives only the
   bridge changes.
2. **UI only, without new callbacks.** Less C++ now, but UI and bridges get touched again later.
3. **Restyle today's tabs first, then restructure.** The look would be done twice, because tiles and the pump
   page need new building blocks anyway.

## Decisions

### Structure

- Entry stays the operator PIN popup. Then the admin area opens with a header "WARTUNG", three sections
  **Pumpen**, **Gläser**, **System**, and "← Zurück" to leave.
- The pump page is a sub-state inside the admin area, not a new value of the global `Page` enum.
  The rest of the app's navigation stays unchanged.

### Pumpen (entry page)

- Up to 12 pumps as tiles in a 4×3 grid, without scrolling. If there are more, the grid scrolls.
- One tile shows: Pod and pump number as a small label, the drink name, a thin fill bar, the remaining volume,
  and a "Nachgefüllt" button. An unassigned pump shows "nicht belegt" and a "Belegen" button.
- **Almost empty:** below 15 % of the bottle's content, or when the Station suspects the bottle is empty.
  The tile shows its bar and text in the error colour, and its "Nachgefüllt" button is filled.
- **Unknown level** (no load cell, or no refill yet so the content is unknown): a neutral grey
  "Füllstand unbekannt" without a bar. It is not shown as a problem.
- **Valves** get tiles too, labelled "Ventil 1" and so on. They count toward the 12. "Rückwärts leeren" is
  hidden for valves. (My call.)
- A Pod that loses its connection disappears from the tiles, as today. The Station does not keep
  disconnected Pods in the model.
- A Pod problem shows as a banner above the grid ("Pod 2 getrennt um 21:12", "Details →" opens System).
- Tapping a tile (outside its button) opens the pump page.

### Pump page

- Header row: "← Pumpen" and "Pod 1 · Pumpe 2".
- Left: the drink name large, "90 ml von 700 ml", a fill bar, the "Nachgefüllt" button.
- Right, section **Pflege**: Reinigen, Rückwärts leeren, Identify.
- Right, section **Einrichten**: Getränk ändern, Kalibrieren.
- Reinigen and Rückwärts leeren are dimmed with "noch nicht verfügbar" until the backend exists.
- Getränk ändern uses today's drink selection in the new look.
- Kalibrieren runs today's calibration steps on the pump page instead of in a popup.

### System

- One card per connected Pod: connection status with a coloured dot, firmware version, number of pumps and
  valves, its problem or "keine Fehler", and the buttons "Neu verbinden" and "Neustarten".
  Both are dimmed with "noch nicht verfügbar" for now.
- **Pod problems** are only connection problems for now: a Pod that disconnected ("Pod 2 getrennt um 21:12")
  and a Pod with an incompatible protocol release. The bridge records the time when it sees the problem.
  A disconnected Pod's message stays until that Pod connects again. (My call.)
- Below the cards: the log with the filter chips "Alle" and "Fehler". Log entries carry no Pod id today, <!-- codespell:ignore -->
  so a filter per Pod waits until logging tags them. (My call.)
- A button "Not-Stopp alle Pods" calls the existing `PodRegistry::force_safe_state_all_pods()`. <!-- codespell:ignore -->

### Gläser

- Same functions as the current glass tab, in the new look.

### Data the UI needs

- `Dispenser` today only carries `id`, `type` and `fill-text`. Tiles need the drink name, the fill as a
  fraction, the remaining and full volume as text, and whether it is almost empty. The station state
  bridge provides them from the existing mapping and fill level tracker.
- `Pod` needs the firmware version for the System cards.
- The station state bridge keeps a list of Pod problems (text and time) for the banner and the System page,
  including Pods that are gone.

### Look

- Values only from `theme/`. New tokens go there (tile radius, tile padding and similar).
- Buttons on tiles and pages use `compact: true`.
- `SelectBox` and `NumberBox` in drink selection and glasses get `color-scheme: dark` in their wrappers,
  if that removes the light std look.

## Left to the builder

- Exact sizes, spacing and the grid behaviour above 12 pumps.
- How the bridge stubs report "not supported", as long as nothing is sent to a Pod.
- Splitting the work into tasks and which existing admin files are removed or reused.
