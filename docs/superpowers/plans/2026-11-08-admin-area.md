# Admin Area Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the Pod/dispenser tabs of the admin area with a calm "Wartung" area: pump tiles, a page per pump,
a System page and the glasses, with the missing backend actions already in place but visibly unavailable.

**Architecture:** The station state bridge gets the data the tiles need (drink name, label, fill ratio, almost-empty,
firmware, Pod problems) and the new callbacks. New widgets (`PumpTile`, `PodCard`, `ProblemBanner`) and pages under
`pages/admin/` read `StationStateContext`. The pump page is a sub-state of `AdminArea`, not a new `Page` value.

**Tech Stack:** C++26 modules, Slint 1.20-dev (edition 1.18, `interfaces`), Catch2, `slint-testing.h`.

**Spec:** `docs/superpowers/specs/2026-11-08-admin-area-design.md`

## Global Constraints

- Follow `gui/ui/CONTRIBUTING.md`: layering, text roles without style overrides, no hex colours outside `theme/`,
  std-widgets only through `components/` wrappers.
- All UI copy is German. Volumes are written `mL`, like the rest of the UI.
- Buttons on tiles and pages use `compact: true`.
- "Almost empty": below 15 % of the bottle's content, or the Station suspects the bottle is empty.
- Unknown level: neutral grey "Füllstand unbekannt" without a bar, not shown as a problem.
- Foreseen actions (Reinigen, Rückwärts leeren, Neu verbinden, Neustarten) are disabled buttons with the caption
  "noch nicht verfügbar". Their bridge stubs log and send nothing to a Pod.
- Never block the execution context. Bridge callbacks spawn on the `AsyncScope` or post to the executor, UI updates go
  through `slint::invoke_from_event_loop`.
- Build with `-j2` at most. After a `.slint` change run `cd build && ninja -t cleandead && ninja -t restat` before
  building (ninja 1.13.2 dyndep abort).
- Comments are full sentences, no "-" or ";" joined sentences.
- Commits: Conventional Commits on `main`, no Co-Authored-By trailer, never stage `CMakePresets.json` or
  `hw_simulator/client.cppm`.

## Review Focus

- A drink reassigned from pump A to pump B: tile A must no longer show the drink (the mapping is keyed by ingredient,
  so the old dispenser loses it implicitly). Test in Task 2.
- A Pod that disconnects and connects again: the problem disappears, the tiles come back, no duplicate problem rows.
  Test in Task 3.
- A calibration run in progress: neither "← Pumpen", the section switch nor the global "← Zurück" may leave the pump
  page. Test in Task 5.
- More than 12 dispensers: the grid scrolls instead of clipping. Test in Task 4.
- A level with percent 0 (empty after refill) must not divide by zero when computing the bottle content. Test in Task 1.

---

### Task 1: Dispenser and Pod data for tiles

**Files:**
- Modify: `gui/ui/types/pod.slint`
- Modify: `gui/src/station_state_bridge.cppm`, `gui/src/station_state_bridge.cpp`
- Test: `gui/src/station_state_bridge.test.cpp`, `gui/src/station_flows.test.cpp:351-380`

**Interfaces:**
- Produces (Slint): `Dispenser { id: int, type: DispenserType, label: string, ingredient-name: string,
  fill-text: string, fill-known: bool, fill-ratio: float, almost-empty: bool }`.
  `Pod` gains `firmware-version: string`.
- Produces (C++, exported from `cm.gui:station_state_bridge`):
  `struct FillPresentation { std::string text; bool known; float ratio; bool almost_empty; };`
  `[[nodiscard]] FillPresentation present_fill_level(const FillLevelView& view);` replacing `format_fill_level`.
  `[[nodiscard]] std::string dispenser_label(DispenserType type, int number);` → "Pumpe 1", "Ventil 1".

- [ ] **Step 1: Replace the `format_fill_level` test with `present_fill_level`**

