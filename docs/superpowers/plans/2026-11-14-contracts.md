# Contracts in System Chapter 5 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give every technical boundary of System chapter 5 an owner-confirmed contract with an `IF-nn` ID, requirements with `IF-nn-Rnn` IDs, open questions and sources, on one Contracts page.

**Architecture:** One AsciiDoc page `contracts.adoc` under chapter 5 holds all contracts in a fixed template. Structure tests in `docs/tests/test_docs_structure.py` parse the page. Contract content is drafted by agents in the scratchpad, confirmed by the owner at two checkpoints, and only then committed.

**Tech Stack:** Antora 3, AsciiDoc, D2 through Kroki, pytest, the `cocktail-maker-docs` plugin (style checker, skills).

**Spec:** `docs/superpowers/specs/2026-11-14-contracts-design.md`

## Global Constraints

- **Commit messages carry NO `Co-Authored-By:` trailer** (`CLAUDE.md`). Conventional messages, checked by commitizen.
- **Never stage** `hw_simulator/client.cppm`, `.claude/settings.json`, `.superpowers/` or `CLAUDE.md`.
- **IDs:** `IF-nn` and `IF-nn-Rnn`, two digits, zero-padded, never changed or reused, defined only on `docs/system/modules/ROOT/pages/contracts.adoc`.
- **Contract template** (spec, "The contract template"): an anchor `[[IF-nn]]`, a title `== IF-nn: <Title>`, a definition list `Sides::`, `Kind::`, `Status::`, then exactly these level-3 sections in this order: `What crosses`, `Guarantees and limits`, `Fault behaviour`, `Requirements`, `Open questions`, `Sources`.
- **Kind** is one of `data`, `power`, `fluid`, `mechanical`. **Status** is one of `Decided`, `Partly decided`, `Not designed yet`.
- **Requirement table:** `[cols="2,6,2,3", options="header"]`, header `| ID | Requirement | Goals | Met by`, one row per line, no `|` inside a cell, each row starts with `| [[IF-nn-Rnn]]IF-nn-Rnn`.
- **Goals:** at least one defined `QG-nn` per requirement. **Met by:** `open` or an xref; an xref to a System or Station page must name an existing file, an xref to `mechanics::` or `embedded::` is accepted without a file check.
- **A requirement states an outcome, never a mechanism.**
- **No unconfirmed content:** a contract, a requirement or a fact is committed only after the owner confirmed it.
- **Writing rules:** prose follows `writing-style`, the style checker reports 0 errors and 0 `glossary-term` warnings on `docs/system` and `docs/station`. Glossary terms apply.
- **Diagrams** follow `writing-diagrams` and stay at most 1400 px wide (`MAX_DIAGRAM_WIDTH`).
- **Plugin version** goes from `0.5.0` to `0.6.0` in `plugins/cocktail-maker-docs/.claude-plugin/plugin.json` and `.claude-plugin/marketplace.json`.

## Review Focus

1. **A copied requirement row keeps the old contract prefix** (`IF-05-R01` inside IF-06): must be reported. Test in Task 1.
2. **A requirement with no goal or an undefined goal** (`QG-07`): must be reported. Test in Task 1.
3. **A "Met by" xref to a page that does not exist** in System or Station: must be reported, while `xref:mechanics::pump.adoc[...]` passes. Test in Task 1.
4. **A contract with a missing, renamed or reordered section, or a Status outside the three values:** must be reported. Test in Task 1.
5. **A contract that is defined but missing from the chapter 5 diagram,** or a diagram that grows past 1400 px with the new glass node: the diagram test and the existing width test must catch it. Test in Task 4.

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

### Task 1: Contracts page skeleton and the structure tests

**Files:**
- Create: `docs/system/modules/ROOT/pages/contracts.adoc` (title `= Contracts`, a short lead, no contract yet)
- Modify: `docs/system/modules/ROOT/nav.adoc` (`** xref:contracts.adoc[]` after `** xref:hardware.adoc[]`)
- Modify: `docs/system/modules/ROOT/pages/building-blocks.adoc` (one sentence linking the Contracts page)
- Modify: `docs/tests/test_docs_structure.py` (`ID_PAGES["IF"] = "contracts.adoc"`, the reader and the tests below)
- Create: `docs/tests/fixtures/contracts/` (fixture pages: one valid, one per broken case)

