# Docs Overhaul Piece 3: arc42 Restructure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:**
- Reorganise System into the 12 arc42 chapters.
- Give Station the numbered arc42 subset.
- Link System chapter 5 and the subprojects both ways.
- Move every ADR to its component prefix.
- Guard the result with tests.

**Architecture:** The content moves, it is not rewritten.
- **System:** `system-design.adoc` is split into chapter pages and removed.
- **Kept pages:** existing pages that already fit stay with their file names as subpages, so cross-component xrefs keep working.
- **Chapter titles:** they carry the chapter number, and the nav lists them in order.
- **Tests:**
  - A source-level pytest checks the ADR titles and ID references, without a built site.
  - A built-site pytest checks the chapter order in the nav.

**Tech Stack:** AsciiDoc and Antora, pytest (standard library only), the piece 1 style checker and skills.

**Spec:** `docs/superpowers/specs/2026-11-13-docs-overhaul-design.md` (section "Piece 3: restructure"). The rules for the structure are in the `architecture-docs` and `writing-adrs` skills in `plugins/cocktail-maker-docs/skills/`. Implementers read both before they start.

## Global Constraints

- **Moved text keeps its wording.** Allowed changes:
  - heading levels
  - a one-sentence lead-in where a moved section needs context on its new page
  - retargeted xrefs
- **Every touched `.adoc` file passes the style checker with zero errors.** The `adoc-style` hook enforces this.
- **Chapter titles** have the form `= <n> <arc42 name>`, for example `= 5 Building Block View`. File names carry no numbers and stay stable. Subpages have plain titles without numbers.
- **Unwritten chapters** use exactly the admonition from `architecture-docs`, followed by one or two lines on what belongs there:

  ```
  [NOTE]
  ====
  Not written yet.
  ====
  ```

- **ADR titles** have the form `= SYS-007: <title>` (System) or `= STA-005: <title>` (Station). The number comes from the file name, and file names never change.
- **ADR references:**
  - Every reference to an ADR ID uses the prefixed form: link texts, plain text and code comments.
  - A reference names the ADR of the component it points to. `xref:adr:…` points to the same component, and `xref:system:adr:…` points to System.
- **Redirects:** none. Old URLs may break, so `page-aliases` are a non-goal.
- **The 3D repo is not touched.** Its docs have no `xref:system::` links today.
- **`CLAUDE.md` is gitignored and untracked,** so it is edited only in the working tree.
- **Never stage** `hw_simulator/client.cppm` or `.claude/settings.json`.

## Target structure

**System** (`docs/system/modules/ROOT/`):

| Nav entry | File | Source |
|---|---|---|
| 1 Introduction and Goals | `index.adoc` (retitled) | `index.adoc` |
| ↳ Quality Goals | `quality-goals.adoc` (kept) | unchanged |
| 2 Constraints | `constraints.adoc` (new) | placeholder |
| 3 Context and Scope | `context-and-scope.adoc` (new) | `system-design.adoc` "System Context" (text and diagram) |
| 4 Solution Strategy | `solution-strategy.adoc` (new) | `system-design.adoc` "Key Technology Decisions" |
| 5 Building Block View | `building-blocks.adoc` (new) | `system-design.adoc` "Conceptual Building Blocks" (text and diagram) |
| ↳ Hardware | `hardware.adoc` (kept) | unchanged |
| 6 Runtime View | `runtime-view.adoc` (new) | placeholder |
| 7 Deployment View | `deployment-view.adoc` (new) | placeholder |
| 8 Cross-cutting Concepts | `cross-cutting-concepts.adoc` (new) | `system-design.adoc` "Cross-Cutting Concepts" intro |
| ↳ Protocol | `protocol.adoc` (kept) | gains `system-design.adoc` "Station-Pod Communication Protocol" and "Physical Transport" |
| ↳ Power | `power.adoc` (new) | `system-design.adoc` "Power Architecture" (text and diagram) |
| ↳ Safety | `safety.adoc` (new) | `system-design.adoc` "Safety Concept" |
| ↳ Naming Conventions | `naming-conventions.adoc` (kept) | unchanged |
| 9 Architecture Decisions | `architecture-decisions.adoc` (new) | one lead sentence, the ADR list in the nav below it |
| ↳ SYS-001 … SYS-007 | `adr:001-…` … `adr:007-…` | retitled in Task 3 |
| 10 Quality Requirements | `quality-requirements.adoc` (new) | placeholder (quality scenarios, the goals stay in chapter 1) |
| 11 Risks and Technical Debt | `risks-and-technical-debt.adoc` (new) | placeholder |
| 12 Glossary | `glossary.adoc` (new) | `system-design.adoc` "Domain Terminology" |

