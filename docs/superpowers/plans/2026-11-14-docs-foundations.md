# Docs Foundations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.
Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the docs a confirmed glossary with checker support, a stable ID scheme with the quality goals as `QG-01` to `QG-05`, and ADRs that name the quality goals they serve.
Then bring the existing System and Station text in line with them.

**Architecture:**
- **Glossaries:** each component keeps its glossary as a TSV file in its `partials` folder, and the glossary page renders it.
- **Checker:** `check_adoc_style.py` reads a plugin copy of the System glossary plus every glossary TSV in the checked repo, and warns on avoided synonyms.
- **Tests:** they keep the copy in sync and the glossaries disjoint.
- **IDs:** they are AsciiDoc anchors on their defining System page.
- **ADRs:** a test enforces a `== Quality Goals` section.
- **Owner checkpoints:** two tasks only draft a proposal and stop.
The owner confirms it before the next task applies it.

**Tech Stack:** Python 3.12 standard library (checker), pytest, AsciiDoc and Antora, Claude Code plugin skills.

**Spec:** `docs/superpowers/specs/2026-11-14-docs-foundations-design.md`

## Global Constraints

- **Commit messages carry NO `Co-Authored-By:` trailer** (`CLAUDE.md`).
Use conventional commit messages, checked by commitizen.
- **The checker stays stdlib only** and runs as a `language: script` pre-commit hook in other repos.
- **Glossary TSV:** UTF-8, tab-separated, one header row `term	definition	level	code	avoid`.
  - `level` is one of `System`, `Station`, `Pod`, `Mechanics`, `UI`.
  - `avoid` holds synonyms separated by `;`.
- **File paths:**
  - System glossary: `docs/system/modules/ROOT/partials/glossary.tsv`
  - Plugin copy: `plugins/cocktail-maker-docs/skills/writing-style/glossary-system.tsv`
  - Subproject glossary: `docs/<component>/modules/ROOT/partials/glossary.tsv`
  - 3D repo glossary: `docs/modules/ROOT/partials/glossary.tsv`
- **Glossary warnings:** the rule is `glossary-term`, severity `warning`, never `error`.
- **Glossary consistency:** canonical terms and avoided synonyms are disjoint across all glossaries of a repo.
A subproject never redefines a System term (not in System `term`, not in System `avoid`).
- **IDs:**
  - `QG-nn` quality goal: System chapter 1, Quality Goals page
  - `QS-nn` quality scenario: System chapter 10
  - `SM-nn` safety measure: System chapter 8, Safety page
  - `IF-nn` contract: System chapter 5
  - Two digits, zero-padded, never changed or reused.
Each is an AsciiDoc anchor `[[QG-01]]`, defined only in System.
- **The quality goal IDs:** `QG-01` Human Safety, `QG-02` Food Safety, `QG-03` Reliability, `QG-04` Maintainability, `QG-05` Usability.
- **Every ADR** has `== Quality Goals` directly after `== Context`, with at least one `QG-` reference.
- **No unconfirmed content:** a glossary row or an ADR goal reference is committed only after the owner confirmed it.
- **Writing rules:** all prose follows the `writing-style` skill, and the style checker reports 0 errors on `docs/system` and `docs/station`.
- **Never stage** `hw_simulator/client.cppm`, `.claude/settings.json`, `.superpowers/` or `CLAUDE.md`.

## Review Focus

1. **A word with two meanings warns falsely.** "module" means a C++ module in Station ADR STA-002.
The checker must offer a way to silence a correct use, and the term pass must end with 0 unexplained `glossary-term` warnings.
Test in Task 1 (opt-out) and Task 7 (zero warnings).
2. **A malformed glossary TSV:** a row with too few columns, an unknown `level`, or a stray tab.
`load_glossary` must fail with the file and line number, not silently skip.
Test in Task 1.
3. **Plural and case forms** of a synonym, for example "Modules" or "pumps", must warn like the base form.
A canonical term that contains a synonym must not warn, for example a canonical "Station computer" when "computer" is avoided.
Test in Task 1.
4. **Synonyms inside code spans, xref targets, attribute names and diagram blocks** must not warn.
Test in Task 1.
5. **A repo without glossary files, or a checked file outside any git repo,** must work with the plugin copy alone and never crash.
Test in Task 1.

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

