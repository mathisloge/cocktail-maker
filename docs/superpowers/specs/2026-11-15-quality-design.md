# Goals and quality scenarios (System chapters 1 and 10): design

Project 4 of the docs programme (see `2026-11-14-docs-foundations-design.md`, section "The whole programme").

## Intent

The docs serve the owner and agents as a reliable basis for decisions, and later as the base for a CE technical file in a separate private repo.
The quality goals QG-01 to QG-06 exist on the Quality Goals page.
They are not measurable yet: chapter 10 is empty, and the `QS-nn` IDs are reserved but unused.

This project makes the goals measurable.
Chapter 10 gets a quality tree that runs from each goal over its quality tags to concrete quality scenarios, a traceability table, and one section per scenario.
Each scenario names its measure, and its number is a target the owner set, a provisional value from a source, or open.
Chapter 1 gets the two arc42 parts it misses: an overview of the main functions and the stakeholders.
The owner asked for thorough reviews, because this content is the base of the later CE file.

### The quality model (changed after the plan review)

The first version of this design mapped the goals onto ISO/IEC 25010:2023.
The plan review showed that ISO/IEC 25010 is a product quality model for ICT and software products, and that the physical side of the machine (food-contact materials, mechanical hazards, physical access for service) falls outside it.
The owner asked for a better proposal for a system design and chose option B:
- The goals and scenarios use the tags of the arc42 quality model Q42: `#efficient`, `#flexible`, `#maintainable`, `#operable`, `#reliable`, `#safe`, `#secure`, `#suitable`, `#usable` (nine tags, verified on https://quality.arc42.org/articles/arc42-quality-model and https://quality.arc42.org/dimensions/).
  A requirement may carry several tags.
- The safety argument for CE rests on the risk assessment (hazards and safety measures, project 5), not on the quality model.
  Safety and food-safety scenarios link to hazards and safety measures once they exist.
- The decision is recorded as ADR SYS-014.

## Goals

- Chapter 1 shows the main functions of the machine and its stakeholders.
- The Quality Goals page names the Q42 tags of each goal instead of ISO/IEC 25010 characteristics.
- Chapter 10 shows the quality tree as a D2 diagram, a traceability table, and one section per quality scenario (`QS-nn`).
- Every quality goal has at least one scenario.
- Every scenario names its measure, and its number has one of these states:
  - **Target:** the owner set the number.
  - **Provisional:** a docs page or an ADR states the number, and it is still to be confirmed on the assembled machine.
  - **Open:** no number yet.
  - **Withdrawn:** the scenario no longer applies, and its ID stays reserved.
- Numbers that pages state as facts today but that are not decided are marked provisional: the 30 s pour limit and the 30 s calibration run limit (the owner: "the 30 s are also not yet decided and may change with the assembled machine").
- Every scenario links what it checks: contracts and requirements, runtime and fault scenarios, ADRs.
- The `architecture-docs` skill describes the scenario template, and `writing-diagrams` allows the one large quality tree.
- Tests guard the structure.

## Non-goals

- No new design decisions. A scenario states what the machine must reach, not how.
- No verification records (test reports, measurements). They belong in the CE file, which links them by `QS-nn`.
- No hazards and no safety measures (`SM-nn`); they come in project 5. The `Checks` field accepts `SM-nn` links from then on.
- No legal references (standards, regulations); they come in project 6 (constraints).
- No change to the goal texts. A missing or wrong goal becomes a question for the owner.

## Anti-goals

- A number that looks decided but is not.
- A scenario that cannot be checked, for example because its measure has no unit or no method.
- A scenario that no contract, runtime scenario, ADR or goal supports, presented as fact.
- A quality tag that the scenario does not really serve, forced to fit.

## Constraints

- The ID rules apply: `QS-nn`, two digits, defined only on the chapter 10 page, never changed or reused.
- The owner rules apply: no implementation details (no function, class or file names, no code constants, the code is not a source), and no "owner decision" as a source. A decision gets an ADR. A target number is a requirement that chapter 10 defines, so it needs no ADR.
- Prose follows `writing-style`, diagrams follow `writing-diagrams`, the glossary terms apply.
- Every agent loads `cocktail-maker-docs:architecture-docs`, `cocktail-maker-docs:writing-diagrams` and `cocktail-maker-docs:writing-style` before it writes.
- Commits carry no `Co-Authored-By:` trailer. Only owner-confirmed content is committed.

## Approaches

- **B, chosen. A tree diagram, a traceability table and one section per scenario.**
  The tree shows the structure, the table is searchable for an assessor, and each scenario is a section with an anchor and a fixed form, like the contracts. Tests keep the three in step.
- **A. Tables only.** Compact, but a scenario table with about twelve columns of running text reads poorly.
- **C. Scenarios as data.** A TSV gives a matrix for free, but long fields read poorly in cells, and it adds a third format.

## The decided parts

### Chapter 1

`docs/system/modules/ROOT/pages/index.adoc` gets two sections after its introduction:
- `== Requirements Overview`: the main functions, one short line each, linked to the runtime scenario or page that shows it where one exists.
- `== Stakeholders`: a table with the header `| Stakeholder | Expects of the machine | Expects of the docs`. Proposed: Guest, Operator, developer, the person who assesses the conformity for CE later. The owner confirms the list.

### The Quality Goals page

The ISO column becomes a `Q42` column with the tags of each goal (proposal, the owner confirms):

| Goal | Q42 tags |
|---|---|
| QG-01 Human Safety | `#safe` |
| QG-02 Food Safety | `#safe`, `#suitable` |
| QG-03 Reliability | `#reliable` |
| QG-04 Serviceability | `#operable` |
| QG-05 Usability | `#usable` |
| QG-06 Maintainability | `#maintainable` |

One sentence links SYS-014 and says that the safety argument for CE rests on the risk assessment. The goal texts stay.

### Change after the arc42 research

The owner decided that chapter 1 keeps only the top goals QG-01 to QG-05, each with a short scenario, and that the order of the table is the priority.
The table has the columns `ID | Quality Goal | Scenario | Q42` and no priority column.
QG-06 Maintainability moved to chapter 10, into the section `== Further Quality Goals` before the quality tree.
Its ID stays, and all links to it point to `quality-requirements.adoc#QG-06`.
The scenario sentences are:
- QG-01: "No Guest and no Operator is harmed by the machine, in use or in service, not by a mechanism, an electrical fault or a software bug."
- QG-02: "Every drink contains only the ingredients of its recipe in the right amounts, and only food-safe parts touch it."
- QG-03: "The machine serves drinks through a whole event without an Operator having to restart it, and a failing Pod stops only its own drinks."
- QG-04: "An Operator with a short briefing refills, swaps a bottle, changes a pump tube and calibrates, using only the written steps."
- QG-05: "A Guest who has never used the machine orders a drink without help."
- QG-06: "A new Station release runs with the existing Pods without changing their firmware, and it is tested without the assembled machine."

### Chapter 10

`quality-requirements.adoc` keeps its title. The "Not written yet" note goes away (and the page leaves `SYSTEM_PLACEHOLDERS`). The page holds:
1. a lead: the tree, the four states, the link to chapter 1 and SYS-014
2. the quality tree, one D2 diagram (target `system-10-quality-tree`), goals in a grid, each goal a container with its own tag containers (the tag `#safe` of QG-01 and of QG-02 are two nodes), each scenario inside the tag container of its first tag of its own goal, shared style, goals `block`, tags `boundary`, scenarios `block`, at most 1400 px wide
3. the traceability table: `| QS | Goal | Q42 | Artifact | Verification | State | Value`, one row per scenario
4. one section per scenario, in ID order

### The scenario template

```adoc
[[QS-01]]
== QS-01: <short title>

Goal:: xref:quality-goals.adoc#QG-01[QG-01]
Q42:: #safe
Source:: <who or what triggers it>
Stimulus:: <what happens>
Artifact:: <the part of the machine: the machine, the Station, a Pod, the Power Management Unit>
Environment:: <normal operation, service or after a fault, and any other condition>
Response:: <what the machine does>
Measure:: <what is measured, with unit and method>
Verification:: <Test, Inspection, Analysis or Demonstration>
Value:: <number and unit, or "none">
State:: <Target, Provisional, Open or Withdrawn>
Checks:: <xrefs>
```

- `Q42` holds one or more tags, and at least one of them is a tag of the goal.
- `Value` is `none` exactly when `State` is `Open` or `Withdrawn`. Otherwise it is a number and a unit, or a count with a noun (for example `0 drinks`).
- A drafting agent writes only `Provisional` (a docs page or an ADR states the number, linked in `Checks`) or `Open`. It may propose a target in `questions.md`, and only the owner sets `Target`.
- `Checks` holds xrefs only: to a defined `IF-nn` or `IF-nn-Rnn` on `contracts.adoc`, to a scenario anchor on `runtime-view.adoc` or `fault-scenarios.adoc`, and to any other existing page of the System or Station docs, an ADR page included, with or without an anchor (the page must exist, and the anchor is not checked for these pages). Mechanics pages are accepted unchecked. From project 5 on it also takes `SM-nn`.
- A field value may run over several lines (one sentence per line) until the next field.
- A scenario has exactly one goal, and states what must be reached, never a mechanism.

### The skills

- `architecture-docs` gets a section "Quality scenarios" with the template and its rules, and the Stable IDs row for `QS-nn` names the Quality Requirements page.
- `writing-diagrams` gets a chapter 10 row (the quality tree) and states that the quality tree may exceed the usual box count, with `direction: right`.
- The plugin goes from 0.7.0 to 0.8.0 in `plugins/cocktail-maker-docs/.claude-plugin/plugin.json` and `.claude-plugin/marketplace.json`, and the plugin test minimum to `(0, 8, 0)`.

### Tests (in `docs/tests/test_docs_structure.py`)

- `Q42_TAGS`: the nine tags.
- `goal_tags() -> dict[str, tuple[str, ...]]`: the tags of each goal from the Q42 column of the Quality Goals page.
- `quality_scenario_problems(text) -> list[str]`: anchor and heading agree; the twelve fields present and in order; the goal is defined; every `Q42` tag is one of the nine and at least one is a tag of the goal; `Verification` and `State` from their lists; the `Value` rule; `Checks` holds only xrefs, each a defined contract ID, a known scenario anchor or an existing System or Station page, and a scenario heading is `==` or `===`; IDs consecutive from `QS-01`.
- `quality_tree_problems(text) -> list[str]`: the tree block names every defined `QG-nn` and exactly the defined `QS-nn` (as `QG-\d\d` and `QS-\d\d` in labels), and the traceability table has exactly one row per scenario with the same goal, tags, state and value as its section.
- Every defined `QG-nn` has at least one scenario.
- Every line that names the 30 s pour limit or the 30 s calibration run limit, on the System pages and the Station runtime view, says it is provisional.
- Chapter 1 has the two sections and the Stakeholders table header.

### Reviews (owner: "use more thorough review agents, as this is really important")

At each checkpoint, three reviewers on the most capable model check the draft before the owner sees it. The controller dispatches them with a full brief (focus list, files, read-only, output format), works their findings in, and lists owner questions:
1. **Faithfulness:** every fact and number has a source, every provisional or open value is marked, nothing is invented, no implementation detail, no owner as source.
2. **Quality model:** the Q42 tags exist and fit each scenario and each goal, nothing is forced, and safety scenarios name what the later risk assessment must cover.
3. **Testability:** every scenario can be checked, every measure has a unit and a method, the verification method fits, stimulus, artifact and environment are concrete.

The final review has four reviewers: content against sources, quality model, testability, tests and skills.

### Owner checkpoints

1. **Chapter 1, the goal tags and the scenario list.** Requirements overview, stakeholders, the Q42 tags of each goal, and the list of scenarios (ID, title, goal, tags, artifact, what it checks, its basis), about two or three per goal.
2. **The full scenarios, the tree and the table.** Every confirmed scenario in the template with measures and states; the owner sets targets where wanted.

Drafts live in `<scratchpad>/quality/`, with a `questions.md` that has a suggested answer for each question. Before a task applies confirmed content, the controller writes the confirmed text into the scratchpad file.

### Execution

1. Chapter 10 frame, the readers and tests, the skills and the version, the provisional marking of the 30 s limits, SYS-014.
2. Checkpoint 1 with its three reviews. Stop.
3. The confirmed chapter 1 sections and goal tags.
4. Checkpoint 2 with its three reviews. Stop.
5. The confirmed scenarios, the tree and the table, with a completeness assert.
6. The final review with four reviewers, the fix wave, the squash into one commit and the local fast-forward merge into `main`.

Every task agent checks that it is on `docs-quality` and never switches branches. The repo stays green after every task.

## Left to the builder

- The wording of the leads and of the skill sections.
- The layout of the tree within the stated rules.
- How the tests parse the sections.