`system-design.adoc` is deleted.
`adr/pages/999-example.adoc` is deleted, because the template now lives in `plugins/cocktail-maker-docs/skills/writing-adrs/adr-template.adoc`.

**Station** (`docs/station/modules/ROOT/`): the same file names it has today, retitled and in this nav order.

| Nav entry | File |
|---|---|
| 1 Introduction and Goals | `index.adoc` |
| 2 Constraints | `constraints.adoc` |
| 3 Context and Scope | `context-and-scope.adoc` |
| 4 Solution Strategy | `solution-strategy.adoc` |
| 5 Building Block View | `building-blocks.adoc` |
| 6 Runtime View | `runtime-view.adoc` |
| 7 Deployment View | `deployment-view.adoc` |
| 8 Cross-cutting Concepts | `cross-cutting-concepts.adoc` |
| 9 Architecture Decisions | `architecture-decisions.adoc` (new), with STA-001 … STA-005 below it |
| 11 Risks and Technical Debt | `risks-and-technical-debt.adoc` |

Nav subpages are nested list items (`**`).
In `nav.adoc` an xref without link text shows the page title, so the nav shows the numbers.

## Review Focus

1. **Moved sections stay complete:** every sentence of `system-design.adoc` must land on exactly one new page, with no loss and no duplication. Test in Task 1 with the sentence check.
2. **Cross-component xrefs:** Station has six links to `system::system-design.adoc` that must point at the chapter that now holds the content. A link to a removed page only warns in Antora, so the build must run with `--log-failure-level warn`. Test in Tasks 1 and 2.
3. **Anchors:** links to anchors (`xref:system::protocol.adoc#_versioning`) and auto-generated section IDs can break when a heading level changes. The warn-level build catches broken page links, but not broken anchors. Check every `#` anchor link in the touched files by hand in Task 1.
4. **Ambiguous plain-text ADR IDs:** in a Station page, a plain "ADR-005" can mean STA-005 or the System ADR 005. Decide each one from its context and the linked file. Test in Task 3 with the source-level ADR check.
5. **The ADR header partial** reads the attributes, not the title, so it must still render. A superseded Station ADR (`003`) points to its successor with an xref text that must become `STA-005`. Test in Task 3.

---

## Verification commands (used by every task)

```bash
export PATH=<scratchpad>/node/bin:<scratchpad>/uvenv/bin:$PATH
# strict local build (fails on any warning, including missing xref targets)
npx antora --log-failure-level warn <scratchpad>/playbook-local.yml
# style
python3 plugins/cocktail-maker-docs/scripts/check_adoc_style.py $(git ls-files 'docs/system/*.adoc' 'docs/station/*.adoc') | grep -c ': error: '   # → 0
# tests
uv run pytest -q
DOCS_SITE_DIR=<scratchpad>/site uv run pytest docs/ui/tests docs/tests -q
```

The baseline strict build at the start of this plan exits 0 with no warnings.

---

### Task 1: System into 12 chapters

**Files:**
- Create in `docs/system/modules/ROOT/pages/`:
  - `constraints.adoc`, `context-and-scope.adoc`, `solution-strategy.adoc`, `building-blocks.adoc`
  - `runtime-view.adoc`, `deployment-view.adoc`, `cross-cutting-concepts.adoc`
  - `power.adoc`, `safety.adoc`, `architecture-decisions.adoc`
  - `quality-requirements.adoc`, `risks-and-technical-debt.adoc`, `glossary.adoc`
- Modify: `docs/system/modules/ROOT/pages/index.adoc` (title `= 1 Introduction and Goals`), `protocol.adoc`, `quality-goals.adoc` and `protocol.adoc` (their xrefs to `system-design.adoc`), `docs/system/modules/ROOT/nav.adoc`
- Delete: `docs/system/modules/ROOT/pages/system-design.adoc`
- Modify: the Station pages and ADRs that link `system::system-design.adoc`, retargeted to the System chapter that now holds the content:
  - `station/modules/ROOT/pages/index.adoc`
  - `solution-strategy.adoc`, `deployment-view.adoc`, `context-and-scope.adoc`, `constraints.adoc`
  - `station/modules/adr/pages/001-ui-toolkit.adoc`
- Create: `docs/tests/test_docs_structure.py`. Modify: `pyproject.toml` (add `docs/tests` to `testpaths`).