### Task 1: Glossary support in the checker

**Files:**
- Modify: `plugins/cocktail-maker-docs/scripts/check_adoc_style.py`
- Create: `plugins/cocktail-maker-docs/skills/writing-style/glossary-system.tsv` (header row only)
- Create: `plugins/cocktail-maker-docs/tests/fixtures/glossary/` (fixture TSVs and a fake repo tree)
- Modify: `plugins/cocktail-maker-docs/tests/test_check_adoc_style.py`, `plugins/cocktail-maker-docs/skills/writing-style/SKILL.md`

**Interfaces:**
- **Produces:**
  - `class GlossaryRow(NamedTuple)`: `term: str`, `definition: str`, `level: str`, `code: str`, `avoid: tuple[str, ...]`
  - `GLOSSARY_LEVELS = ("System", "Station", "Pod", "Mechanics", "UI")`
  - `DEFAULT_SYSTEM_GLOSSARY: Path` points at the plugin copy.
  - `load_glossary(path: Path) -> list[GlossaryRow]`.
It raises `ValueError` naming the file and line on a malformed row.
  - `find_repo_glossaries(start: Path) -> list[Path]`.
It walks up from `start` to the directory that holds `.git` and returns the sorted `docs/**/partials/glossary.tsv` files there.
If there is no git root, it returns `[]`.
  - `glossary_synonyms(rows: Iterable[GlossaryRow]) -> dict[str, str]`: lowercase synonym → canonical term.
  - `check_text(text, avoid_words=None, glossary=None)`, where `glossary: Mapping[str, str] | None` is the synonym map.
It adds `Finding(line, "glossary-term", "warning", 'Use the glossary term "<term>" instead of "<match>".')`.
Matching works like the avoid words: case-insensitive, at word start, a suffix of word characters allowed for single words, and on the masked text.
  - A synonym that is part of a longer canonical term occurring at that position does not warn.
  - `main` loads the plugin copy plus `find_repo_glossaries` of each checked file, merges the maps and passes them to `check_text`.
- **Opt-out for a correct use of a two-meaning word:** the existing `// adoc-style: ignore-next-line`.
Document it in the skill.

- [ ] **Step 1: Write the failing tests**

```python
def test_load_glossary_reads_rows(fixture_dir): ...      # term, level, code, avoid tuple parsed; header skipped
def test_load_glossary_rejects_bad_rows(tmp_path): ...    # 4 columns, unknown level → ValueError mentioning file and line
def test_glossary_term_warns_on_synonym(): ...            # "The module pours." with {"module": "Pod"} → one glossary-term warning naming Pod
def test_glossary_term_matches_plural_and_case(): ...     # "Modules" and "modules" warn
def test_glossary_term_not_inside_canonical_term(): ...   # {"computer": "Station computer"}: "the Station computer" → no warning
def test_glossary_term_ignores_code_and_macros(): ...     # `module`, xref:module.adoc[], :module-name:, a [d2] block → no warning
def test_glossary_opt_out(): ...                          # ignore-next-line silences it
def test_find_repo_glossaries(tmp_path): ...              # fake repo with .git and two partials/glossary.tsv → both found; outside a repo → []
def test_main_uses_plugin_copy_without_repo_glossary(tmp_path, capsys): ...  # no crash, exit 0 for warnings only
```

- [ ] **Step 2: Run them to verify they fail**

Run: `uv run pytest plugins/cocktail-maker-docs/tests -q` Expected: FAIL (`load_glossary` is not defined)

- [ ] **Step 3: Implement**

- Add the header-only plugin copy.
- Add one paragraph to `writing-style/SKILL.md`: glossary terms are used exactly, the glossaries live in the `partials` folders and the plugin copy, warnings name the term, and the opt-out exists for a correct second meaning.

- [ ] **Step 4: Run all tests and pre-commit**

Expected: PASS

- [ ] **Step 5: Commit** `feat(docs): warn on glossary synonyms in the style checker`

---

### Task 2: Glossary pages and consistency tests

