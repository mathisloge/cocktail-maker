# GUI `ui/` Structure and Theme Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restructure `gui/ui/` into dependency-ordered folders with a single theme layer, fixed text roles and theme-aware wrappers, keeping the current look.

**Architecture:** Folders `theme → components → types → globals → widgets → pages/popups → app-window`, each importing only from earlier ones. All colors, typography, sizes and motion come from `theme/`. Text uses role components without local overrides. Std-widgets are only used through wrappers in `components/`.

**Tech Stack:** Slint (`.slint`), C++26 modules, Catch2, CMake presets.

**Spec:** `docs/superpowers/specs/2026-11-07-gui-ui-structure-design.md`

## Global Constraints

- No redesign: the look stays. Small visual changes from merging near-duplicate values are allowed. The builder decides what counts as intended, and lists every visible change in the final summary.
- Global, struct and enum names exported to C++ stay unchanged (e.g. `cm::gui::Page`, `StationStateContext`, `RecipeView`). Only the **component** `Page` is renamed, to `PageBase`.
- Dependency order: a folder may import only from folders listed before it. `widgets/` must not import `globals/`. `components/` must not import `types/` or `globals/`.
- Plain `Text {}` only inside `components/`. Pages, popups and widgets never import `std-widgets.slint`.
- Outside `theme/`: no hex colors, no raw `px`/`pt`/`rem` except `0px`, `1px` hairlines and layout math (e.g. `parent.width - 1px`, percentages).
- Imports always use explicit relative paths (`./`, `../`). No barrel files.
- Build with `cmake --build --preset developer -j2` (never more parallel). Tests: `xvfb-run -a ctest --preset developer`.
- Commits on the current branch, Conventional Commits, **no Co-Authored-By trailer**. Never stage `CMakePresets.json`.
- Comments only where they explain intent, as full sentences.

## Review Focus

- Text that was centered by default (`SmallText`, `LargeText`) loses centering when moved to a role without explicit `horizontal-alignment` → each replacement keeps today's alignment. Covered by the screenshot diff in Task 5.
- `in-out` properties that C++ sets (e.g. `ProcessContext.glass`) must keep their names when files move → covered by the full build plus `ctest` after every task.
- Popups still open full-size over the page after moves (`width/height: root.*`) → covered by popup snapshots in Task 1.
- A wrapper that drops a callback or property (`Slider.changed`, `ScrollView` viewport) silently breaks the boost slider or scrolling → Task 4 re-exposes every property used today and the station flow tests stay green.
- Font not found after moving the `.ttf` → falls back silently. Covered by the snapshot diff in Task 2.

---

## File Structure (target)

```
gui/ui/
  theme/        colors.slint (Palette, TextTone), typography.slint (Typography), layout.slint (Layout),
                motion.slint (Motion), SpaceGrotesk.ttf
  components/   button, chip, separator, separator-text, corner-deco, pulse-dot, text.slint (roles),
                scroll-area, loading-spinner, progress-bar, value-slider
  types/        recipe.slint, pod.slint, glass.slint, log.slint, navigation.slint
  globals/      *-context.slint (unchanged names)
  widgets/      recipe-card, recipe-tags, glass-card, glass-icon, command-item, command-list-item,
                boost-text, system-log
  pages/        page-base.slint, recipe-list-page, recipe-detail-page, mix-*-page, boot-screen,
                admin/ (admin-area, pod-config-tab, glass-config-tab, dispenser-calibration)
  popups/       fill-warning-popup, manual-command-popup, operator-pin-popup
  app-window.slint
  CONTRIBUTING.md
```

Removed: `components.slint`, `pages.slint`, `popups.slint`, `data.slint`, `palette.slint`, `admin-area.slint` (root), `components/{small,medium,large}-text.slint`.

---

### Task 1: Snapshot harness and baseline

**Files:**
- Create: `gui/src/ui_snapshots.test.cpp`
- Modify: `gui/CMakeLists.txt` (add to `cocktail-maker-gui-tests` sources)
- Modify: `gui/ui/app-window.slint` (`boot_screen_confirmed` becomes `in-out property <bool> boot-screen-confirmed`)
- Create (scratchpad, not committed): `compare_ppm.py`

**Interfaces:**
- Produces: hidden Catch2 test `"ui snapshots"` tagged `[.snapshot]`, run with `CM_SNAPSHOT_DIR=<dir> ./build/runtime/cocktail-maker-gui-tests "[.snapshot]"`. Writes one `<name>.ppm` (P6) per screen. Skips (SUCCEED) when the env var is unset.
- Produces: `python3 -I compare_ppm.py <before-dir> <after-dir>` prints per file `<name>: <n> differing pixels` and exits 0.

