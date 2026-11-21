# Plastic Pod bottles and the Station show front Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Record in the System docs that Pod bottles may be food-contact HDPE or PP, and that the Station owns a show front.

**Architecture:** Two new System ADRs: SYS-015 supersedes SYS-009, and SYS-016 is new.
The requirements and diagrams that cite them follow.
No code changes.

**Tech Stack:** Antora/AsciiDoc, D2 diagrams through Kroki, pytest docs tests, the `cocktail-maker-docs` plugin skills.

**Spec:** `docs/superpowers/specs/2026-11-21-pod-bottles-and-show-front-design.md`

## Global Constraints

* Load the skills `cocktail-maker-docs:writing-style` and `cocktail-maker-docs:writing-adrs` before writing, and `cocktail-maker-docs:writing-diagrams` before Task 3.
* Every prose line holds one sentence, with no semicolons and no dash breaks.
* Style check for each changed `.adoc` file: `python3 /home/mathi/.claude/plugins/cache/cocktail-maker/cocktail-maker-docs/0.8.0/scripts/check_adoc_style.py <file>` reports no errors.
* Docs tests: `uv run pytest docs/tests -q` passes (the site tests skip without `DOCS_SITE_DIR`).
* Terms: "bottle" for the container of an ingredient in a Pod, whatever its material, "show front" and "show column" for the new Station parts.
* New ADRs use `docs/system/modules/adr/pages/014-quality-model-q42.adoc` as the header pattern: `:adr-status: Accepted`, `:adr-date: 2026-11-21`, `:adr-authors: {gh}mathisloge[@mathisloge]`.
* EU 10/2011 values, verified in the consolidated EUR-Lex PDF of 14.07.2026: D1 for alcoholic foods above 20 %, C for 6 to 20 % vol, more than 1 day and up to 3 days of contact tested for 3 days, 20 °C to 40 °C tested at 40 °C, OM2 is 10 d at 40 °C.
* QG-02 and SYS-013 do not change.
* Commits on the current branch, Conventional Commits, no `Co-Authored-By` trailer.
* Never stage `CMakePresets.json`, `.claude/settings.json` or `hw_simulator/client.cppm` (unrelated local changes).

## Review Focus

* A page still cites SYS-009 as the source of a rule: `grep -rn "009-wetted-parts" docs/system docs/station` must list only nav, chapter 9, SYS-015 and SYS-009 itself (Task 2, Step 4).
* The plastic rule loses its conditions somewhere: QS-09, IF-05-R01 and IF-06-R01 must all name the declaration under EU 10/2011, not only "HDPE or PP" (Task 2, Step 4).
* The contract requirement numbers in IF-05 must stay gapless (R07, R08 follow R06), or the contract test fails (Task 2, Step 5).
* A D2 edge without a class or a part without a part class breaks the diagram tests (Task 3, Step 5).
* "Bottle" must not turn into "container" or "reservoir" on the changed pages (Tasks 1 to 3, style check plus `grep -n -i "container\|reservoir"` on the changed files).

---

### Task 1: SYS-015 food-contact plastic bottles, superseding SYS-009

**Files:**
- Create: `docs/system/modules/adr/pages/015-food-contact-plastic-bottles.adoc`
- Modify: `docs/system/modules/adr/pages/009-wetted-parts.adoc:4-8`
- Modify: `docs/system/modules/ROOT/nav.adoc:32` (add after SYS-014)
- Modify: `docs/system/modules/ROOT/pages/architecture-decisions.adoc:19` (add after SYS-014)

**Interfaces:**
- Produces: `xref:adr:015-food-contact-plastic-bottles.adoc[SYS-015]`, used by Task 2.

- [ ] **Step 1: Write SYS-015 from the ADR template**