**Files:**
- Create: `docs/system/modules/ROOT/partials/glossary.tsv` (header row only, the same as the plugin copy)
- Modify: `docs/system/modules/ROOT/pages/glossary.adoc` (lead, table from the TSV, link to naming conventions)
- Modify: `docs/tests/test_docs_structure.py`, `plugins/cocktail-maker-docs/skills/architecture-docs/SKILL.md`

**Interfaces:**
- **Consumes:** `load_glossary`, `GlossaryRow` (Task 1), via `pythonpath` as the other tests do.
- **Produces:** `glossary_problems(system: list[GlossaryRow], others: dict[str, list[GlossaryRow]]) -> list[str]`, in `docs/tests/test_docs_structure.py`.
It reports:
  - a canonical term that is also an avoided synonym (across all glossaries)
  - a subproject term that is a System term or System synonym
  - duplicate terms
- **The page** renders the TSV with `[%header,format=tsv,cols="2,5,1,2,2"]` around `include::partial$glossary.tsv[]`, under the title `= 12 Glossary`.

- [ ] **Step 1: Write the failing tests**

```python
def test_glossary_problems_reader(): ...            # fixture rows: overlap, redefinition, duplicate → each reported; clean rows → []
def test_plugin_glossary_copy_is_in_sync(): ...     # plugin copy bytes == docs/system/.../partials/glossary.tsv
def test_glossaries_are_consistent(): ...           # glossary_problems(real system, real station if present) == []
@site_test
def test_glossary_page_renders_the_table(): ...     # system/glossary.html has a table with the five header cells
```

- [ ] **Step 2: Run to verify the new tests fail**

- [ ] **Step 3: Implement**

Write the page, the System TSV header and the `architecture-docs` text:
- chapter 12 is optional in subprojects, for terms only that subproject needs
- it uses the same TSV columns, links the System glossary, and never redefines a System term

Name this rule in the skill's chapter table.

- [ ] **Step 4: Build clean, run all tests, the style check and pre-commit**

- [ ] **Step 5: Commit** `feat(docs): render glossaries from TSV and keep them consistent`

---

### Task 3: Stable ID scheme and the quality goal IDs

**Files:**
- Modify: `docs/system/modules/ROOT/pages/quality-goals.adoc` (each goal gets its anchor and its ID shown in the table)
- Modify: `plugins/cocktail-maker-docs/skills/architecture-docs/SKILL.md` (a section "Stable IDs": the table and the rules from Global Constraints)
- Modify: `docs/tests/test_docs_structure.py`

**Interfaces:**
- **Produces:** `ID_PAGES = {"QG": "quality-goals.adoc", "QS": "quality-requirements.adoc", "SM": "safety.adoc", "IF": "building-blocks.adoc"}` and `defined_ids(prefix: str) -> list[str]` (the anchors on the defining page), both in `test_docs_structure.py`.
- **The quality goals table** gets an ID column.
Each row starts with its anchor, for example `| [[QG-01]]QG-01 | *Human Safety* | …`.
Builder's choice, as long as the anchor resolves and the ID is visible.

- [ ] **Step 1: Write the failing tests**

```python
def test_quality_goal_ids(): ...            # defined_ids("QG") == ["QG-01", ..., "QG-05"], in that order, names match Global Constraints
def test_ids_are_defined_only_on_their_page(): ...  # no [[XX-nn]] anchor for these prefixes on any other System/Station page
def test_ids_are_unique_and_well_formed(): ...      # per prefix: unique, two digits
@site_test
def test_quality_goal_anchor_resolves(): ...        # system/quality-goals.html has id="QG-01"
```

- [ ] **Step 2: Run to verify they fail**

- [ ] **Step 3: Implement**

- [ ] **Step 4: Build clean, run all tests, the style check and pre-commit**

- [ ] **Step 5: Commit** `feat(docs): give the quality goals stable IDs and define the ID scheme`

---

### Task 4: ADR skill and template with quality goals

**Files:**
- Modify: `plugins/cocktail-maker-docs/skills/writing-adrs/SKILL.md`, `plugins/cocktail-maker-docs/skills/writing-adrs/adr-template.adoc`
- Modify: `docs/tests/test_docs_structure.py`, `plugins/cocktail-maker-docs/tests/test_plugin_layout.py`

