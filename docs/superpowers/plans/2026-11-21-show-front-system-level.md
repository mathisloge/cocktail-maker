# Show front on System level Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rewrite SYS-016 and its pages on System level, and add the contract IF-09 between the Station software and the show front hardware.

**Architecture:** Docs only: SYS-016 in place, IF-09 on the contracts page, cleanup of the pages that name show front parts.

**Tech Stack:** AsciiDoc, D2 through Kroki, pytest docs tests, the `writing-style`, `writing-adrs` and `writing-diagrams` skills.

**Spec:** `docs/superpowers/specs/2026-11-21-show-front-system-level-design.md`

## Global Constraints

* No System or Station page names a part, a count, a voltage of a part, a liquid or a Mechanics ADR of the show front.
* IF-09 follows the contract form that `contract_problems` checks: Sides, Kind `data`, Status `Partly decided`, the six sections, R01 to R04 with QG goals and `open` or xrefs in Met by.
* The chapter 5 diagram names IF-09 on an edge (`test_chapter5_diagram_names_every_contract`).
* Glossary and its plugin copy stay byte-identical.
* Diagrams stay at most 1400 px wide, checked through Kroki.
* Commit on the current branch, Conventional Commits, no trailer.

## Review Focus

* A leftover part name: `grep -rniE "propylene|glycol|WS2811|SK6812|air pump|LED line|show column|WLED|CM3D-030"` over `docs/system docs/station` finds nothing.
* IF-07-R04 and IF-09-R04 must not both claim the show front liquid.
* QS-21 must still name the show front as part of the full load.
* SYS-016 Quality Goals: every listed goal is assessed, and QG-05 is not used.
* The Station building block "Show Front Control" must point at IF-09 and still say it is not built.

---

### Task 1: SYS-016, IF-09 and the cleanup

**Files:**
- Modify: `docs/system/modules/adr/pages/016-station-show-front.adoc` (rewrite)
- Modify: `docs/system/modules/ROOT/pages/contracts.adoc` (IF-02 guarantees, IF-07-R04, new IF-09 at the end)
- Modify: `docs/system/modules/ROOT/pages/building-blocks.adoc`, `hardware.adoc`, `quality-requirements.adoc` (QS-21), `safety.adoc`
- Modify: `docs/system/modules/ROOT/partials/glossary.tsv`, `plugins/cocktail-maker-docs/skills/writing-style/glossary-system.tsv`
- Modify: `docs/station/modules/ROOT/pages/building-blocks.adoc`, `context-and-scope.adoc`, `risks-and-technical-debt.adoc`

- [ ] **Step 1: Prove the leftovers exist**

Run: `grep -rniE "propylene|glycol|WS2811|air pump|LED line|show column|WLED" docs/system docs/station`
Expected: hits in SYS-016, glossary, hardware, building blocks, contracts, QS-21, safety and the Station risks page.

- [ ] **Step 2: Rewrite SYS-016 and add IF-09 as the spec states, and clean the other pages**

- [ ] **Step 3: Verify**

Run the grep of Step 1 again: no hits.
Run: `uv run python -m pytest -q` (all pass), the style checker on every changed `.adoc`, and Kroki widths for the building block and hardware diagrams (at most 1400 px).

- [ ] **Step 4: Commit**

```bash
git commit -m "docs: state the show front on System level and add the contract IF-09"
```
