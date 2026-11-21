# Wide-screen layout and compact ADR header Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Centre the docs article on wide screens, scale the type above 1800 px, and show the ADR header as one line with a status chip.

**Architecture:** CSS in `docs/ui/supplemental/css/site-extra.css`, and a new body for both `adr-header.adoc` partials.

**Tech Stack:** CSS, AsciiDoc partials, pytest UI and docs tests.

**Spec:** `docs/superpowers/specs/2026-11-21-wide-screen-layout-design.md`

## Global Constraints

* Palette colours only, `/* contrast: --cm-x on --cm-y */` on every `color`.
* `docs/system/modules/adr/partials/adr-header.adoc` and `docs/station/modules/adr/partials/adr-header.adoc` stay byte-identical.
* Commit on the current branch, Conventional Commits, no trailer.

## Review Focus

* An ADR whose `adr-superseded-by` is unset must not print a raw `{adr-superseded-by}`.
* The status class must match the four statuses exactly: Proposed, Accepted, Deprecated, Superseded.
* Centring must not shift the page below 1024 px, where the bundle has no flex layout.
* The table of contents must keep its sticky position.
* No site test may still expect the old table or warning.

---

### Task 1: Layout, type and ADR header

**Files:**
- Modify: `docs/ui/supplemental/css/site-extra.css` (two new sections at the end)
- Modify: `docs/system/modules/adr/partials/adr-header.adoc`, `docs/station/modules/adr/partials/adr-header.adoc`
- Modify: `docs/ui/tests/test_dark_theme.py` (new CSS test, changed reference page test)
- Modify: `docs/tests/test_docs_structure.py` (new test that both partials are identical and hold the chip)
- Modify: `plugins/cocktail-maker-docs/skills/writing-adrs/SKILL.md:66`

- [ ] **Step 1: Write the failing tests**

`test_wide_screen_layout` in `test_dark_theme.py`: `site-extra.css` has `.content` with `justify-content: center`, `.doc` with `flex: 0 1 46rem`, `html` with `font-size: 1.1875em` in a context with `min-width:1800px` and `1.25em` with `min-width:2200px`, and rules for `.adr-status-Accepted`, `.adr-status-Proposed`, `.adr-status-Deprecated`, `.adr-status-Superseded`.
`test_adr_header_partials_match` in `test_docs_structure.py`: both partials are equal, contain `[.adr-meta]` and one chip per status, and contain no `|===`.
`test_reference_page_has_every_element` (site test): replace the table row and warning checks by an element with the class `adr-status-Superseded` and the text "Superseded", and an `adr-meta` element whose text holds "by SYS-003".

- [ ] **Step 2: Run them and watch them fail**

Run: `uv run python -m pytest docs/ui/tests docs/tests -q -k "wide_screen or adr_header_partials"`
Expected: 2 failures.

- [ ] **Step 3: Write the partial and the CSS as the spec describes, and update the skill sentence**

- [ ] **Step 4: Run all Python tests**

Run: `uv run python -m pytest -q`
Expected: all pass, site tests skip.

- [ ] **Step 5: Commit**

```bash
git add docs/ui docs/system/modules/adr/partials docs/station/modules/adr/partials docs/tests/test_docs_structure.py plugins/cocktail-maker-docs/skills/writing-adrs/SKILL.md docs/superpowers/specs/2026-11-21-wide-screen-layout-design.md docs/superpowers/plans/2026-11-21-wide-screen-layout.md
git commit -m "docs(ui): centre the page on wide screens and show the ADR header as one line"
```