Title: `= SYS-015: Food-contact HDPE and PP bottles may hold ingredients`.
Context: the Mechanics round (8 hidden 2 l square bottles, Pod about 880 × 340 × 520 mm instead of 880 × 670 × 570 mm, twice the volume, no breakage when lifted out), and that SYS-009 named only tubes and stainless steel parts, so it did not even cover glass.
Name and link SYS-009 as superseded.
Decision drivers: QG-02 (food-safe parts only), QG-04 (an Operator lifts a bottle out without breaking it), QG-06 kept from SYS-009 (printed parts stay free).
Decision, as the spec section "SYS-015" states it, with these sentences verbatim:

```text
These parts may touch an ingredient or a drink: food-grade tubes, stainless steel parts, glass, and HDPE or PP.
HDPE and PP are allowed for bottles, caps and fittings, and not for tubes.
An HDPE or PP part needs a declaration of compliance under EU 1935/2004 and EU 10/2011.
The declaration covers the food simulants A, B, C and D1, or all food types, at least 3 days at 40 °C, and repeated use.
No 3D printed part touches a liquid.
An ingredient stays in a bottle for at most 48 h, and then the Operator empties and rinses the bottle.
Each bottle holds one ingredient.
```

Alternatives: glass bottle sets as hidden bottles, larger glass bottles, polycarbonate, PET, bag-in-box, HDPE without a time limit, bottles with an outlet at the bottom.
Use the reasons of the spec and the Mechanics input.
Consequences must include these bad ones: the declaration is missing for every candidate today, HDPE takes up aromas such as anethole and limonene, the 48 h limit adds a step at the end of each event day, and spirits above 50 % vol are covered by no release of the tube or the bottle (small at 48 h).
In "Effect on the Quality Goals", rate each listed goal.

- [ ] **Step 2: Supersede SYS-009**

Set `:adr-status: Superseded` and `:adr-superseded-by: xref:015-food-contact-plastic-bottles.adoc[SYS-015]`.
Leave the rest of the page as it is.

- [ ] **Step 3: Add SYS-015 to nav and chapter 9**

`** xref:adr:015-food-contact-plastic-bottles.adoc[]` in `nav.adoc`, `* xref:adr:015-food-contact-plastic-bottles.adoc[]` in `architecture-decisions.adoc`.

- [ ] **Step 4: Check**

Run the style check on the two ADR files, then `uv run pytest docs/tests -q -k "adr or chapter_9"`.
Expected: no style errors, tests pass.

- [ ] **Step 5: Commit**

```bash
git add docs/system/modules/adr/pages/015-food-contact-plastic-bottles.adoc docs/system/modules/adr/pages/009-wetted-parts.adoc docs/system/modules/ROOT/nav.adoc docs/system/modules/ROOT/pages/architecture-decisions.adoc
git commit -m "docs: allow food-contact HDPE and PP bottles in the Pod (SYS-015)"
```

### Task 2: Requirements follow SYS-015

**Files:**
- Modify: `docs/system/modules/ROOT/pages/quality-requirements.adoc` (QS-09 block and the QS-09 node label `q09`)
- Modify: `docs/system/modules/ROOT/pages/contracts.adoc` (IF-05 guarantees, requirements, open questions, sources, and IF-06 guarantees, R01, sources)

**Interfaces:**
- Consumes: SYS-015 from Task 1.

- [ ] **Step 1: Rewrite QS-09**

Title: `=== QS-09: Only food-safe parts touch a drink`.
Response: `Each part that touches the ingredient or the drink is a food-grade tube, a stainless steel part, glass, or an HDPE or PP bottle, cap or fitting with a declaration of compliance as SYS-015 requires, and none is a printed part.`
Measure: count the parts that break this rule, and the scenario holds at 0.
Checks: IF-05-R01, IF-06-R01, SYS-015.
Keep the D2 node label short, for example `"QS-09 Only food-safe wetted parts"` (unchanged).

- [ ] **Step 2: Update IF-05**

