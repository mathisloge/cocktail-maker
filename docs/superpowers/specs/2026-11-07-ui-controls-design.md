# Station UI controls redesign

## Intent

Four controls still look like Slint's default Fluent widgets (blue accents) and break the art-deco look,
or are hard to read. They are customer-facing, so they come first. The admin area redesign follows as a
separate project.

## Goals

- **Boost slider:** a gradient track from slate (milder) over amber (original) to ember (stronger), a large
  round knob for touch, a mark at 0, snapping in 5 % steps over −80 … +80 %.
- **Spinner:** three pulsing amber dots, usable large (boot screen) and small (command rows).
- **Mix progress:** an amber light sweep instead of the blue indeterminate line. The command list below it
  already shows which step runs.
- **Button:** a readable disabled state (dark surface, translucent cream text, no lock symbol) and a
  `compact` variant (48 px) next to the normal one (75 px).

## Non-goals

- Segmented progress per step (discussed, kept for later).
- Restyling `SelectBox`/`NumberBox`/`ScrollArea`. That belongs to the admin project
  (`color-scheme: dark` is a candidate there).
- Switching to edition 1.19 inside the same commit as the controls.

## Approaches

- **A) Own components from Slint primitives (chosen).** Full control over gradient, snapping and
  animation, values only from `theme/`. Each control implements the matching std-widget interface from
  `std-widget-interfaces.slint` (experimental `interfaces` feature), so its API stays the one of the std
  widget it replaces and the compiler checks it.
- **B) Keep std widgets and recolor them.** Rejected: the std palette only exposes `color-scheme`, not the
  accent color, so blue stays blue and a gradient is impossible.
- **C) A custom Slint widget style.** Rejected: far too much for four controls and couples us to Slint
  internals.

## Decisions

- `gui/ui/slint-project.json` with `main: app-window.slint` and `experimental-features: ["interfaces"]`.
  `slint_target_sources` compiles the project file instead of `app-window.slint`. The VS Code extension
  reads the same file.
- Edition 1.19 is set in `slint-project.json` in a step of its own, verified with the snapshot test,
  because it changes layout rules.
- `BoostSlider` lives in `widgets/` (boost-specific, uses the boost colors). It implements
  `SliderInterface`. `components/value-slider.slint` is removed if nothing else uses it.
- `LoadingSpinner` implements `SpinnerInterface`. `ProgressBar` implements `ProgressIndicatorInterface`
  and shows the sweep while `indeterminate` is true (a plain amber fill for a set `progress`).
- `Button` gets `in property <bool> compact`. Every place that sets `height: Layout.touch-height-sm`
  on a button switches to `compact: true`. The disabled state uses `surface-elevated` and
  `text-primary` at reduced alpha via new semantic tokens.
- New tokens go into `theme/` (e.g. knob size, dot size, sweep duration). The slider has
  `accessible-role: slider` and keyboard steps of 5 %.
- The snapshot test covers the boot screen, detail page and mix page. Admin buttons are checked through
  `[.inspect]` and MCP.

## Left to the builder

- Exact sizes and durations of knob, dots and sweep, as long as they look like the approved mockup
  (https://claude.ai/artifact/Kz7aBn5HtTGjQ2WhgHXTxt: slider A, spinner B, progress C, button C).
- Whether the slider handles drag through `TouchArea` pointer events or `SwipeGestureHandler`.
