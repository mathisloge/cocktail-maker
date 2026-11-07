# Station UI Controls Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the default-styled boost slider, spinner and progress line with themed controls and give `Button` a readable disabled state and a `compact` size.

**Architecture:** A `gui/ui/slint-project.json` enables the experimental `interfaces` feature (and later edition 1.19). The new controls are built from Slint primitives, read only `theme/` tokens, and implement the std-widget interfaces from `std-widget-interfaces.slint`.

**Tech Stack:** Slint (master `8d51770e`, 1.20.0-dev), C++26, Catch2 with `slint-testing.h`, CMake.

**Spec:** `docs/superpowers/specs/2026-11-07-ui-controls-design.md`

## Global Constraints

- Approved look: mockup https://claude.ai/artifact/Kz7aBn5HtTGjQ2WhgHXTxt, slider A, spinner B, progress C, button C (no lock symbol) plus compact.
- Boost range −80 … +80 %, snapping in 5 % steps, 0 = original recipe.
- Follow `gui/ui/CONTRIBUTING.md`: folder rules, only semantic tokens, text roles without overrides, no `std-widgets.slint` outside `components/`.
- Interfaces are imported from `"std-widget-interfaces.slint"` and implemented with `implement <Name>Interface <=> self;`.
- Precondition: the user's Slint bump in `CMakeLists.txt` (`gh:slint-ui/slint#8d51770e…`) is committed before Task 1.
- Build with `cmake --build --preset developer -j2`. Tests: `ctest --preset developer`. Snapshots: `CM_SNAPSHOT_DIR=<dir> ./build/runtime/cocktail-maker-gui-tests "[.snapshot]"`.
- Commits on the current branch, Conventional Commits, no Co-Authored-By trailer. Never stage `CMakePresets.json`.

## Review Focus

- Tapping the slider track, not only dragging the knob, must jump to the tapped value (snapped). Pinned by the pointer test in Task 3.
- Reopening the detail page starts at 0 % again, as today (`boost-value` is private to the page and starts at 0). Pinned by the "reopen" assertion in Task 3.
- A disabled compact button must still be 48 px and readable. Checked by `[.inspect]` on the calibration popup in Task 5.
- Pulsing dots and the sweep must stop when hidden (no animation running on invisible elements). Checked by reading the `running`/`visible` bindings in Task 4 review.
- The mix page with `prefers-reduced-motion` is out of scope for Slint here. Not tested.

---

### Task 1: Slint project file

**Files:**
- Create: `gui/ui/slint-project.json`
- Modify: `gui/CMakeLists.txt` (`slint_target_sources(generated-ui ui/slint-project.json NAMESPACE cm::gui)`)

**Interfaces:**
- Produces: experimental feature `interfaces` available to every `.slint` file of `generated-ui`.

- [ ] **Step 1: Failing check.** Add `import { SliderInterface } from "std-widget-interfaces.slint";` to `components/value-slider.slint`, build. Expected: compile error that the import needs the experimental `interfaces` feature.
- [ ] **Step 2: Create `slint-project.json`** with `"main": "app-window.slint"` and `"experimental-features": ["interfaces"]`. No edition yet.
- [ ] **Step 3: Point CMake at it** and build. Expected: build passes with the temporary import. Remove the temporary import again.
- [ ] **Step 4: `ctest` and snapshot `c1`** compared against a fresh baseline `c0` taken before Step 1. Expected: identical apart from known animation noise.
- [ ] **Step 5: Commit** `build(ui): compile the ui through a slint project file`.

### Task 2: Edition 1.19

**Files:**
- Modify: `gui/ui/slint-project.json` (`"edition": "1.19"`), and any `.slint` file whose layout shifts.