**Interfaces:**
- **Consumes:** `defined_ids(prefix)`, `ID_PAGES`, `SYSTEM_PAGES`, `STATION_PAGES`, `site_test` (existing).
- **Produces** (in `test_docs_structure.py`):
  - `CONTRACTS_PAGE = SYSTEM_PAGES / "contracts.adoc"`
  - `CONTRACT_SECTIONS = ("What crosses", "Guarantees and limits", "Fault behaviour", "Requirements", "Open questions", "Sources")`
  - `class Requirement(NamedTuple)`: `id: str`, `text: str`, `goals: tuple[str, ...]`, `met_by: str`
  - `class Contract(NamedTuple)`: `id: str`, `title: str`, `kind: str`, `status: str`, `sections: tuple[str, ...]`, `requirements: tuple[Requirement, ...]`
  - `parse_contracts(text: str) -> list[Contract]`
  - `xref_page_exists(xref: str) -> bool | None` (None for `mechanics::` and `embedded::` targets)
  - `contract_problems(text: str) -> list[str]`, readable messages, `[]` for a valid page

- [ ] **Step 1: Write the failing tests**

```python
def test_contract_problems_reader(): ...
    # valid fixture → []
    # missing "Fault behaviour" / sections reordered → reported
    # Status "Planned", Kind "optical" → reported
    # IF-05-R01 inside IF-06 → reported; duplicate requirement ID → reported
    # requirement without goal, with QG-07 → reported
    # Met by "xref:missing-page.adoc[]" → reported; "xref:mechanics::pump.adoc[Pump]" and "open" → accepted
def test_contracts_page_is_valid(): assert contract_problems(CONTRACTS_PAGE.read_text()) == []
def test_contracts_page_is_in_nav_after_hardware(): ...
def test_building_blocks_links_contracts(): ...
def test_chapter5_diagram_names_every_contract(): ...  # every defined_ids("IF") appears in the conceptual-blocks source of building-blocks.adoc
@site_test
def test_contracts_page_renders(): ...  # system/contracts.html exists; every defined IF id has id="IF-nn"
```

The existing ID tests run unchanged against the new `ID_PAGES["IF"]`.

- [ ] **Step 2: Run them and see them fail**

- [ ] **Step 3: Implement the reader, the page skeleton, the nav entry and the link**

`parse_contracts` splits the page at `[[IF-nn]]` anchors; the requirement rows are the lines starting with `| [[IF-`. A contract-free page is valid.

- [ ] **Step 4: Build clean, run all tests, the style check and pre-commit**

- [ ] **Step 5: Commit** `feat(docs): add the contracts page and its structure tests`

---

### Task 2: The skill section, the handoff line and the plugin version

**Files:**
- Modify: `plugins/cocktail-maker-docs/skills/architecture-docs/SKILL.md` (a section "Contracts"; the "Stable IDs" table names the Contracts page for `IF-nn` and adds `IF-nn-Rnn`)
- Modify: `docs/superpowers/handoffs/2026-11-13-mechanics-docs-prompt.md` (one line in the nav step: the Mechanics chapter 5 links the contracts that touch its parts, with `xref:system::contracts.adoc#IF-05[IF-05]`; the minimum plugin version becomes 0.6.0)
- Modify: `plugins/cocktail-maker-docs/.claude-plugin/plugin.json`, `.claude-plugin/marketplace.json` (0.6.0)
- Modify: `plugins/cocktail-maker-docs/tests/test_plugin_layout.py`

The section holds the template from Global Constraints, the rule that a requirement states an outcome with at least one goal, how a decision changes a contract (an ADR, then "Met by" and the guarantees, never a reused ID), and that subprojects only link contracts.

- [ ] **Step 1: Write the failing test** `test_architecture_skill_has_the_contract_template()`: the skill names all six section titles of `CONTRACT_SECTIONS` in order and the three Status values; the two manifests carry `0.6.0`.
- [ ] **Step 2: Run it and see it fail**
- [ ] **Step 3: Write the section, the handoff line and the version bump**
- [ ] **Step 4: Run all tests, the style check on the skill and the handoff, and pre-commit**
- [ ] **Step 5: Commit** `docs: describe contracts in the architecture skill`

---

### Task 3: OWNER CHECKPOINT 1, the inventory and the IF-01 sample (no commit)

