# System Runtime View Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Write System chapter 6 with four owner-confirmed normal scenarios and a "Fault Scenarios" subpage with five owner-confirmed fault scenarios, each tied to the contracts.

**Architecture:** Two AsciiDoc pages in a fixed scenario template, each scenario a D2 sequence diagram with Contracts, Open and Details sections. A reader in `docs/tests/test_docs_structure.py` checks the structure. Scenario content is drafted in the scratchpad and committed only after the owner confirms it.

**Tech Stack:** Antora 3, AsciiDoc, D2 sequence diagrams through Kroki, pytest, the `cocktail-maker-docs` plugin.

**Spec:** `docs/superpowers/specs/2026-11-15-runtime-view-design.md`

## Global Constraints

- **Use our skills:** before writing anything, every task agent loads `cocktail-maker-docs:architecture-docs`, `cocktail-maker-docs:writing-diagrams` and `cocktail-maker-docs:writing-style` with the Skill tool and follows them (owner: "use our skills for that").
- **Commit messages carry NO `Co-Authored-By:` trailer** (`CLAUDE.md`). Conventional messages.
- **Never stage** `hw_simulator/client.cppm`, `.claude/settings.json`, `.superpowers/` or `CLAUDE.md`.
- **No unconfirmed content:** a scenario, a step or a fact is committed only after the owner confirmed it.
- **Scenario order and anchors:**
  - `runtime-view.adoc`: `switching-on`, `drink-end-to-end`, `refilling`, `switching-off`
  - `fault-scenarios.adoc`: `link-breaks-during-pour`, `power-fails-during-pour`, `guest-cancels`, `bottle-empty-during-pour`, `pump-lid-opened`
- **Scenario template** (spec, "The scenario template"): `[[anchor]]`, `== Title`, one or two sentences on trigger and end state, a `.Title` line, exactly one D2 block with target `system-6-<anchor>` (the `<component>-<chapter>-<topic>` scheme of writing-diagrams) and `shape: sequence_diagram`, a caption line, then `=== Contracts`, `=== Open`, `=== Details` in this order.
- **Anchors** are lowercase kebab-case, never an ID-like anchor such as `IF-…`.
- **Open steps:** an open step keeps its edge kind class and ends its label with `(open, <ID>)` or `(open, <ID>, <ID>)`, each `<ID>` a defined `IF-nn-Rnn` or `IF-nn`, on one line of the label, in a quoted label. Every such ID appears as a whole token (`IF-01` is not `IF-01-R11`) in the scenario's Open section. Every Open bullet is one line and names a defined `IF-nn` or `IF-nn-Rnn`, or the section is exactly `Nothing open.`.
- **Diagram classes:** every participant has one shape class (`actor` or `block`), every edge one kind class, notes and groups use `boundary`, at most one `key` edge per diagram (the existing diagram tests enforce this on committed pages).
- **Cross-component xrefs** use a link text that names the component or an ID.
- **Writing and diagram rules:** the style checker reports 0 errors and 0 `glossary-term` warnings on `docs/system` and `docs/station`; diagrams at most 1400 px wide, no layout engine on a sequence diagram.
- **Plugin version** `0.6.0` → `0.7.0` in `plugins/cocktail-maker-docs/.claude-plugin/plugin.json` and `.claude-plugin/marketplace.json`.

## Review Focus

1. **A D2 target that does not follow `system-6-<anchor>`** (for example a Station-style `runtime-guest-mixes`): reported. Test in Task 1.
2. **An `(open, IF-01-R11)` label whose ID is missing from the Open section:** reported. Test in Task 1.
3. **An Open or Contracts entry with an undefined or stale requirement ID** (renumbered in project 2): reported. Test in Task 1.
4. **A scenario with two D2 blocks, or a D2 block that is not a sequence diagram:** reported. Test in Task 1.
5. **A fault scenario that claims a safe end state that no source or owner decision supports:** not testable; the checkpoint drafts mark every end state "from <source>" or "assumption", and the owner confirms it (Tasks 2 and 4).

---

## Shared commands

```bash
export PATH=<scratchpad>/node/bin:<scratchpad>/uvenv/bin:$PATH
uv run pytest -q
rm -rf <scratchpad>/site && npx antora --log-failure-level warn <scratchpad>/playbook-local.yml
DOCS_SITE_DIR=<scratchpad>/site uv run pytest docs/ui/tests docs/tests -q
python3 plugins/cocktail-maker-docs/scripts/check_adoc_style.py $(git ls-files 'docs/system/*.adoc' 'docs/station/*.adoc')
uv run pre-commit run --all-files
```