```cpp
TEST_CASE("present_fill_level words and measures what is known about a bottle", "[gui][StationStateBridge]")
{
    const auto grams = [](double value) { return value * cm::units::si::gram; };
    using cm::units::percent;
    const auto unknown = cm::gui::present_fill_level(cm::FillLevelView{});
    CHECK(unknown.text == "Füllstand unbekannt");
    CHECK_FALSE(unknown.known);
    CHECK_FALSE(unknown.almost_empty);
    CHECK(cm::gui::present_fill_level(cm::FillLevelView{.no_load_cell = true}).text == "Füllstand unbekannt");
    const auto no_capacity = cm::gui::present_fill_level(cm::FillLevelView{.level = grams(640)});
    CHECK(no_capacity.text == "ca. 640 mL");
    CHECK_FALSE(no_capacity.known);
    const auto full = cm::gui::present_fill_level(cm::FillLevelView{.level = grams(640), .percent = 64.0 * percent});
    CHECK(full.text == "640 mL von 1000 mL");
    CHECK(full.known);
    CHECK(full.ratio == Catch::Approx(0.64));
    CHECK_FALSE(full.almost_empty);
    CHECK(cm::gui::present_fill_level(cm::FillLevelView{.level = grams(90), .percent = 14.0 * percent}).almost_empty);
    CHECK_FALSE(cm::gui::present_fill_level(cm::FillLevelView{.level = grams(150), .percent = 15.0 * percent}).almost_empty);
    const auto drained = cm::gui::present_fill_level(cm::FillLevelView{.level = grams(0), .percent = 0.0 * percent});
    CHECK(drained.text == "0 mL");
    CHECK(drained.almost_empty);
    const auto empty = cm::gui::present_fill_level(
        cm::FillLevelView{.level = grams(30), .percent = 3.0 * percent, .suspected_empty = true});
    CHECK(empty.text == "vermutlich leer");
    CHECK(empty.almost_empty);
    CHECK(empty.ratio == 0.0F);
}

TEST_CASE("dispenser_label numbers pumps and valves from one", "[gui][StationStateBridge]")
{
    CHECK(cm::gui::dispenser_label(cm::gui::DispenserType::Pump, 1) == "Pumpe 1");
    CHECK(cm::gui::dispenser_label(cm::gui::DispenserType::Valve, 2) == "Ventil 2");
}
```

Add to the existing pod lifecycle test a section "A connected pod lists labelled dispensers and its firmware":
`update_info` with `num_pumps = 2, num_valves = 1, firmware_version = {1, 4, 0}`, then the model row has
`firmware_version == "1.4.0"` and dispenser labels `"Pumpe 1", "Pumpe 2", "Ventil 1"`, each with
`fill_text == "Füllstand unbekannt"` and `fill_known == false`.

- [ ] **Step 2: Run, expect a compile failure** (`present_fill_level`, `label` and `firmware_version` unknown).

Run: `cmake --build --preset developer -j2 --target cocktail-maker-gui-tests`
Expected: FAIL to compile.

- [ ] **Step 3: Implement**

Bottle content = level / (percent / 100), only when percent > 0. With percent 0 the text is "0 mL" and ratio 0.
`ratio` is clamped to 0…1. Dispensers are numbered per type starting at 1 (`id` stays the protocol id).
`update_dispenser_fill_text` becomes `update_dispenser_fill(const PodDispenser&, const FillPresentation&)` and sets
all four fields. Format the firmware with the existing `std::formatter<cm::Version>`.

- [ ] **Step 4: Update the flow test texts** in "The admin row shows the level and takes a refill":
`"unbekannt"` → `"Füllstand unbekannt"`, `"ca. 0 mL"` stays, `"ca. 300 mL · 100 %"` → `"300 mL von 300 mL"`.
The `pod-config-tab.slint` row keeps compiling, as `fill-text` stays.

- [ ] **Step 5: Run** `./build/runtime/cocktail-maker-gui-tests "[StationStateBridge],[flow]"`. Expected: PASS.

- [ ] **Step 6: Commit** `feat(gui): describe dispensers for the admin tiles`

### Task 2: Drink names on dispensers

**Files:**
- Modify: `cocktail-maker/station_config.cppm`, `cocktail-maker/station_config.cpp`
- Modify: `gui/src/station_state_bridge.cppm`, `gui/src/station_state_bridge.cpp`, `gui/src/gui_application.cpp`
- Test: `cocktail-maker/station_config.test.cpp`, `gui/src/station_state_bridge.test.cpp`

**Interfaces:**
- Produces (`cm::StationConfig`):
  `void set_change_handler(std::function<void()> handler);` called after every assignment change and after `load()`,
  on the thread that made the change.
  `[[nodiscard]] std::vector<std::pair<IngredientId, PodDispenser>> assignments() const;`
- Produces (`cm::gui::StationStateBridge`):
  `void watch_assignments(const StationConfig& station_config, const IngredientStore& ingredient_store);`
  Publishes the current names at once and on every change. `PodUiModel` keeps
  `std::map<PodDispenser, std::string>` of display names (requires `operator<` or a hash for `PodDispenser`, add one
  if missing) and applies it to existing rows and to rows it creates later.