**Interfaces:**
- **Produces:** `adr_goal_problems(text: str) -> list[str]`, in `docs/tests/test_docs_structure.py`.
It reports:
  - a missing `== Quality Goals` section
  - the section not directly after `== Context` (only `=== Decision Drivers` may sit inside Context before it)
  - no `QG-nn` reference in the section
  - a `QG-`/`QS-` reference that is not a defined ID

  Task 8 applies it to the real ADRs.
- **The skill rules (from the spec):**
  - the section lists the goals and scenarios served or affected, by ID, with one line each on how
  - the decision drivers refer to these IDs
  - the consequences assess each listed goal (better, worse or neutral, with the reason)
  - an ADR with no goal reference is incomplete
  - references link to `xref:system::quality-goals.adoc#QG-01[QG-01]`
- **The template** gains the section with placeholder lines.

- [ ] **Step 1: Write the failing tests**

```python
def test_adr_goal_problems_reader(): ...  # fixture ADR texts: missing section, wrong place, no QG, unknown QG-09 → reported; good text → []
def test_adr_template_has_quality_goals(): ...  # in test_plugin_layout.py: adr_goal_problems-like check on adr-template.adoc passes
```

- [ ] **Step 2: Run to verify they fail**

- [ ] **Step 3: Implement the skill text, the template and the reader**

- [ ] **Step 4: Run all tests, the style check on the skill and template, and pre-commit**

- [ ] **Step 5: Commit** `docs: tie ADRs to the quality goals in the ADR skill and template`

---

### Task 5: Avoid words and the Hardware title

**Files:**
- Modify: `docs/system/modules/adr/pages/002-pysical-pod-connection.adoc:26` ("robust")
- Modify: `docs/station/modules/adr/pages/004-domain-type-safety.adoc:47-48` ("robust", "comprehensively")
- Modify: `docs/system/modules/adr/pages/001-system-split.adoc:14` ("Furthermore")
- Modify: `docs/system/modules/ROOT/pages/hardware.adoc` (line 6 "seamlessly", title line `# Hardware` → `= Hardware`)

- [ ] **Step 1: Reword each of the five as the avoid list suggests**

Keep every fact.
A "robust" becomes the concrete property it means in that sentence.

- [ ] **Step 2: Verify**

- The style checker reports no `avoid-word` warning on `docs/system` and `docs/station`.
- The Hardware page's first line is `= Hardware`.
- The build is clean, all tests pass and pre-commit passes.

- [ ] **Step 3: Commit** `docs: replace the remaining avoid words and fix the Hardware title`

---

### Task 6: OWNER CHECKPOINT 1, the glossary draft (no commit)

**Files:**
- Create (scratchpad only, not in the repo): `<scratchpad>/foundations/glossary-draft.tsv` and `<scratchpad>/foundations/glossary-draft.md`

**Sources for the draft:**
- the code: `strong_type` identifiers and domain types in `core/` and `cocktail-maker/`
- `docs/system/modules/ROOT/pages/naming-conventions.adoc`
- the System and Station pages and diagrams
- the term counts in the spec

**What the draft holds:**
- **Terms:** at least the spec's list (Station, Pod, dispenser, pump, ingredient, recipe, glass, Guest, Operator, calibration, fill level, transaction, Power Management Unit, Station computer, Power Grid, raw ingredients), plus terms the sources show are needed.
- **Columns:** each row fills every column, in the Global Constraints format.
- **The markdown file:**
  - lists each row with a one-line source note (where the definition comes from)
  - marks every row or cell the agent is unsure about as a question
  - lists, for each `avoid` synonym, how many hits it has in `docs/system` and `docs/station`, and which hits are a correct second meaning
- **Consistency:** run `glossary_problems` (Task 2) on the draft, and it must report `[]`.

- [ ] **Step 1: Write the draft files**

- [ ] **Step 2: Run `glossary_problems` on the draft**

Expected: `[]`

- [ ] **Step 3: STOP**

Report the draft paths and the open questions.
Make no commit.
The owner confirms, corrects or removes each row before Task 7.

---

### Task 7: Apply the confirmed glossary and the term pass