- [ ] **Step 1: Make boot confirmation settable from C++** by turning the private property into `in-out property <bool> boot-screen-confirmed: false;` and updating its three uses in `app-window.slint`.
- [ ] **Step 2: Write the snapshot test.** Create `AppWindow`, set window size to 1280×720 physical, `show()`. Snapshot `boot_screen` with `station_ready=false`. Then set `station_ready=true`, `boot_screen_confirmed=true` and for each `Page` value except `BootScreen` and `FinishedPage` set `active_screen`, `flush_slint_events()`, `window().take_snapshot()`, write PPM named after the enum value in snake_case. Then on the list page open each popup via `invoke_open_fill_warning_popup()`, `invoke_open_manual_command_popup(Command{})`, `invoke_open_operator_pin_popup()`, snapshot, close it. Use `SlintFlusher` from `slint_events.test.hpp`. `REQUIRE` each snapshot has a value.
- [ ] **Step 3: Build and run** `cmake --build --preset developer -j2 && CM_SNAPSHOT_DIR=$SCRATCH/before xvfb-run -a ./build/runtime/cocktail-maker-gui-tests "[.snapshot]"`. Expected: 11 PPM files (boot, 7 pages, 3 popups), each 1280×720.
- [ ] **Step 4: Write `compare_ppm.py`** in the scratchpad and check `compare_ppm.py before before` prints 0 for every file.
- [ ] **Step 5: Run `xvfb-run -a ctest --preset developer`.** Expected: all pass, snapshot test not listed.
- [ ] **Step 6: Commit** `test(ui): add hidden snapshot test for all screens`.

---

### Task 2: Folder layout, types split, theme move (pixel-identical)

**Files:**
- Create: `types/{recipe,pod,glass,log,navigation}.slint` from `data.slint` (recipe: `CommandStatus`, `Command`, `RecipeView`, `Ingredient`; pod: `DispenserType`, `Dispenser`, `ConnectionState`, `Pod`; glass: `GlassIconData`, `GlassDescriptor`; log: `LogLevel`, `LogEntry`; navigation: `Page`).
- Move: `palette.slint` → `theme/colors.slint` (Palette) and `theme/layout.slint` (Layout), `SpaceGrotesk.ttf` → `theme/`.
- Move: domain components to `widgets/`, `admin-area.slint` and `admin/*` to `pages/admin/`, `pages/page.slint` → `pages/page-base.slint` with component renamed `PageBase`, `popups/manual_command_popup.slint` → `popups/manual-command-popup.slint`.
- Delete: `data.slint`, `palette.slint`, `components.slint`, `pages.slint`, `popups.slint`.
- Modify: every `.slint` import, `app-window.slint` (font import path, globals re-exports stay in `app-window.slint`).

**Interfaces:**
- Produces: final file locations from File Structure above, still with today's token names (`Palette.background`, `Layout.font-size-xs`, ...).

- [ ] **Step 1: Check the failing condition:** `ls gui/ui/data.slint gui/ui/components.slint` exists and `grep -rln 'data.slint' gui/ui` is non-empty.
- [ ] **Step 2: `git mv` all files** to their targets and split `data.slint`.
- [ ] **Step 3: Rewrite imports** to direct relative paths. `app-window.slint` must still `export` every global and every type C++ uses (keep the existing `export { ... }` lines, add `export` for types only if the build reports missing C++ names).
- [ ] **Step 4: Verify:** `grep -rnE 'from "[a-z]' gui/ui` empty (no prefix-less paths), `grep -rn 'data.slint\|palette.slint\|components.slint' gui/ui` empty, `grep -rln 'globals/' gui/ui/widgets gui/ui/components` empty.
- [ ] **Step 5: Build and `ctest`.** Expected: green, no C++ change needed.
- [ ] **Step 6: Snapshot to `$SCRATCH/t2`, compare with `before`.** Expected: 0 differing pixels everywhere.
- [ ] **Step 7: Commit** `refactor(ui): split ui into dependency-ordered folders`.

---

### Task 3: Theme tokens (pixel-identical)

**Files:**
- Modify: `theme/colors.slint`, `theme/layout.slint`
- Create: `theme/typography.slint`, `theme/motion.slint`
- Modify: every consumer of the renamed tokens.

