# Docs foundations: glossary, IDs and decisions tied to goals

## Intent

The Sir-Mix-A-Lot docs must be a reliable basis for decisions, for the owner and for agents.
The typical questions are:
- which decisions were made
- how the system fits together
- what the contracts between the parts are
- what the long-term goals of the architecture are

The long-term goal behind this is a potential CE certification.
Its technical file will live later in a separate private repo.
The public architecture docs are the base that file builds on and references.
For now `main` is that base, and versioned releases come later.

The structure from the docs overhaul is in place: System has 12 arc42 chapters, Station has the subset, and Mechanics is in the 3D repo.
What is missing is a fixed vocabulary, stable IDs for the statements that later work references, and decisions that state which goals they serve.
This project lays those foundations.
The content of the empty chapters follows in later projects.

## The whole programme (context, not part of this project)

The owner chose to build the docs foundations first and then go chapter by chapter.
For each chapter, an agent drafts the content from the code, the docs and the ADRs.
It marks every point it cannot derive as an open question to the owner, and the owner confirms the content before it is final.

The order:
1. Foundations (this project)
2. Contracts in System chapter 5
3. System chapter 6 Runtime View
4. Goals and quality scenarios (chapters 1 and 10), with a quality tree in chapter 10 that maps the goals to the characteristics and subcharacteristics of ISO/IEC 25010:2023 and ends in the `QS-` scenarios
5. Safety concept (chapter 8, Safety page)
6. Constraints (chapter 2), including the regulatory frame to consider
7. Risks (chapter 11)
8. Deployment (chapter 7)

Each later project gets its own brainstorm, design and plan.

## Goals

- **A real System glossary in chapter 12.** For each term, a definition, the level, the term in code, and the synonyms to avoid.
- **Glossaries in subprojects.** A subproject may have its own glossary for terms only it needs.
- **The style checker warns** when a text uses a synonym that a glossary marks as "avoid".
This works in every repo.
- **A stable ID scheme** for the statements later work references:
  - `QG-nn` quality goals
  - `QS-nn` quality scenarios
  - `SM-nn` safety measures
  - `IF-nn` contracts and interfaces

  The five existing quality goals get their IDs now.
- **Every ADR states which quality goals and scenarios it serves or affects.** Its drivers and consequences refer to them by ID.
- **The existing System and Station docs use the glossary terms.** The five avoid-word warnings are gone, and the Hardware page has a proper AsciiDoc title.

## Non-goals

- **Writing new chapter content.** The placeholders in chapters 2, 6, 7, 10 and 11 stay.
Chapter 8 Safety stays as it is, apart from term changes.
- **Quality scenarios (`QS-nn`), safety measures (`SM-nn`) and contracts (`IF-nn`) themselves.** This project only defines the scheme.
The entries come in projects 2, 4 and 5.
- **The Mechanics glossary content.** It is written in the 3D repo.
This project only adds the step to a handoff prompt.
- **Versioned releases of the docs.**
- **Any claim about CE compliance.**

## Anti-goals

- **A glossary term the owner has not confirmed.** Agents treat the glossary as truth.
- **Two definitions of the same term**, for example in System and in a subproject.
- **A change of meaning in existing text during the term pass.**
- **IDs that change or get reused.** The private technical file will reference them.
- **An ADR change that rewrites the decision itself.** Only the goal references and their assessment are added.

## Constraints

- **Rules live in the plugin.** The rules for writing live in the `cocktail-maker-docs` plugin (`writing-style`, `architecture-docs`, `writing-adrs`, `writing-diagrams`).
Other repos use the plugin and the `adoc-style` hook.
- **The checker stays stdlib only.** The checker `plugins/cocktail-maker-docs/scripts/check_adoc_style.py` is stdlib only and runs as a `language: script` pre-commit hook in other repos.
There it sees only that repo's files and the plugin's own files.
- **Every page passes the writing rules.** All `.adoc` text follows `writing-style`, and the checker reports 0 errors.
- **Commits.** Commit messages in this repo carry no `Co-Authored-By:` trailer, as `CLAUDE.md` says.
- **Antora limits.** Antora can include files from the same or another component (`include::system:ROOT:partial$…[]`), but not files outside the content sources.

## Approaches

### Programme order (chosen: A)

- **A, chosen.
Foundations first, then chapter by chapter.** An agent drafts and the owner confirms each chapter.
Every statement is confirmed, and the terms and IDs exist before the content that uses them.
- **B. Draft every missing chapter at once, then one review.** Faster, but wrong details would slip through, and agents and the technical file would build on them.
- **C. The owner writes the content and agents only check.** The most reliable, but slow, and it wastes the owner's time on content an agent can draft from the code.

### Where a glossary's data lives (my call)