- [ ] **Step 1: Set the edition, build.** Expected: the "no edition" info is gone. Record new warnings.
- [ ] **Step 2: Snapshot `c2` vs `c1`.** Every difference must be explained by the 1.19 layout changes. Fix layouts where the old look should stay. List intended changes in the commit message body.
- [ ] **Step 3: `ctest`.**
- [ ] **Step 4: Commit** `build(ui): switch the ui to slint edition 1.19`.

### Task 3: BoostSlider

**Files:**
- Create: `gui/ui/widgets/boost-slider.slint`, `gui/src/boost_slider.test.cpp` (add to `cocktail-maker-gui-tests` in `gui/CMakeLists.txt`)
- Modify: `gui/ui/pages/recipe-detail-page.slint`, `gui/ui/theme/layout.slint`
- Delete: `gui/ui/components/value-slider.slint` (and its import in `app-window.slint`)

**Interfaces:**
- Produces: `export component BoostSlider` with `implement SliderInterface <=> self;`, defaults `minimum: -80`, `maximum: 80`, `step: 5`, `accessible-role: slider`, `accessible-label: "Alkohol-Boost"`, `accessible-value` = the current value as text. `changed(value)` fires only when the snapped value changes. `released(value)` fires on pointer up.
- Tokens: `Layout.boost-knob-size` (40px), `Layout.boost-track-height` (8px).

- [ ] **Step 1: Write the failing test** `TEST_CASE("boost slider snaps to 5 percent steps", "[boost_slider]")` in `boost_slider.test.cpp`. Create `AppWindow`, set `station_ready`, `boot_screen_confirmed`, `active_screen = CocktailDetailPage`, flush events, record `RecipeContext.on_boost_active_recipe` values. Find the slider with `slint::testing::ElementHandle::find_by_accessible_label(ui, "Alkohol-Boost")`, then:
  - `REQUIRE(slider.size() == 1)`, `accessible_value_step() == 5`, `accessible_value_minimum() == -80`, `accessible_value_maximum() == 80`.
  - `set_accessible_value("12")` → `accessible_value() == "10"`, last boost callback value `10`.
  - `invoke_accessible_increment_action()` → `"15"`. `invoke_accessible_decrement_action()` twice → `"5"`.
  - `set_accessible_value("200")` → `"80"`. `set_accessible_value("-200")` → `"-80"`.
  - Reopen: switch `active_screen` to `CocktailListPage` and back to `CocktailDetailPage`, find the slider again → `accessible_value() == "0"`.
  - Pointer: dispatch a pointer press and release at 75 % of the slider's width (`absolute_position()` + `size()`, `slint::testing::send_mouse_click` or window `dispatch_pointer_*`) → `"40"`.