**Interfaces:**
- Produces in `theme/colors.slint`: non-exported `global Primitives` (raw values: `ink #0a0806`, `ink-raised #120e0a`, `ink-elevated #1c1510`, `amber #c8913a`, `amber-light #e8b86d`, `cream #f5eed8`, `sand #d9cfae`, `red #c0392b`, `green #5cad75`, slate/ember pairs for boost, the six log hues, `black`). Exported `global Palette` with semantic tokens only, referencing `Primitives` (alpha via `.with-alpha()`, ±1 rounding acceptable). Renames: `background`→`surface`, `background-card`→`surface-card`, `background-elevated`→`surface-elevated`, `text-secondary`→`text-interactive`. All other token names unchanged. `mount-animation-*` removed from Palette.
- Produces in `theme/colors.slint`: `export enum TextTone { primary, interactive, label, muted, on-accent, error, success }` and `Palette.text-color(tone: TextTone) -> color` (pure public function).
- Produces `export global Typography` in `theme/typography.slint`: `font-family: "Space Grotesk"`, and for each role `display|headline|title|body|label|caption` the properties `<role>-size`, `<role>-weight`, `<role>-spacing`. Start values: display 42pt bold 1.5pt, headline 30pt bold 1.5pt, title 20pt extra-bold 0.38rem, body 14pt normal 1.5pt, label 12pt semi-bold 2pt, caption 10pt normal 2pt. `Layout.font-size-*` are removed and their uses point to `Typography.<role>-size` (xs→caption, sm→label, md→body, lg→title, xl→headline, display→display).
- Produces `export global Motion` in `theme/motion.slint`: `mount-duration: 200ms`, `mount-easing: ease-in`, `mount-delay: 60ms` (used by `PageBase`'s timer).

- [ ] **Step 1: Failing condition:** `grep -rn 'Palette.background\b\|Palette.text-secondary\|font-size-\|mount-animation' gui/ui --include=*.slint | grep -v theme/` is non-empty.
- [ ] **Step 2: Implement the four theme files** with the values above.
- [ ] **Step 3: Rename all uses.** `app-window.slint` sets `default-font-family: Typography.font-family`.
- [ ] **Step 4: Verify:** the grep from Step 1 is empty, and `grep -rn 'Primitives' gui/ui | grep -v theme/` is empty.
- [ ] **Step 5: Build, `ctest`, snapshot `t3` vs `before`.** Expected: 0 or ±1-value differences only from alpha rounding.
- [ ] **Step 6: Commit** `refactor(ui): introduce primitive and semantic theme tokens`.

---

### Task 4: Std-widget wrappers and box replacement

**Files:**
- Create: `components/scroll-area.slint` (`ScrollArea`), `components/loading-spinner.slint` (`LoadingSpinner`), `components/progress-bar.slint` (`ProgressBar`), `components/value-slider.slint` (`ValueSlider`).
- Modify: `pages/recipe-list-page.slint`, `pages/recipe-detail-page.slint`, `pages/mix-progress-page.slint`, `pages/boot-screen.slint`, `widgets/command-item.slint`, `popups/operator-pin-popup.slint`, `pages/admin/{pod-config-tab,glass-config-tab,dispenser-calibration}.slint`.

**Interfaces:**
- Produces: `ScrollArea inherits ScrollView`, `LoadingSpinner inherits Spinner`, `ProgressBar inherits ProgressIndicator`, `ValueSlider inherits Slider`, each in its own file and the only place in `ui/` that imports `std-widgets.slint`. They start as plain subclasses so the look stays the same. Theming them is future work in one place. If a wrapper can set a themed property without visible change (e.g. size from `Layout`), do it there.

- [ ] **Step 1: Failing condition:** `grep -rln 'std-widgets.slint' gui/ui | grep -v '^gui/ui/components/'` is non-empty.
- [ ] **Step 2: Create the wrappers** and switch every use to them.
- [ ] **Step 3: Replace `VerticalBox`/`HorizontalBox`** with `VerticalLayout`/`HorizontalLayout` using `padding: Layout.spacing-sm; spacing: Layout.spacing-sm;` (the Box defaults of the current style). Keep explicit paddings/spacings already set on those boxes.
- [ ] **Step 4: Verify** the grep from Step 1 is empty.
- [ ] **Step 5: Build, `ctest`, snapshot `t4` vs `t3`.** Expected: no difference, or ≤1px offsets where Box defaults differed. Note any in the change list.
- [ ] **Step 6: Commit** `refactor(ui): route std-widgets through themed wrappers`.

---

### Task 5: Text roles

**Files:**
- Create: `components/text.slint`
- Delete: `components/{small,medium,large}-text.slint`
- Modify: every file using `SmallText`/`MediumText`/`LargeText` or plain `Text {}` outside `components/` (app-window station title, `AdminTab` in `pages/admin/admin-area.slint`, `pages/admin/dispenser-calibration.slint:75`).

**Interfaces:**
- Consumes: `Typography`, `Palette.text-color`, `TextTone` (Task 3).
- Produces: `DisplayText`, `HeadlineText`, `TitleText`, `BodyText`, `LabelText`, `CaptionText`, each `inherits Text` with `in property <TextTone> tone` and size/weight/letter-spacing bound to its `Typography` group. Default tone per role: the tone most of today's uses of that size have. Consumers may set `text`, `tone`, alignment, `wrap`, `overflow`, `font-italic`, geometry. Nothing else.

- [ ] **Step 1: Failing condition:** `grep -rnE 'SmallText|MediumText|LargeText' gui/ui` non-empty, and `grep -rnE '^\s*(font-size|letter-spacing|font-weight|font-family):' gui/ui --include=*.slint | grep -vE 'components/text.slint|theme/'` non-empty.
- [ ] **Step 2: Implement `components/text.slint`.**
- [ ] **Step 3: Migrate file by file.** Map by current effective size. Keep today's alignment explicitly where the old component centered by default. Where a use overrode size/spacing/weight, choose the closest role. Add a new role or token only if no role fits within one size step. Record each visible change (file, element, old → new).
- [ ] **Step 4: Verify** both greps from Step 1 are empty, and `grep -rn '^\s*Text {' gui/ui | grep -v components/` is empty.
- [ ] **Step 5: Build, `ctest`, snapshot `t5` vs `t4`.** Look at every non-zero file. Each difference must match an entry in the change list.
- [ ] **Step 6: Commit** `refactor(ui): replace text overrides with typography roles`.

---

### Task 6: Magic numbers to tokens

**Files:**
- Modify: `theme/layout.slint`, `theme/motion.slint` and the files with raw lengths (largest first: `pages/mix-error-page.slint`, `widgets/glass-card.slint`, `pages/mix-success-page.slint`, `widgets/system-log.slint`, `pages/boot-screen.slint`, ...).

**Interfaces:**
- Produces: new `Layout` tokens for recurring values (at least `border-width` 1px where used as a border, `accent-bar` 3px, row/card heights and icon sizes used in more than one place, `panel-width-side` 380px) and `Motion` tokens for design durations (pulse 1700ms, long-press 4s). One-off values that are layout math stay local.

- [ ] **Step 1: Failing condition:** `grep -rnoE '\b[0-9]+(\.[0-9]+)?(px|pt|rem)\b' gui/ui --include=*.slint | grep -v theme/ | grep -vE ':(0|1)px$'` lists entries.
- [ ] **Step 2: Add tokens and replace uses.** Repeated near-equal values merge into one token (record the change).
- [ ] **Step 3: Verify:** the grep from Step 1 only lists layout math. Every remaining hit is reviewed by eye.
- [ ] **Step 4: Build, `ctest`, snapshot `t6` vs `t5`.** Expected: differences only for recorded merges.
- [ ] **Step 5: Commit** `refactor(ui): replace magic numbers with layout and motion tokens`.

---

### Task 7: CONTRIBUTING and CLAUDE.md

**Files:**
- Create: `gui/ui/CONTRIBUTING.md`
- Modify: `CLAUDE.md` (GUI (Slint) section)

- [ ] **Step 1: Write `gui/ui/CONTRIBUTING.md`** covering: purpose of each folder, the dependency order and the extra rules (widgets without globals, components without types/globals), theme usage (semantic `Palette` only, `Primitives` stays private, text roles without overrides, no raw lengths except `0px`/`1px`/layout math, no plain `Text {}` and no `std-widgets.slint` outside `components/`), naming (kebab-case files, one exported component per file, CamelCase components, explicit relative imports), how to add a token, a role or a wrapper, and how to run the snapshot test and compare before/after.
- [ ] **Step 2: Update CLAUDE.md's GUI section:** folder list matches the new layout, the components rule reads "`components/` only depend on `theme/`", and one line points to `gui/ui/CONTRIBUTING.md`.
- [ ] **Step 3: Final checks:** all greps from Tasks 2–6 empty or reviewed, full build, `xvfb-run -a ctest --preset developer` green, snapshot `final` vs `before` reviewed against the change list.
- [ ] **Step 4: Commit** `docs(ui): document ui structure and theme rules`.
- [ ] **Step 5: Report** to the user: the change list of visible differences (file, element, old → new) and the location of the before/final snapshots.