Guarantees line: replace the SYS-009 bullet with `Only food-grade tubes, stainless steel parts, glass and declared HDPE or PP bottles, caps and fittings touch a liquid, and no printed part does (SYS-015).`
Requirements:

```text
IF-05-R01 | Only the parts that SYS-015 allows touch an ingredient between the bottle and the pump, and no printed part does. | QG-02 | SYS-015
IF-05-R07 | An ingredient stays in a bottle for at most 48 h, and then the Operator empties and rinses the bottle. | QG-02, QG-04 | SYS-015
IF-05-R08 | Each bottle holds only one ingredient, and an Operator replaces a bottle that smells or is worn. | QG-02 | SYS-015
```

Open questions: remove the barbed fittings question (SYS-015 answers it).
Change the bottle question to `Which HDPE or PP bottle has a declaration of compliance as SYS-015 requires, and how does it sit on its load cell?`
Add `Which release covers spirits above 50 % vol, since simulant D1 of the tube and the bottle is 50 % ethanol?`
Sources: SYS-009 becomes SYS-015.

- [ ] **Step 3: Update IF-06**

The guarantees bullet and IF-06-R01 name the SYS-015 parts instead of "tubes and stainless steel parts", and point at SYS-015.
Sources: SYS-009 becomes SYS-015.

- [ ] **Step 4: Check that no rule still rests on SYS-009**

Run: `grep -rn "009-wetted-parts\|SYS-009" docs/system docs/station`
Expected: only `nav.adoc`, `architecture-decisions.adoc`, `009-wetted-parts.adoc` and the Context of `015-food-contact-plastic-bottles.adoc`.
Run: `grep -n "10/2011\|SYS-015" docs/system/modules/ROOT/pages/contracts.adoc docs/system/modules/ROOT/pages/quality-requirements.adoc`
Expected: hits in QS-09, IF-05 and IF-06.

- [ ] **Step 5: Style check and tests**

Run the style check on both pages, then `uv run pytest docs/tests -q`.
Expected: no style errors, tests pass (the contract tests check the gapless R numbers).

- [ ] **Step 6: Commit**

```bash
git add docs/system/modules/ROOT/pages/quality-requirements.adoc docs/system/modules/ROOT/pages/contracts.adoc
git commit -m "docs: point the wetted part requirements at SYS-015"
```

### Task 3: SYS-016 the Station owns the show front

**Files:**
- Create: `docs/system/modules/adr/pages/016-station-show-front.adoc`
- Modify: `docs/system/modules/ROOT/nav.adoc`, `docs/system/modules/ROOT/pages/architecture-decisions.adoc` (add after SYS-015)
- Modify: `docs/system/modules/ROOT/partials/glossary.tsv` (two rows)
- Modify: `docs/system/modules/ROOT/pages/building-blocks.adoc` (diagram and Station bullet)
- Modify: `docs/system/modules/ROOT/pages/hardware.adoc` (Station boundary of the diagram)
- Modify: `docs/system/modules/ROOT/pages/contracts.adoc` (IF-07-R04)
- Modify: `docs/station/modules/ROOT/pages/risks-and-technical-debt.adoc` (Open Points)

- [ ] **Step 1: Write SYS-016 from the ADR template**