**Interfaces:**
- Produces the System chapter pages and file names in "Target structure". Tasks 2 and 3 link to them, in particular `system::building-blocks.adoc` and `system::glossary.adoc`.
- **`docs/tests/test_docs_structure.py`:** source-level tests with no built site, plus site tests gated on `DOCS_SITE_DIR` like `docs/ui/tests`. It defines `REPO: Path` and a helper `nav_titles(site_dir: Path, component: str) -> list[str]`. That helper reads the sidebar nav titles in order from `<site>/<component>/index.html` with `html.parser`.

- [ ] **Step 1: Save the sentence list of `system-design.adoc`**

Write every non-empty line of `system-design.adoc` to a scratch file. Leave out headings (`=`), block delimiters, attribute and macro lines.
After the move, this check must print nothing:

```bash
python3 - <<'EOF'
import pathlib
old = [l.strip() for l in pathlib.Path("<scratch>/system-design-lines.txt").read_text().splitlines() if l.strip()]
new = "\n".join(p.read_text() for p in pathlib.Path("docs/system/modules/ROOT/pages").glob("*.adoc"))
for l in old:
    if new.count(l) != 1:
        print(new.count(l), l)
EOF
```

A line may be printed only where a retargeted xref changed it. List each such line in the report.

- [ ] **Step 2: Write the failing tests**

```python
SYSTEM_CHAPTERS = ["1 Introduction and Goals", "2 Constraints", "3 Context and Scope", "4 Solution Strategy",
                   "5 Building Block View", "6 Runtime View", "7 Deployment View", "8 Cross-cutting Concepts",
                   "9 Architecture Decisions", "10 Quality Requirements", "11 Risks and Technical Debt", "12 Glossary"]

def test_system_chapter_pages_exist_with_numbered_titles():
    # for each (file, title) pair from the Target structure, the file's first line == f"= {title}"

def test_system_design_is_gone():
    # docs/system/modules/ROOT/pages/system-design.adoc does not exist and no .adoc under docs/ links "system-design.adoc"

def test_placeholders_use_the_admonition():
    # constraints, runtime-view, deployment-view, quality-requirements, risks-and-technical-debt (System)
    # contain the exact four-line [NOTE] / ==== / Not written yet. / ==== block

@site_test
def test_system_nav_lists_chapters_in_order():
    # [t for t in nav_titles(site, "system") if t[0].isdigit()] == SYSTEM_CHAPTERS
```

- [ ] **Step 3: Run the tests to verify they fail**

Run: `uv run pytest docs/tests -v`
Expected: the source tests FAIL, because the pages are missing and `system-design.adoc` still exists.

- [ ] **Step 4: Move the content and write the pages, the placeholders and the nav**

For the placeholders, the one or two lines say what belongs in the chapter, following arc42.
For example, chapter 6 says: "This chapter will show how the Station and the Pods work together at runtime for the main scenarios. Until then, see xref:station::runtime-view.adoc[the Station runtime view]."

- [ ] **Step 5: Retarget the links to `system-design.adoc`**

This covers the System pages and the six Station links.
Each link goes to the chapter whose content the sentence talks about. For example, context goes to `context-and-scope.adoc`, building blocks to `building-blocks.adoc`, and power to `power.adoc`.

- [ ] **Step 6: Run the sentence check from Step 1, the tests, the strict build and the style check**

Expected:
- The sentence check prints only the retargeted-xref lines you listed.
- All tests pass.
- The build exits 0.
- The style check reports 0 errors.

- [ ] **Step 7: Commit**

```bash
git add docs/system docs/station docs/tests pyproject.toml
git commit -m "docs: split the System design into the twelve arc42 chapters"
```

---

### Task 2: Station subset and zoom links

**Files:**
- Modify: every page in `docs/station/modules/ROOT/pages/` (numbered titles), `docs/station/modules/ROOT/nav.adoc`
- Create: `docs/station/modules/ROOT/pages/architecture-decisions.adoc`
- Modify: `docs/system/modules/ROOT/pages/building-blocks.adoc` (zoom links)
- Modify: `docs/tests/test_docs_structure.py`

**Interfaces:**
- Consumes: `system::building-blocks.adoc` from Task 1, and `nav_titles` and `REPO` from `test_docs_structure.py`.
- Produces: Station chapter titles and nav order as in "Target structure".

**Zoom links:**
- **System `building-blocks.adoc`:** the Station block's text links `xref:station::building-blocks.adoc[the Station building block view]`. The Mechanics or Pod mechanics text links `xref:mechanics::index.adoc[the Mechanics docs]`, which points to the start page until the 3D repo has its chapter 5. Embedded stays plain text without a link.
- **Station `building-blocks.adoc`:** it opens with one or two sentences on where the Station sits in System chapter 5, linking `xref:system::building-blocks.adoc[System building block view]`. This follows the example in `architecture-docs`.
- **Station `index.adoc`:** it is titled `= 1 Introduction and Goals`. Its existing sections stay as they are.