---

### Task 1: Pages, structure tests, skill and version

**Files:**
- Modify: `docs/system/modules/ROOT/pages/runtime-view.adoc` (drop the "Not written yet" note and remove `runtime-view.adoc` from `SYSTEM_PLACEHOLDERS` in the tests; a lead that says what the chapter shows and links `fault-scenarios.adoc` and `xref:station::runtime-view.adoc[Station runtime view]`)
- Create: `docs/system/modules/ROOT/pages/fault-scenarios.adoc` (`= Fault Scenarios` and a lead only)
- Modify: `docs/system/modules/ROOT/nav.adoc` (`** xref:fault-scenarios.adoc[]` directly under `* xref:runtime-view.adoc[]`)
- Modify: `docs/station/modules/ROOT/pages/runtime-view.adoc` (one line linking `xref:system::runtime-view.adoc[System runtime view]` and `xref:system::fault-scenarios.adoc[System fault scenarios]`; title unchanged)
- Modify: `plugins/cocktail-maker-docs/skills/architecture-docs/SKILL.md` (section "Runtime scenarios": the template and the rules of the spec) and `plugins/cocktail-maker-docs/skills/writing-diagrams/SKILL.md` (one sentence on the `(open, <ID>)` label form, in a quoted label)
- Modify: both manifests (0.7.0), `plugins/cocktail-maker-docs/tests/test_plugin_layout.py` (minimum `(0, 7, 0)`; the skill names `Contracts`, `Open`, `Details` and `shape: sequence_diagram` in its runtime section)
- Modify: `docs/tests/test_docs_structure.py`

**Interfaces:**
- **Consumes:** `SYSTEM_PAGES`, `STATION_PAGES`, `defined_ids`, `defined_requirement_ids`, `d2_blocks`, `site_test` (existing).
- **Produces** (in `test_docs_structure.py`):
  - `RUNTIME_PAGE`, `FAULT_PAGE` (Paths), `NORMAL_SCENARIOS`, `FAULT_SCENARIOS` (the anchor tuples from Global Constraints), `SCENARIO_SECTIONS = ("Contracts", "Open", "Details")`
  - `class Scenario(NamedTuple)`: `anchor: str`, `title: str`, `diagrams: tuple[tuple[str, str], ...]` (target, source text), `sections: tuple[str, ...]`, `contracts: tuple[str, ...]`, `open_lines: tuple[str, ...]`, `open_labels: tuple[str, ...]` (IDs from `(open, …)` labels)
  - `parse_scenarios(text: str) -> list[Scenario]`: splits at lines that are exactly `[[<kebab-case>]]` followed by a `== ` heading; reads the D2 blocks from text (a text variant of `d2_blocks`) and the target from the block's attribute line; Contracts and Open bullets are one line each
  - `scenario_problems(text: str, expected: tuple[str, ...]) -> list[str]`

`scenario_problems` reports: anchors that are not an ordered prefix of `expected`; not exactly one D2 block; a target other than `system-6-<anchor>`; a block without `shape: sequence_diagram`; sections not equal to `SCENARIO_SECTIONS`; a Contracts list with no ID or an undefined ID; an Open bullet without a defined ID unless the section is exactly `Nothing open.`; an `(open, …)` ID that is undefined or not a whole token of the Open section.

- [ ] **Step 1: Write the failing tests**

```python
def test_scenario_problems_reader(): ...
    # valid inline page → []; page with lead only → []
    # anchor out of order / unknown anchor → reported
    # two d2 blocks; a non-sequence block; target "runtime-guest-mixes" → reported; "system-6-<anchor>" → accepted
    # "Open" before "Contracts" / missing "Details" → reported
    # Contracts with "IF-09" or "IF-01-R12" (undefined) → reported; Contracts empty → reported
    # Open bullet "something" (no ID) → reported; "Nothing open." alone → accepted
    # label "(open, IF-01-R11)" with IF-01-R11 missing from Open → reported
    # label "(open, IF-01)" with only IF-01-R11 in Open → reported; "(open, IF-01-R11, IF-03)" with both in Open → accepted
def test_runtime_pages_are_valid(): ...   # scenario_problems on both real pages → []
def test_fault_scenarios_in_nav_under_chapter_6(): ...
def test_runtime_pages_link_each_other(): ...  # chapter 6 links fault-scenarios.adoc; Station runtime view links system::runtime-view.adoc and system::fault-scenarios.adoc
```