- [ ] **Step 1: Write failing tests**

`station_config.test.cpp`: "StationConfig reports every change of the assignment": a handler counting calls,
`update_dispenser_ingredient_mapping` twice → count 2, `assignments()` holds one entry per ingredient.

`station_state_bridge.test.cpp`: "StationStateBridge - Dispensers show the drink assigned to them":
a `StationConfig` over a `TempDir` with ingredients "rum" ("Weißer Rum") and "lime" ("Limettensaft"),
a connected pod "Pod-A" with 2 pumps, `watch_assignments`, assign rum → dispenser 0, flush:
dispenser 0 `ingredient_name == "Weißer Rum"`, dispenser 1 `== ""`.
Then assign rum → dispenser 1, flush: dispenser 0 `== ""`, dispenser 1 `== "Weißer Rum"`.

- [ ] **Step 2: Run, expect compile failure.**

- [ ] **Step 3: Implement.** The handler runs on the executor in production, so the bridge's handler reads
`assignments()` and `find_by_id(...)->display_name` there and posts the finished map with
`slint::invoke_from_event_loop`. `GuiApplication::run` calls `watch_assignments(station_config_, ingredient_store_)`
right after creating the bridge.

- [ ] **Step 4: Run** `cocktail-maker-lib-tests "[StationConfig]"` and `cocktail-maker-gui-tests "[StationStateBridge]"`.
Expected: PASS.

- [ ] **Step 5: Commit** `feat(gui): show the assigned drink on each dispenser`

### Task 3: Pod problems and maintenance commands

**Files:**
- Create: `gui/ui/types/maintenance.slint`
- Modify: `gui/ui/globals/station-state-context.slint`
- Modify: `gui/src/station_state_bridge.cppm`, `gui/src/station_state_bridge.cpp`
- Test: `gui/src/station_state_bridge.test.cpp`

**Interfaces:**
- Produces (Slint): `export struct PodProblem { pod-id: string, text: string }` in `types/maintenance.slint`.
  `StationStateContext`:
  `in property <[PodProblem]> pod-problems;`
  `in property <bool> clean-available: false; in property <bool> reverse-available: false;`
  `in property <bool> restart-available: false; in property <bool> reconnect-available: false;`
  `callback clean-dispenser(Pod, Dispenser); callback reverse-dispenser(Pod, Dispenser);`
  `callback restart-pod(Pod); callback reconnect-pod(Pod); callback emergency-stop-all();`
- Problem texts (time `HH:MM`, local time): disconnected `"{id} getrennt um {HH:MM}"`,
  incompatible `"{id} hat eine inkompatible Firmware ({status-text})"`. One entry per Pod id, replaced on change,
  removed when the Pod reaches `connected`. Pods without an id are ignored.

- [ ] **Step 1: Write failing tests** ("StationStateBridge - Pod problems" with sections):
  - A pod that connects, then is destroyed → exactly one problem, `pod_id == "Pod-A"`, text starts with
    `"Pod-A getrennt um "`.
  - A new pod state with the same id that connects → no problems.
  - A pod reaching `incompatible` → text starts with `"Pod-A hat eine inkompatible Firmware"`.
  - "Emergency stop reaches every pod": register a fake `IPod` counting `force_safe_state` (move `RefusingPod` from
    `station_flows.test.cpp` into a new shared `gui/src/fake_pod.test.hpp` and use it in both), invoke
    `emergency_stop_all`, run `ctx` until idle → `stops == 1`.
  - "Foreseen actions start disabled": all four `*_available` are false after construction.

- [ ] **Step 2: Run, expect failure.**

- [ ] **Step 3: Implement.** `emergency-stop-all` spawns `pod_registry.force_safe_state_all_pods()` and logs an error
when it throws. The four foreseen callbacks log a warning "… is not supported yet" and do nothing else.

- [ ] **Step 4: Run** `cocktail-maker-gui-tests "[StationStateBridge],[flow]"`. Expected: PASS.

- [ ] **Step 5: Commit** `feat(gui): track pod problems and add maintenance commands`

### Task 4: Admin shell and pump tiles

**Files:**
- Modify: `gui/ui/theme/layout.slint` (tile tokens: `tile-radius`, `tile-padding`, `fill-bar-height`)
- Create: `gui/ui/widgets/pump-tile.slint`, `gui/ui/widgets/problem-banner.slint`
- Create: `gui/ui/pages/admin/pumps-section.slint`
- Modify: `gui/ui/pages/admin/admin-area.slint`
- Create test: `gui/src/admin_area.test.cpp` (add to `gui/CMakeLists.txt` test list)