Title: `= SYS-016: The Station owns the show front`.
Context: the Mechanics round put 8 bubbling acrylic columns on each Pod, and the user moved them to the Station so that Pods stand hidden anywhere and the Station looks the same with any number of Pods.
Decision drivers: QG-05 (a Guest sees which ingredient pours), QG-03 (a show front fault never stops a drink), QG-06 (no protocol change).
Decision, as the spec section "SYS-016" states it: 8 show columns with water and 30 to 40 % propylene glycol, one 24 V air pump and one WS2811 LED line (24 V) per column, the Station alone drives them from its own events (pour started, pour finished, ingredient, recipe, idle, fault), a rule in the Station software maps events to a column and an effect, no Pod knows about it, the protocol does not change, a failed show front never stops or delays a drink, its effects never block the Station execution context (`xref:station:adr:003-asynchronous-execution-model.adoc[STA-003]`), and the liquid of a show column never reaches the Raspberry Pi, the display or the power supply and never touches the flow path of a drink.
Open in the Consequences: the controller (WLED controller on a wired link as the favourite, own controller, or drivers on the Raspberry Pi), the animations and what a show column stands for.
Alternative: show columns, air pumps and LED lines on each Pod on the free outputs of the Octopus, rejected because the LED lines cross IF-07 and the look depends on the number of Pods.
Bad consequences: one more controller and a second liquid in the Station, and the effect lags the pour by the USB round trip.
Add the ADR to nav and chapter 9 as in Task 1, Step 3.

- [ ] **Step 2: Add the glossary rows**

```text
Show front	The visible front of the Station with the show columns, which shows the pours to the Guests and never touches a drink.	System
Show column	An acrylic column of the show front, filled with water and propylene glycol, that bubbles and lights up while the Station pours.	System		show bottle;bubble column
```

Keep the tabs of the empty cells.

- [ ] **Step 3: Add the show front to the diagrams**

`building-blocks.adoc`: a shape `show_front` inside `machine` with `{class: block}`, an edge `machine.station -> machine.show_front: "drives the effects\nfrom its pour events" {class: data}`, and an edge `machine.show_front -> guest: "shows the pours" {class: interaction}`.
Add a bullet **Show front** under the block list, linking SYS-016.
`hardware.adoc`: inside the Station boundary a part `show_front: "Show Front · 8 show columns, air pumps, LED lines" {class: mechanical-part}` and a part `show_controller: "Show Controller · open, see SYS-016" {class: data-part}`, with `rpi -> show_controller` (data) and `show_controller -> show_front` (power), and `psu -> show_controller` (power).
Follow `writing-diagrams` for labels.

- [ ] **Step 4: Widen IF-07-R04 and add the Station open point**

IF-07-R04: `Liquid from a Pod, from the fluid path or from a show column never reaches the electronics of the Pod or the Station.`
Station risks page, under Open Points:

```text
* *Show front leak protection:* the show columns hold water and propylene glycol next to the Raspberry Pi, the display and the power supply (SYS-016).
How a leak is kept away from them is not designed yet.
```

- [ ] **Step 5: Style check and tests**

Run the style check on every file of this task, then `uv run pytest docs/tests -q`.
Expected: no style errors, tests pass, including the D2 class tests.

- [ ] **Step 6: Commit**

```bash
git add docs/system/modules/adr/pages/016-station-show-front.adoc docs/system/modules/ROOT/nav.adoc docs/system/modules/ROOT/pages/architecture-decisions.adoc docs/system/modules/ROOT/partials/glossary.tsv docs/system/modules/ROOT/pages/building-blocks.adoc docs/system/modules/ROOT/pages/hardware.adoc docs/system/modules/ROOT/pages/contracts.adoc docs/station/modules/ROOT/pages/risks-and-technical-debt.adoc
git commit -m "docs: add the Station show front (SYS-016)"
```

### Task 4: Final check

- [ ] **Step 1:** `uv run pre-commit run --files <every changed file>` passes (formatting only, no retest needed after it).
- [ ] **Step 2:** If network access is available, build the site (`npm install && npx antora antora-playbook.yml`) and run `DOCS_SITE_DIR=build/site uv run pytest docs/tests -q`.
Expected: all site tests pass, and SYS-009 shows the superseded warning.
- [ ] **Step 3:** Commit the spec and this plan.

```bash
git add docs/superpowers/specs/2026-11-21-pod-bottles-and-show-front-design.md docs/superpowers/plans/2026-11-21-pod-bottles-and-show-front.md
git commit -m "docs: add the design and plan for plastic Pod bottles and the show front"
```