**Input:** the owner-confirmed glossary (from the controller, after Task 6).

**Files:**
- Modify: `docs/system/modules/ROOT/partials/glossary.tsv` and its plugin copy (identical)
- Modify: the `.adoc` pages under `docs/system` and `docs/station` that use an avoided synonym

**Steps:**
- [ ] **Step 1: Write the confirmed rows into the System TSV and copy it to the plugin**

- [ ] **Step 2: Run the checker and list every `glossary-term` warning**

- [ ] **Step 3: Replace each synonym with the glossary term**

- Keep the meaning.
- For a correct second meaning, add the opt-out comment above the line.
- List in the report every replacement where the meaning could shift.
- Never change text inside code spans, diagrams or ADR decision sentences beyond the term.

- [ ] **Step 4: Verify**

- 0 `glossary-term` warnings (except opted-out lines)
- `glossary_problems` reports `[]`
- the sync test passes
- the build is clean
- all tests and pre-commit pass

- [ ] **Step 5: Commit** `docs: add the confirmed glossary and use its terms`

---

### Task 8: OWNER CHECKPOINT 2, ADR quality goal references (draft, then apply)

**Files:**
- Draft (scratchpad only): `<scratchpad>/foundations/adr-goals-draft.md`
- Later modify: the 12 ADR pages in `docs/system/modules/adr/pages/` and `docs/station/modules/adr/pages/`, plus `docs/tests/test_docs_structure.py`

**The draft:**
- **Per ADR:** the proposed `== Quality Goals` section (QG IDs with one line each on how), the ID additions to the drivers and consequences, and a source note.
- **Questions:** every uncertain reference is marked as a question.
- **SYS-001:**
  - Its existing `== Related Quality Goals` section moves directly after `== Context`.
  - It is renamed `== Quality Goals`, and its rows get the IDs.
Its wording stays.
- **Prose mentions:** in SYS-003, SYS-004, SYS-006, STA-003 and STA-004, add the ID next to the goal name, for example "*Human Safety* (QG-01)".
Nothing else in those sentences changes.
- **The decision text** stays untouched.

- [ ] **Step 1: Write the draft**

- [ ] **Step 2: STOP**

Report the draft path and the questions, and make no commit.
The owner confirms or corrects the draft.

- [ ] **Step 3 (after confirmation): Apply the confirmed sections to the 12 ADRs**

- [ ] **Step 4: Add the real-ADR test**

```python
@pytest.mark.parametrize("adr", sorted(ADR pages of system and station))
def test_adrs_name_their_quality_goals(adr): assert adr_goal_problems(text) == []
```

- [ ] **Step 5: Verify**

The test passes, the build is clean, the `QG` xrefs resolve, all tests, the style check and pre-commit pass.

- [ ] **Step 6: Commit** `docs: name the quality goals of every ADR`

---

### Task 9: Handoff addition and plugin version

**Files:**
- Modify: `docs/superpowers/handoffs/2026-11-13-mechanics-docs-prompt.md`
- Modify: `plugins/cocktail-maker-docs/.claude-plugin/plugin.json`, `.claude-plugin/marketplace.json`

**Steps:**
- [ ] **Step 1: Add a handoff step**

The step covers:
- an optional Mechanics glossary (`docs/modules/ROOT/partials/glossary.tsv` and a `= 12 Glossary` page rendering it) for mechanical terms
- never redefining a System term
- using the System glossary terms exactly
- adding the `== Quality Goals` section to every CM3D ADR, with the references confirmed by the owner

Set the minimum plugin version to the new one.

- [ ] **Step 2: Bump the plugin to `0.5.0` in both manifests**

- [ ] **Step 3: Verify**

The style checker reports 0 errors on the handoff, and all tests and pre-commit pass.

- [ ] **Step 4: Commit** `docs: extend the Mechanics handoff with glossaries and ADR goals`

---

## Not in this plan

- **The contents of the quality scenarios (`QS-`), the safety measures (`SM-`) and the contracts (`IF-`):** these are projects 2, 4 and 5.
- **The placeholder chapters 2, 6, 7, 10 and 11.**
- **The Mechanics glossary content:** it is written in the 3D repo.