**Interfaces:**
- Produces: `PumpTile { in property <string> pod-id; in property <Dispenser> dispenser; callback refill();
  callback open(); }` with `accessible-role: button`, `accessible-label: pod-id + " " + dispenser.label`, default
  action = `open()`. Shows "nicht belegt" and a "Belegen" button (also `open()`) when `ingredient-name` is empty.
- `ProblemBanner { in property <[PodProblem]> problems; callback details(); }` shows the first problem and
  "Details →".
- `AdminArea` sections: `enum AdminSection { pumps, glasses, system }` (private to `admin-area.slint`), header
  "WARTUNG" with "Pumpen", "Gläser", "System". `in-out property <bool> pump-open; property <Pod> open-pod;
  property <Dispenser> open-dispenser;` (the pump page itself comes in Task 5; here `pump-open` only switches to a
  placeholder `BodyText` "Pumpe").
- `PumpsSection` lays out tiles in a 4-column grid inside a `ScrollArea`. Tiles of all `StationStateContext.pods`,
  in model order.

- [ ] **Step 1: Write failing UI tests** in `admin_area.test.cpp`, with a fixture like `DetailPage` in
`boost_slider.test.cpp` that opens `Page::AdminAreaPage` and sets `pods` to a `VectorModel` with "Pod-A" (pumps
`Pumpe 1` "Weißer Rum" 0.64, `Pumpe 2` "Limettensaft" 0.12 almost-empty, `Pumpe 3` unassigned):
  - "shows a tile per dispenser": `find_by_accessible_label(ui, "Pod-A Pumpe 1")` not empty, same for 2 and 3.
  - "refills from the tile": on `mark_dispenser_refilled` records the dispenser id. Invoke the default action of
    the "Nachgefüllt" button inside the "Pod-A Pumpe 2" tile → recorded id 1.
  - "opens the pump page from a tile": default action on "Pod-A Pumpe 1" → element with label "← Pumpen" exists.
  - "scrolls with more than 12 tiles": a pod with 16 pumps, the tile "Pod-A Pumpe 16" exists and the section's
    `ScrollArea` viewport height is larger than its visible height (check via `size()` of the last tile's
    `absolute_position().y` beyond 720).
  - "shows a pod problem": `pod-problems = [{pod-id: "Pod-B", text: "Pod-B getrennt um 21:12"}]` → an element with
    label "Pod-B getrennt um 21:12" exists.

- [ ] **Step 2: Run, expect failure** (labels not found).

- [ ] **Step 3: Implement** the widgets and section per the spec's Pumpen section: small `LabelText` label
"{pod-id} · {label}", `TitleText` drink name, fill bar (`fill-known`), `CaptionText` fill text, error tone when
`almost-empty`, the "Nachgefüllt" button `primary` when almost empty. Remove the old tab strip. "Details →" switches
to the System section.

- [ ] **Step 4: Run** `cocktail-maker-gui-tests "[admin_area]"`. Expected: PASS. Render
`CM_SNAPSHOT_DIR=… "[.snapshot]"` and look at `admin_area_page`.

- [ ] **Step 5: Commit** `feat(ui): show pumps as tiles in the admin area`

### Task 5: Pump page with care, setup and calibration

**Files:**
- Create: `gui/ui/pages/admin/pump-page.slint`
- Modify: `gui/ui/pages/admin/dispenser-calibration.slint` (export `DispenserCalibrationPanel` instead of the popup)
- Modify: `gui/ui/pages/admin/admin-area.slint`
- Delete: `gui/ui/pages/admin/pod-config-tab.slint`
- Test: `gui/src/admin_area.test.cpp`

**Interfaces:**
- Consumes: Task 1 `Dispenser` fields, Task 3 callbacks and `*-available` flags.
- Produces: `PumpPage { in property <Pod> pod; in property <Dispenser> dispenser; callback back(); }`.
  Header "← Pumpen" (disabled while `DispenserCalibrationContext.is-busy()`) and "{pod.id} · {dispenser.label}".
  Left: drink name, fill text, bar, "Nachgefüllt". Right: "Pflege" with Reinigen, Rückwärts leeren (hidden for
  valves), Identify. "Einrichten" with Getränk ändern (shows a `SelectBox` over `RecipeContext.ingredient-ids`,
  calls `assign-ingredient-to-dispenser`) and Kalibrieren (replaces the right column with
  `DispenserCalibrationPanel`, whose "Fertig" returns to the actions).