- **Chosen.
A table file per component.** Each component keeps its glossary as a TSV file in its own `partials` folder, and its glossary page renders that file as a table.
The checker reads a copy of the System file that ships in the plugin, plus the repo's own glossary file.
A test in this repo keeps the plugin copy equal to the System file.
- **Glossary only as an AsciiDoc table, parsed by the checker.** Rejected, because parsing tables from pages is fragile and the 3D repo has no copy of the System page.
- **Glossary only in the plugin.** Rejected, because the System glossary is project content and belongs to the System docs, and subproject glossaries could not live in their repos.

## Design

### Glossary data and pages

**System glossary.** The file is `docs/system/modules/ROOT/partials/glossary.tsv`, UTF-8, tab-separated, with one header row.
Its columns:

| Column | Content |
|---|---|
| `term` | the canonical term, as it is written in prose (for example `Pod`, `Guest`) |
| `definition` | one or two plain sentences |
| `level` | `System`, `Station`, `Pod`, `Mechanics` or `UI` |
| `code` | the name in code if one exists (for example `PodId`, `DispenserId`), else empty |
| `avoid` | synonyms to avoid, separated by `;` inside the cell, else empty |

**The page.** `docs/system/modules/ROOT/pages/glossary.adoc` (`= 12 Glossary`) keeps its title.
It gets a short lead and renders the TSV as a table through an `include` inside a `[%header,format=tsv]` table block.
It links the naming conventions page for code and protocol naming, which stays.

**The term list.** An agent drafts the term list from the code (the `strong_type` identifiers and the domain types), `naming-conventions.adoc`, the docs and the diagrams.
At least these terms:
- Station, Pod, dispenser, pump, ingredient, recipe, glass
- Guest, Operator
- calibration, fill level, transaction
- Power Management Unit, Station computer
- Power Grid, raw ingredients

The owner confirms every row before the term pass starts.
A term the owner does not confirm is left out.

