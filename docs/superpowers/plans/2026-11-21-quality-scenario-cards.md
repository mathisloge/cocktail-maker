# Quality scenario cards Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show each quality scenario of the System page "10 Quality Requirements" as a compact card, with CSS only.

**Architecture:** One new commented section in `docs/ui/supplemental/css/site-extra.css` styles every `.sect2` whose `h3` id starts with `QS-` and lays out its definition list as a four-column grid.
The AsciiDoc source and the docs tests of the scenarios stay as they are.

**Tech Stack:** CSS (`:has`, grid, `:nth-of-type`), pytest UI tests in `docs/ui/tests`.

**Spec:** `docs/superpowers/specs/2026-11-21-quality-scenario-cards-design.md`

## Global Constraints

* Colours only as `var(--cm-*)` from `docs/ui/supplemental/css/palette.css`, no hex, no colour functions.
* Every rule that sets `color` carries `/* contrast: --cm-<text> on --cm-<background> */`.
* Selectors start with `.doc .sect2:has(> h3[id^="QS-"])`, so no other definition list changes.
* Field positions 1 to 12 follow `QS_FIELDS` in `docs/tests/test_docs_structure.py`: Goal, Q42, Source, Stimulus, Artifact, Environment, Response, Measure, Verification, Value, State, Checks.
* Below 768 px the grid has two columns.
* Commit on the current branch, Conventional Commits, no trailer.

## Review Focus

* A long Value (QS-16) must not be squeezed into a quarter column: Value and Checks span columns 2 to 4.
* State must sit in the row of Verification and not leave a gap (`grid-auto-flow: row dense`).
* At phone width no field keeps a `grid-column` that forces a horizontal scroll.
* The rules must not touch the ADR header table, the glossary or any other `dl`.
* The anchor link of the `h3` heading must stay usable inside the card band.

---

### Task 1: Card styles for the quality scenarios

**Files:**
- Modify: `docs/ui/supplemental/css/site-extra.css` (new section at the end, headed `/* --- quality scenarios ---- */`)
- Test: `docs/ui/tests/test_dark_theme.py` (new test)

- [ ] **Step 1: Write the failing test**

```python
def test_quality_scenarios_are_cards():
    css = (CSS_DIR / "site-extra.css").read_text(encoding="utf-8")
    card = '.doc .sect2:has(> h3[id^="QS-"])'
    rules = [rule for rule in parse_css(css) if any(s.startswith(card) for s in rule.selectors)]
    declarations = {(prop, value) for rule in rules for prop, value in rule.declarations}
    assert ("grid-template-columns", "max-content 1fr max-content 1fr") in declarations
    assert ("grid-auto-flow", "row dense") in declarations
    assert ("border-left", "4px solid var(--cm-amber)") in declarations
    assert any(prop == "grid-column" and value == "2 / -1" for prop, value in declarations)
    assert all(s.startswith(card) or s.startswith("@") for rule in rules for s in rule.selectors)
```

Use the module's existing `parse_css` and `CSS_DIR`. If `parse_css` does not descend into `@media`, the last assertion only covers top-level rules, which is fine.

- [ ] **Step 2: Run it and watch it fail**

Run: `uv run python -m pytest docs/ui/tests/test_dark_theme.py -q -k quality_scenarios`
Expected: FAIL on the first assertion.

- [ ] **Step 3: Add the CSS section**

Port the rules of the mockup `a-css.html` (in the session scratchpad) between its `the proposed rules` markers into `site-extra.css`, as the spec section "Design" describes.

- [ ] **Step 4: Run the UI and docs tests**

Run: `uv run python -m pytest docs/ui/tests docs/tests -q`
Expected: all pass, the site tests skip without `DOCS_SITE_DIR`.

- [ ] **Step 5: Commit**

```bash
git add docs/ui/supplemental/css/site-extra.css docs/ui/tests/test_dark_theme.py docs/superpowers/specs/2026-11-21-quality-scenario-cards-design.md docs/superpowers/plans/2026-11-21-quality-scenario-cards.md
git commit -m "docs(ui): show the quality scenarios as cards"
```