- [ ] **Step 1: Write the failing tests**

```python
STATION_CHAPTERS = ["1 Introduction and Goals", "2 Constraints", "3 Context and Scope", "4 Solution Strategy",
                    "5 Building Block View", "6 Runtime View", "7 Deployment View", "8 Cross-cutting Concepts",
                    "9 Architecture Decisions", "11 Risks and Technical Debt"]

def test_station_chapter_titles():
    # each Station file's first line == f"= {title}" per the Target structure

def test_zoom_links_both_ways():
    # system building-blocks.adoc contains "xref:station::building-blocks.adoc[" and "xref:mechanics::index.adoc[";
    # station building-blocks.adoc contains "xref:system::building-blocks.adoc[" within its first 6 lines;
    # no .adoc under docs/system mentions "xref:embedded::"

@site_test
def test_station_nav_lists_chapters_in_order():
    # [t for t in nav_titles(site, "station") if t[0].isdigit()] == STATION_CHAPTERS
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `uv run pytest docs/tests -v`
Expected: the new tests FAIL.

- [ ] **Step 3: Retitle, add `architecture-decisions.adoc`, reorder the nav and write the zoom lines**

- [ ] **Step 4: Run the tests, the strict build and the style check**

Expected: all pass, the build exits 0, and the style check reports 0 errors.

- [ ] **Step 5: Commit**

```bash
git add docs/station docs/system/modules/ROOT/pages/building-blocks.adoc docs/tests
git commit -m "docs: number the Station chapters and link the building block views both ways"
```

---

### Task 3: ADR prefixes everywhere

**Files:**
- Modify: all ADR pages in `docs/system/modules/adr/pages/` (SYS) and `docs/station/modules/adr/pages/` (STA). That covers titles, link texts and plain-text IDs.
- Modify: every other `.adoc` under `docs/system` and `docs/station` that names an ADR ID.
- Modify: `cocktail-maker/fill_level.cppm:71` (`ADR-007` → `SYS-007`, the System calibration ADR) and `core/strong_type.test.cpp:33` (`ADR-004` → `STA-004`, the Station type-safety ADR).
- Modify, working tree only: `CLAUDE.md` lines with `ADR-002`, `ADR-003`, `ADR-005` and `ADR-004`, which become `STA-…` (all Station ADRs).
- Modify: `docs/superpowers/handoffs/2026-11-13-mechanics-docs-prompt.md`. The Mechanics chapter 5 now links back to `xref:system::building-blocks.adoc[System building block view]`, so drop the "start page until piece 3" wording.
- Delete: `docs/system/modules/adr/pages/999-example.adoc`
- Modify: `docs/tests/test_docs_structure.py`

**Interfaces:**
- Consumes: the chapter 9 pages from Tasks 1 and 2.

- [ ] **Step 1: Write the failing tests**

```python
def test_adr_titles_carry_the_component_prefix():
    # for docs/system/modules/adr/pages/NNN-*.adoc: first line matches rf"^= SYS-{NNN}: \S"
    # for docs/station/modules/adr/pages/NNN-*.adoc: first line matches rf"^= STA-{NNN}: \S"

def test_no_unprefixed_adr_ids_left():
    # no match of r"\bADR[- ]?\d{3}\b" in any .adoc under docs/system and docs/station,
    # nor in cocktail-maker/fill_level.cppm and core/strong_type.test.cpp

def test_example_adr_is_gone():
    # docs/system/modules/adr/pages/999-example.adoc does not exist
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `uv run pytest docs/tests -v`
Expected: the three new tests FAIL.

- [ ] **Step 3: Rename the titles and every reference, delete the example, update the code comments, `CLAUDE.md` and the handoff**

For each plain-text or link-text ID, decide the component from the xref target or the context. List every decision that was not obvious in the report.

- [ ] **Step 4: Run the tests, the strict build, the style check, and `uv run pre-commit run --all-files`**

Expected: everything passes. `clang-format` must leave the two C++ files unchanged apart from the IDs.

- [ ] **Step 5: Commit**

```bash
git add docs/system docs/station docs/tests docs/superpowers/handoffs cocktail-maker/fill_level.cppm core/strong_type.test.cpp
git commit -m "docs: prefix every ADR with its component"
```

---

## Not in this plan

- **Writing the placeholder chapters:** later, one at a time.
- **The dark theme, the palette generator, the D2 migration and the reference page:** piece 4.
- **Mechanics:** it follows the handoff prompt in the 3D repo.