**Subproject glossaries.**
- A subproject may have `docs/<component>/modules/ROOT/partials/glossary.tsv` (or the 3D repo's `docs/modules/ROOT/partials/glossary.tsv`) and a chapter 12 page with the same columns.
- It never redefines a System term.
A test in this repo checks that for Station.
- The `architecture-docs` skill makes chapter 12 optional in subprojects, for terms only that subproject needs, and says it links the System glossary.

### Checker support for glossaries

- **Where the checker reads synonyms.** The checker reads the `avoid` column of:
  1. the plugin copy `plugins/cocktail-maker-docs/skills/writing-style/glossary-system.tsv`
  2. if present in the repo it checks, every `docs/**/partials/glossary.tsv`, found from the checked file's repo root
- **What it reports.** A synonym from an `avoid` cell gives a `glossary-term` warning that names the canonical term.
It is a warning, never an error, like the avoid words.
Matching works like the avoid words: case-insensitive, on word boundaries, not inside code spans or macro targets.
- **Two meanings.** Some synonyms are valid in other senses, for example "module" for a C++ module.
Where needed, the `avoid` cell may name a context with `word (prose only)`, or a line opts out with the existing `// adoc-style: ignore-next-line`.
The builder picks the simplest rule that keeps false warnings rare in the current docs.
- **Keeping the copy in sync.** A test in this repo fails if the plugin copy differs from `docs/system/modules/ROOT/partials/glossary.tsv`.
The generator or a small sync script writes the copy.
- **Skill text.** The `writing-style` skill says that glossary terms are used exactly, and where the glossaries are.

### ID scheme

**The scheme.** It goes into the `architecture-docs` skill, as a section "Stable IDs":

| Prefix | Meaning | Defined in |
|---|---|---|
| `QG-nn` | quality goal | System chapter 1, Quality Goals page |
| `QS-nn` | quality scenario | System chapter 10 |
| `SM-nn` | safety measure | System chapter 8, Safety page |
| `IF-nn` | contract or interface between parts | System chapter 5 |

**The rules:**
- IDs are two-digit, zero-padded and unique per prefix.
- An ID is never changed or reused.
A retired entry stays with a "Retired" note.
- Each ID is an AsciiDoc anchor (`[[QG-01]]`), so other pages and repos can link it with `xref:system::quality-goals.adoc#QG-01[QG-01]`.
- IDs are only defined in System.

**What gets IDs now.** The five quality goals get `QG-01` to `QG-05`, in their current order, on the Quality Goals page:
- `QG-01` Human Safety
- `QG-02` Food Safety, widened at the final review: food-safe materials, and no wrong ingredient or quantity reaches a drink
- `QG-03` Reliability
- `QG-04` Serviceability (renamed from Maintainability at checkpoint 2, the ID stays)
- `QG-05` Usability
- `QG-06` Maintainability, added at checkpoint 2: the code, the firmware and the hardware design stay easy to change, test and verify

The owner chose the names at checkpoint 2 so that they map cleanly onto ISO/IEC 25010:2023, where Maintainability means changing the product.
The Quality Goals page shows the ISO characteristic of each goal in a column.
The full quality tree follows in project 4.

**Tests.** IDs are unique per prefix, each has an anchor, and each prefix appears only on its defining page.

### ADRs tied to quality goals

**The skill.** `writing-adrs` gets these rules:
- Every ADR has a section `== Quality Goals` right after `== Context`.
It lists the goals and scenarios the decision serves or affects, by ID, each with one line on how.
- The decision drivers refer to these IDs.
- The consequences assess the effect on each listed goal (better, worse, neutral, with the reason).
- An ADR with no goal reference is incomplete.

**The template.** The `adr-template.adoc` gets the section.

**The test.** Every ADR page in `docs/system` and `docs/station` has the section and at least one `QG-` reference that resolves to an anchor.

**Retrofit.**
- The 12 existing ADRs (SYS-001 to 007, STA-001 to 005) get the section.
- An agent drafts the goal references from each ADR's content and marks every uncertain one as a question.
- The owner confirms the references before they are committed.
- The decision text itself does not change.
- `QS-` references are added in project 4, when the scenarios exist.

### Term pass and cleanup

**The term pass.**
- After the owner has confirmed the glossary, an agent replaces the avoided synonyms in `docs/system` and `docs/station` with the glossary terms.
- The meaning stays the same, and every replacement where the meaning could shift is listed for the owner.
- The checker reports 0 `glossary-term` warnings afterwards.
The only exceptions are warnings with a documented opt-out.

**Other cleanup:**
- The five avoid-word warnings are reworded:
  - "robust" in `system/adr/002` and `station/adr/004`
  - "comprehensively" in `station/adr/004`
  - "Furthermore" in `system/adr/001`
  - "seamlessly" in `system/hardware.adoc`
- The Hardware page title `# Hardware` becomes `= Hardware`.

### Consistency between glossaries (my call)

- **Canonical terms and avoided synonyms never overlap.** Across all glossaries in a repo (System and the repo's own), a word that is a canonical `term` in one row never appears in any `avoid` cell.
A test enforces this, and the checker therefore never warns on a canonical term.
- **A subproject glossary never redefines a System term.** Its `term` values are not in the System `term` column and not in the System `avoid` column.
A test checks this for Station in this repo.
The 3D repo gets the same rule through the skill.

### Execution with owner checkpoints (my call)

There is one plan with two owner checkpoints, and the owner is reachable during execution.

**Before checkpoint 1.** The plan builds:
- the checker support
- the tests
- the ID scheme
- the `QG-` anchors
- the ADR skill and template
- the glossary page

It tests all of this against fixture glossaries in the test folder, never against an unconfirmed real glossary.

**Checkpoint 1: glossary.** An agent drafts `glossary.tsv` as a proposal and stops.
The owner confirms, corrects or removes every row.
Only then is the real glossary committed and the term pass run.

**Checkpoint 2: ADR goal references.** An agent drafts the `== Quality Goals` sections of the 12 ADRs and stops.
The owner confirms them, and only then are they committed.
The ADR test is switched on with this commit.

### Existing quality goal mentions in ADRs (my call)

- **SYS-001.** It already has a `== Related Quality Goals` section at the end, with a table.
That section is moved right after `== Context`, renamed to `== Quality Goals`, and its rows get the `QG-` IDs.
Its wording stays.
This counts as the new section, so there is no second list.
- **Prose mentions in the other ADRs.** Some name a goal in prose, such as "*Human Safety*" in SYS-003, SYS-004 and SYS-006, or a link to the quality goals page in STA-003 and STA-004.
These get the ID added next to the name, for example "*Human Safety* (QG-01)".
Nothing else in those sentences changes.

### Handoff addition

The Mechanics handoff prompt `docs/superpowers/handoffs/2026-11-13-mechanics-docs-prompt.md` gets one more step:
- an optional Mechanics glossary (`docs/modules/ROOT/partials/glossary.tsv` and a chapter 12 page) for mechanical terms
- the System glossary terms used exactly
- the `QG-` references in the CM3D ADRs, confirmed by the owner

The plugin version is bumped.

## Left to the builder

- The exact rendering of the TSV in AsciiDoc and how the page lead reads.
- How the glossary rule handles words with two meanings, as long as false warnings stay rare.
- Whether the plugin copy is written by the palette generator, a new small script or a test-only check with a manual copy, as long as a test catches drift.
- The test layout, following the existing tests in `docs/tests` and `plugins/cocktail-maker-docs/tests`.
- The plugin version number, following the existing bumps.
