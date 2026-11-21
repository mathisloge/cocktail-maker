# Quality scenarios as cards

Date: 2026-11-21.

## Intent

The quality scenarios on the System page "10 Quality Requirements" are hard to read.
It is not clear where one scenario starts and where it ends.
Each of the 12 fields takes a line for its name and a line for its value, so a scenario fills the screen with little text.
A long page is fine.

## Goals

* Each scenario reads as one card with a clear start and end.
* A scenario needs about half the height of today.
* The page stays readable at phone width.

## Non-goals

* Chips for Goal, Q42, Verification and State, and a coloured State.
  CSS cannot read the text of a field, so this needs an Asciidoctor extension later.
* A change to the AsciiDoc source of the scenarios or to the test that checks their fields.

## Approaches

1. **CSS only in `docs/ui/supplemental/css/site-extra.css`.**
   Chosen by the user.
   The rules find a scenario by `.sect2:has(> h3[id^="QS-"])` and lay out its definition list as a grid.
   The source and `parse_quality_scenarios` stay as they are.
2. An Asciidoctor extension next to `docs/ui/extensions/d2-blend.js` that renders design A with chips.
   Not now: it is more code with its own tests, it depends on the field format, and Node is not installed in this environment.
3. A table per scenario in the source.
   Rejected: it changes 34 scenarios and the test, and design A does not need it.

## Design

The look is design A of the mockup `a-css.html` from this session.

* A scenario section is a card.
  It has the background `--cm-ink-raised`, a 4 px `--cm-amber` border on the left and its heading on a `--cm-ink-elevated` band.
* The definition list is a grid of four columns: label, value, label, value.
* Goal and Q42 share the top row, in a smaller `--cm-sand-soft` text above a thin line.
* Source, Stimulus, Artifact, Environment, Response and Measure have the label on the left and the text over the other three columns.
* Verification and State share one row below a dashed line, and Value and Checks each take a full row.
  `grid-auto-flow: row dense` moves State up into the row of Verification.
* Labels are small uppercase `--cm-amber` text.
* Below 768 px the grid has two columns, and every field takes its own row.

## Constraints

* Every colour is a `--cm-*` variable from `palette.css`, and every rule that sets a text colour names its pair in a `/* contrast: --cm-x on --cm-y */` comment (`docs/ui/tests/test_dark_theme.py`).
* The rules match only the scenario sections, so no other definition list changes.
* The field order of a scenario is fixed by the docs test, so the `:nth-of-type` positions 1 to 12 are stable.
  A new field would need new positions, and the test would catch the new field first.

## Left to the builder

* The exact spacing values.
* Where in `site-extra.css` the rules go, as long as they sit in their own commented section.