- [ ] **Step 2: Run them and see them fail**
- [ ] **Step 3: Implement the reader, the pages, the nav entry, the back-link, the skill section and the version bump**
- [ ] **Step 4: Build clean, run all tests with the site, the style check and pre-commit**
- [ ] **Step 5: Commit** `feat(docs): add the runtime scenario pages and their structure tests`

---

### Task 2: OWNER CHECKPOINT 1, the normal scenarios (no commit)

**Files (scratchpad only):** `<scratchpad>/runtime/switching-on.adoc`, `drink-end-to-end.adoc`, `refilling.adoc`, `switching-off.adoc`, `<scratchpad>/runtime/questions.md`

**Sources:** `contracts.adoc`, `protocol.adoc`, `power.adoc`, `safety.adoc`, `hardware.adoc`, the System and Station ADRs, `docs/station/modules/ROOT/pages/runtime-view.adoc`, the Station code in `cocktail-maker/`, and the Mechanics docs in `<scratchpad>/3d` at `origin/main`.

- [ ] **Step 1: Write the four scenarios in the template.** Each step and the end state is from a source or marked as an assumption in `questions.md`.
- [ ] **Step 2: Validate** on a temporary copy of `runtime-view.adoc`: `scenario_problems(text, NORMAL_SCENARIOS)` is `[]`; on each block `d2_edges`/`d2_edge_has_a_kind`, `d2_shapes`/`d2_shape_has_a_class` and the one-key-edge rule pass; the style checker reports 0 errors; every diagram compiles on kroki.io with the text of `docs/system/modules/ROOT/partials/d2-style.d2` in place of the include line, and its SVG viewBox width is at most 1400.
- [ ] **Step 3: Write `questions.md`:** per scenario every step as "from <source>" or "assumption", then the questions for the owner with a suggested answer each, most important first.
- [ ] **Step 4: STOP.** Report the paths and the questions. Make no commit.

---

### Task 3: Apply the normal scenarios

**Input:** the owner-confirmed drafts of Task 2.

**Files:** `docs/system/modules/ROOT/pages/runtime-view.adoc`, `docs/tests/test_docs_structure.py`

- [ ] **Step 1: Write `test_runtime_view_is_complete()`:** the anchors of `RUNTIME_PAGE` equal `NORMAL_SCENARIOS`. See it fail.
- [ ] **Step 2: Apply the confirmed scenarios unchanged, and see the test pass**
- [ ] **Step 3: Build clean, run all tests with the site (the width test), the style check and pre-commit**
- [ ] **Step 4: Commit** `docs: add the normal runtime scenarios of the machine`

---

### Task 4: OWNER CHECKPOINT 2, the fault scenarios (no commit)

Like Task 2, for `link-breaks-during-pour`, `power-fails-during-pour`, `guest-cancels`, `bottle-empty-during-pour`, `pump-lid-opened`, validated with `scenario_problems(text, FAULT_SCENARIOS)` on a temporary copy of `fault-scenarios.adoc`. Each fault scenario's first sentence names its end state or says that it is open. The pump lid scenario shows the residual risk stated in IF-08.

- [ ] **Step 1: Write the five scenarios**
- [ ] **Step 2: Validate**
- [ ] **Step 3: Write `<scratchpad>/runtime/questions-faults.md`**
- [ ] **Step 4: STOP.** Report the paths and the questions. Make no commit.

---

### Task 5: Apply the fault scenarios

Like Task 3, with `test_fault_scenarios_are_complete()` for `FAULT_PAGE` and `FAULT_SCENARIOS`.

- [ ] **Step 1: Write the completeness test and see it fail**
- [ ] **Step 2: Apply the confirmed scenarios unchanged, and see the test pass**
- [ ] **Step 3: Build clean, run all tests with the site, the style check and pre-commit**
- [ ] **Step 4: Commit** `docs: add the fault scenarios of the machine`

---

After Task 5: a final review of the whole branch, its fix wave, then the squash of all commits since `main` into one commit and a local fast-forward merge into `main` (no trailer).