**Files (scratchpad only):** `<scratchpad>/contracts/inventory.md`, `<scratchpad>/contracts/IF-01.adoc`

**Sources for IF-01:**
- `docs/system/modules/ROOT/pages/protocol.adoc`, ADRs SYS-002, SYS-003, SYS-006, STA-003, STA-005
- the schema in `build/_deps/cocktail-maker-protocol-src`
- the Pod session and timeout code in `cocktail-maker/` (grep for the timeout per Pod exchange)

**The draft:**
- `inventory.md`: the eight contracts of the spec's inventory table, one line each on the boundary, and any proposal to add, merge or split, with its reason
- `IF-01.adoc`: the full contract in the template, every fact with its source, every requirement marked as "from <source>" or "proposal"; a list of questions for the owner at the end of `inventory.md`
- Insert `IF-01.adoc` into a temporary copy of `contracts.adoc` in the scratchpad: `contract_problems` reports `[]`, the style checker 0 errors

- [ ] **Step 1: Write the draft**
- [ ] **Step 2: Validate it on the temporary copy**
- [ ] **Step 3: STOP.** Report the paths and the questions. Make no commit.

---

### Task 4: Apply IF-01 and the inventory to the page and the diagram

**Input:** the owner-confirmed inventory and IF-01.

**Files:**
- Modify: `docs/system/modules/ROOT/pages/contracts.adoc` (IF-01 as confirmed)
- Modify: `docs/system/modules/ROOT/pages/building-blocks.adoc` (`conceptual-blocks`: each covered arrow's label gets its `IF-nn` of the confirmed inventory; a glass node and the IF-06 and IF-07 edges, with classes from `writing-diagrams`)

The diagram test of Task 1 covers only defined IDs, so it checks IF-01 now. The labels for IF-02 to IF-08 go in now, because the inventory is confirmed.

- [ ] **Step 1: Apply the confirmed IF-01 and the diagram labels and edges**
- [ ] **Step 2: Render the diagram and check its width is at most 1400 px and every label fits**
- [ ] **Step 3: Build clean, run all tests, the style check and pre-commit**
- [ ] **Step 4: Commit** `docs: add the Station to Pod data contract and the contract IDs in chapter 5`

---

### Task 5: OWNER CHECKPOINT 2, IF-02 to IF-08 (no commit)

**Files (scratchpad only):** `<scratchpad>/contracts/IF-02.adoc` … `IF-08.adoc`, `<scratchpad>/contracts/questions.md`

**Sources:**
- Power (IF-02, IF-03, IF-04): `power.adoc`, `hardware.adoc`, `safety.adoc`, SYS-004
- Fluid (IF-05, IF-06): `hardware.adoc`, `safety.adoc`, SYS-007, the glossary, and from `cocktail-maker-3d` (fetch `origin/main` into `<scratchpad>/3d`): `pump.adoc`, `pump-service.adoc`, CM3D-003, CM3D-011, `docs/open-points.md`
- Mechanical (IF-07, IF-08): `hardware.adoc`, `safety.adoc`, SYS-004 (main switch), and from `cocktail-maker-3d`: `structure.adoc`, `pod-electronics.adoc`, `pump-service.adoc`, `docs/open-points.md`

**The draft:** like Task 3, for each contract. Undecided boundaries get `Status:: Not designed yet` or `Partly decided`, requirements as outcomes, and open questions. Every value marked "re-measure" or "assumption" in its source keeps that mark. `questions.md` lists the questions for the owner, most important first, and per contract the requirements marked "proposal".

- [ ] **Step 1: Write the drafts**
- [ ] **Step 2: Validate them on a temporary copy of `contracts.adoc`** (`contract_problems` `[]`, style 0 errors)
- [ ] **Step 3: STOP.** Report the paths and the questions. Make no commit.

---

### Task 6: Apply IF-02 to IF-08

**Input:** the owner-confirmed drafts.

**Files:**
- Modify: `docs/system/modules/ROOT/pages/contracts.adoc`

- [ ] **Step 1: Apply the confirmed contracts in ID order**
- [ ] **Step 2: Build clean (the xrefs resolve), run all tests with the site, the style check and pre-commit**
- [ ] **Step 3: Commit** `docs: add the power, fluid and mechanical contracts`

---

After Task 6: a final review of the whole branch, its fix wave, and the squash of all commits since `main` into one commit with a message the owner approves (no trailer).