- [ ] **Step 2: Run** `./build/runtime/cocktail-maker-gui-tests "[boost_slider]"`. Expected: FAIL, no element with that label (today's slider has none).
- [ ] **Step 3: Implement `BoostSlider`.** Track with `@linear-gradient(90deg, Palette.boost-negative 0%, Palette.accent 50%, Palette.boost-positive 100%)`, zero mark in `Palette.text-primary` at 50 % opacity, round knob in `Palette.text-primary` with an `accent` ring. Snapping: `round(v / step) * step`, clamped to `[minimum, maximum]`. A `TouchArea` maps the pointer x to a value on press and on move. Accessibility actions `set-value`, `increment`, `decrement` go through the same snap function.
- [ ] **Step 4: Use it in `recipe-detail-page.slint`** instead of `ValueSlider`. Keep the "milde"/"stark" captions and the `changed` handler that calls `RecipeContext.boost-active-recipe`. Delete `value-slider.slint`.
- [ ] **Step 5: Run the test.** Expected: PASS. Then `ctest`.
- [ ] **Step 6: Snapshot** the detail page and compare it with the mockup by eye.
- [ ] **Step 7: Commit** `feat(ui): add a themed boost slider with 5 percent steps`.

### Task 4: LoadingSpinner and ProgressBar

**Files:**
- Modify: `gui/ui/components/loading-spinner.slint`, `gui/ui/components/progress-bar.slint`, `gui/ui/theme/layout.slint`, `gui/ui/theme/motion.slint`

**Interfaces:**
- Produces: `LoadingSpinner` with `implement SpinnerInterface <=> self;` and `in property <length> dot-size: Layout.spinner-dot-size;`. Three dots in `Palette.accent`, pulsing opacity 25 % → 100 % and scale 0.8 → 1 with `Motion.spinner-cycle` (1.2s), staggered by a third of the cycle. Used large in `boot-screen.slint` and small in `widgets/command-item.slint` (`dot-size` from a smaller token).
- Produces: `ProgressBar` with `implement ProgressIndicatorInterface <=> self;`. Track in `Palette.surface-elevated`, height `Layout.progress-height`. With `indeterminate`, a sweep of 30 % width with a transparent → `accent` → transparent gradient moves left to right in `Motion.sweep` (1.6s). Otherwise a fill of `progress * width` in `accent`.
- Tokens: `Layout.spinner-dot-size` (10px), `Layout.spinner-dot-size-sm` (5px), `Motion.spinner-cycle`, `Motion.sweep`.

- [ ] **Step 1: Failing check.** `grep -n 'inherits Spinner\|inherits ProgressIndicator' gui/ui/components/*.slint` lists both wrappers.
- [ ] **Step 2: Implement both components.** No `std-widgets.slint` import remains in these two files.
- [ ] **Step 3: Verify** the grep from Step 1 is empty. Build, `ctest`.
- [ ] **Step 4: Snapshot** boot screen and mix page, compare with the mockup by eye. These pages animate, so judge by eye, not by pixel count.
- [ ] **Step 5: Commit** `feat(ui): add themed spinner and progress sweep`.

### Task 5: Button disabled state and compact size

**Files:**
- Modify: `gui/ui/components/button.slint`, `gui/ui/theme/colors.slint`, and every button that sets `height: Layout.touch-height-sm` (`pages/admin/pod-config-tab.slint:141,150,156,162`, `pages/admin/dispenser-calibration.slint:146`, `pages/admin/glass-config-tab.slint` "Add" buttons and the "x" button at 51-52, `pages/mix-error-page.slint:267` if it is a button).

**Interfaces:**
- Produces: `Button.compact: bool` (default false). `min-height` is `Layout.touch-height-sm` when compact, else `Layout.touch-height`.
- Tokens: `Palette.surface-disabled` (`surface-elevated`) and `Palette.text-disabled` (cream at 45 %).

- [ ] **Step 1: Failing check.** `grep -rn 'height: Layout.touch-height-sm' gui/ui --include=*.slint` lists the button sites above.
- [ ] **Step 2: Implement** `compact` and the disabled state (background `surface-disabled`, border `border`, text `text-disabled`, for primary and secondary alike).
- [ ] **Step 3: Switch the button sites** to `compact: true` and drop their `height`. Non-button sites (admin tab `min-height`, select boxes) stay.
- [ ] **Step 4: Verify** the grep from Step 1 only lists non-button elements. Build, `ctest`, snapshots.
- [ ] **Step 5: Inspect** `CM_INSPECT_PAGE=admin_area_page` with MCP: select a Pod, open the calibration popup. The disabled "Mit Referenzgewicht kalibrieren" is readable, the compact buttons are 48 px.
- [ ] **Step 6: Commit** `feat(ui): readable disabled buttons and a compact size`.

### Task 6: Docs

**Files:**
- Modify: `gui/ui/CONTRIBUTING.md`

- [ ] **Step 1:** Document `slint-project.json` (experimental features and edition live there), that new controls implement the matching std-widget interface, `BoostSlider` in `widgets/`, and `Button.compact`. Update the std-widgets section: spinner and progress are own components now.
- [ ] **Step 2: Commit** `docs(ui): document the slint project file and new controls`.