- The pump page reads the dispenser fresh from `StationStateContext.pods` by pod id and dispenser id, so refills and
  name changes show while it is open.
- The section switch in the admin header is disabled while `DispenserCalibrationContext.is-busy()`.

- [ ] **Step 1: Write failing UI tests**
  - "foreseen actions are disabled": on the pump page, `accessible_enabled()` of "Reinigen" and "Rückwärts leeren"
    is false, and an element labelled "noch nicht verfügbar" exists.
  - "valves have no backwards run": open a valve tile → no element labelled "Rückwärts leeren".
  - "identify reaches the bridge": default action on "Identify" → `highlight_dispenser` recorded.
  - "calibration keeps the page while running": default action on "Kalibrieren", set
    `DispenserCalibrationContext.state.dispenser-status = running` → `accessible_enabled()` of "← Pumpen" and of
    the global "← Zurück" is false.
  - "back returns to the tiles": default action on "← Pumpen" → "Pod-A Pumpe 1" tile exists again.

- [ ] **Step 2: Run, expect failure.**

- [ ] **Step 3: Implement.** Keep the calibration steps and texts of `DispenserCalibrationContent`, drop the popup
card, scrim and `PopupWindow`. Remove `PodConfigTab` and its import.

- [ ] **Step 4: Run** `cocktail-maker-gui-tests "[admin_area],[DispenserCalibrationBridge]"`. Expected: PASS.

- [ ] **Step 5: Commit** `feat(ui): add a page per pump with care and setup actions`

### Task 6: System section

**Files:**
- Create: `gui/ui/widgets/pod-card.slint`, `gui/ui/pages/admin/system-section.slint`
- Modify: `gui/ui/widgets/system-log.slint` (filter)
- Modify: `gui/ui/pages/admin/admin-area.slint`
- Test: `gui/src/admin_area.test.cpp`

**Interfaces:**
- Produces: `PodCard { in property <Pod> pod; in property <string> problem; }` with a status dot (connected →
  success colour, otherwise error), firmware, "{n} Pumpen · {m} Ventile", the problem or "keine Fehler", and
  "Neu verbinden" / "Neustarten" bound to the `*-available` flags and callbacks of Task 3.
- `SystemLog` gains `in property <bool> errors-only;` and the section shows chips "Alle" and "Fehler" above it. <!-- codespell:ignore -->
- A danger-styled button "Not-Stopp alle Pods" calls `StationStateContext.emergency-stop-all()`. <!-- codespell:ignore -->
- Problems of Pods that are gone appear as plain rows above the cards.

- [ ] **Step 1: Write failing UI tests**
  - "lists pods with their firmware": label "1.4.0" (or the card text containing it) exists for "Pod-A".
  - "emergency stop reaches the bridge": default action on "Not-Stopp alle Pods" → callback recorded. <!-- codespell:ignore -->
  - "restart and reconnect are disabled": `accessible_enabled()` false for both.
  - "the error filter hides other entries": log entries one info "Mojito fertig", one error "Timeout bei Ping",
    default action on "Fehler" → no element with label/text "Mojito fertig" (give log rows `accessible-label` of
    their message).

- [ ] **Step 2: Run, expect failure.** - [ ] **Step 3: Implement.** - [ ] **Step 4: Run**
`cocktail-maker-gui-tests "[admin_area]"`. Expected: PASS.

- [ ] **Step 5: Commit** `feat(ui): add the system section with pod status and log filter`

### Task 7: Glasses look, dark std-widgets, docs

**Files:**
- Modify: `gui/ui/pages/admin/glass-config-tab.slint` → rename to `glasses-section.slint`, new look
- Modify: `gui/ui/components/select-box.slint`, `gui/ui/components/number-box.slint`
- Modify: `gui/ui/CONTRIBUTING.md`, `docs/station/modules/ROOT/pages/building-blocks.adoc` (admin area description,
  if it describes the tabs)

- [ ] **Step 1: Take before snapshots** `CM_SNAPSHOT_DIR=<scratch>/before "[.snapshot]"`.
- [ ] **Step 2: Restyle** the glasses section with the tile tokens and compact buttons, same functions.
  Try `color-scheme: dark` on the wrapped std-widget. Keep it only if the snapshot shows the dark look.
- [ ] **Step 3: Run** the full suite `ctest --preset developer` and take after snapshots. Compare `admin_area_page`,
  open the glasses and system sections through `[.inspect]` and the MCP server, and look at each by eye.
- [ ] **Step 4: Commit** `feat(ui): restyle the glasses section` and `docs(ui): describe the new admin area`.
