# Contracts in System chapter 5: design

Project 2 of the docs programme (see the foundations design, `2026-11-14-docs-foundations-design.md`, section "The whole programme").

## Intent

The docs serve the owner and agents as a reliable basis for decisions.
Later they are the base for a CE technical file, which will live in a separate private repo.
The owner asked for "the contracts between the parts" as one of the questions the docs must answer.

Chapter 5 of the System docs shows the blocks of the machine (Station, Pods, Power Management Unit) and arrows between them, but no contract says what crosses an arrow and who guarantees what.
The facts are spread over the protocol page, the power page, the ADRs, the protocol schema repo and the Mechanics repo.
Many mechanical and fluid boundaries are not designed yet, because the Pod design has not started (see `docs/open-points.md` in `cocktail-maker-3d`, "Pod design (decide when it starts)").

This project gives every technical boundary of chapter 5 a contract with a stable `IF-nn` ID.
For a boundary that is not designed yet, the contract lays the groundwork: it states the requirements that the future design must meet, and the open questions.
The requirements become the specification for the Pod design, and later each requirement gets a verification in the CE file.

## Goals

- Every technical boundary between the blocks of System chapter 5 has a contract with an `IF-nn` ID.
  The scope is data, power, fluid and mechanical boundaries (owner's choice b).
- Every contract has the same sections: sides, what crosses, guarantees and limits, fault behaviour, requirements, open questions and sources.
- Every requirement has its own ID (`IF-05-R01`), names at least one quality goal (`QG-nn`), and says what meets it: a decided design (an ADR, the protocol schema, a page) or "open".
- Decided facts come with their source.
  Undecided boundaries get requirements and open questions instead of facts (owner's choice b, "requirements first").
- The chapter 5 diagram shows the `IF-nn` IDs on its arrows.
- The `architecture-docs` skill describes the contract template and its rules, so agents in all repos write and use contracts the same way.
- Tests guard the structure: anchors, ID format, sections, the goal of each requirement.

## Non-goals

- No new design decisions.
  An open question stays open until the owner decides it, and the decision then gets an ADR (SYS, CM3D or EMB) and updates the contract.
- No copy of the protocol messages or frame layout.
  IF-01 links the protocol page and the schema repo.
- No verification column yet.
  The CE file adds verification later, by requirement ID.
- No quality scenarios (`QS-nn`), they come in project 4.
- No runtime sequences, they come in project 3 (chapter 6).
- The human boundaries (Guest and Operator at the UI) stay in chapter 3 Context and Scope.
  Only the Operator's physical access to a Pod is a contract (IF-08), because it is a mechanical boundary with safety requirements.

## Anti-goals

- A fact that nobody confirmed.
  Every contract is drafted by an agent and confirmed by the owner before it is committed, as in the foundations project.
- A requirement that prescribes a mechanism instead of an outcome.
  "The Operator can change a bottle without tools" is a requirement, "the bottle sits in a bayonet mount" is a design decision.
- A contract that repeats a page and then drifts from it.
  A contract links the page that holds the details.

## Constraints

- The ID rules of the foundations project apply: IDs are defined only in System, two digits, zero-padded, never changed and never reused.
  A requirement ID is the contract ID plus `-Rnn`, for example `IF-05-R01`, and follows the same rules.
- All prose follows `writing-style`, and the style checker reports 0 errors.
  The glossary terms apply, for example Pod, Dispenser, Power Management Unit, Power Grid, Operator.
- Diagrams follow `writing-diagrams`, with the shared style partial and the edge classes.
- Commits carry no `Co-Authored-By:` trailer.
- The Mechanics and embedded repos only link contracts, they never define `IF-` IDs.

## Approaches

- **A, chosen. One "Contracts" page under chapter 5.**
  Each contract is a section with its `[[IF-nn]]` anchor, and its requirements form a table.
  It is the smallest step, keeps one defining page per ID prefix, and an agent reads one file.
  The page grows with the contracts, and a split into one page per contract stays a mechanical change, because the anchors keep their names.
- **B. One page per contract**, with an overview table in chapter 5.
  Short pages, but the ID test must allow several pages for one prefix, and the template repeats eight times.
- **C. Requirements as data**, a TSV rendered into tables like the glossary.
  Easy to test and to turn into a traceability matrix, but long requirements read poorly in TSV, and a contract would sit in two places.

## The decided parts

### Page and IDs

- A new page `docs/system/modules/ROOT/pages/contracts.adoc`, title `= Contracts`, nested under chapter 5 in `nav.adoc`, after the Hardware page.
- `IF-nn` anchors are defined only on this page.
  The foundations ID scheme named chapter 5 for `IF-nn`, and the contracts page belongs to chapter 5.
  The test constant `ID_PAGES["IF"]` changes from `building-blocks.adoc` to `contracts.adoc`, and the `architecture-docs` "Stable IDs" table names the Contracts page.
- Chapter 5 links the Contracts page and puts the `IF-nn` ID into the label of each arrow that a contract covers.
- "The chapter 5 diagram" is `conceptual-blocks` in `building-blocks.adoc` (my call).
  The Hardware diagram stays as it is.
  Today it has arrows for IF-01 (Station to Pod), IF-02 and IF-03 (Power Management Unit to Station and Pod), IF-04 (Power Grid to Power Management Unit), IF-05 (Raw Ingredients to Pod) and IF-08 (Operator to Pod).
  IF-06 (Pod to glass) and IF-07 (Pod to Station, mechanical) have no arrow.
  The diagram gets a glass node and these two edges once the owner confirms the inventory at checkpoint 1 (my call).
  This draws boundaries that physically exist and decides no design.
  The diagram stays at most 1400 px wide.

### The contract inventory (a proposal, the owner confirms it at checkpoint 1)

| ID | Boundary | Kind |
|---|---|---|
| IF-01 | Station ↔ Pod | data (USB CDC, protocol) |
| IF-02 | Power Management Unit → Station | power (5 V) |
| IF-03 | Power Management Unit → Pod | power (12/24 V) |
| IF-04 | Power Grid → Power Management Unit | power (230 V AC) |
| IF-05 | bottle → Pod | fluid (intake, bottle mount) |
| IF-06 | Pod → glass | fluid (outlet, glass position, drips) |
| IF-07 | Pod ↔ Station | mechanical (mounting, cables) |
| IF-08 | Operator ↔ Pod | mechanical (service access) |

The drafting agent may propose to add, merge or split boundaries when the sources show a reason.
The owner decides.

### The contract template

Each contract is a level-2 section in this order:

```adoc
[[IF-05]]
== IF-05: Bottle to Pod

Sides:: <the two blocks or parties, and which docs own each side>
Kind:: <data, power, fluid or mechanical>
Status:: <Decided, Partly decided or Not designed yet>

=== What crosses
<one short paragraph>

=== Guarantees and limits
<what each side guarantees, with limits and units, each with its source>

=== Fault behaviour
<what happens when one side breaks the contract, and who detects it>

=== Requirements
[cols="2,6,2,3", options="header"]
|===
| ID | Requirement | Goals | Met by
| [[IF-05-R01]]IF-05-R01 | <outcome the design must reach> | QG-02 | open
|===

=== Open questions
* <one question per line, for the owner>

=== Sources
* <links to ADRs, pages, the schema repo, Mechanics pages>
```

- "Met by" is either `open` or a link to what fulfils the requirement (an ADR, the protocol page, a Mechanics page).
  An xref to a section anchor of a page is fine.
- A requirement cell is one line with no `|` in it, so the table stays simple to read and to parse (my call).
- The drafting agent proposes the goals of each requirement, and the owner confirms them at the checkpoints.
  Every requirement names at least one goal, also a technical one, because a requirement without a goal has no reason to exist.
- A section that has nothing yet says so in one line, for example "Not designed yet." under Guarantees and limits.
- Values carry units, and a value from a source that is marked "re-measure" or "assumption" keeps that mark.

### The skill

The `architecture-docs` skill gets a section "Contracts":
- the template above
- what a requirement is (an outcome with a goal, never a mechanism)
- how a decision changes a contract (an ADR, then the contract's "Met by" and Guarantees, never a reused ID)
- that subprojects link contracts with `xref:system::contracts.adoc#IF-05[IF-05]` and never define them
- the plugin version goes from 0.5.0 to 0.6.0, in `plugins/cocktail-maker-docs/.claude-plugin/plugin.json` and `.claude-plugin/marketplace.json`

The Mechanics handoff (`docs/superpowers/handoffs/2026-11-13-mechanics-docs-prompt.md`) gets one line: the Mechanics chapter 5 links the contracts that touch its parts.

### Tests (in `docs/tests/test_docs_structure.py`)

- `IF-nn` anchors exist only on `contracts.adoc`, unique and well-formed (the existing ID tests, with the new page).
- Every requirement ID is `IF-nn-Rnn`, its `IF-nn` prefix is the contract it sits in, unique.
- Every contract has all seven sections in the template order, and a `Status::` from the three values.
- Every requirement row names at least one defined `QG-nn` and has a "Met by" of `open` or an xref whose target page exists in the repo (my call).
  The Antora build with `--log-failure-level warn` catches a broken page link, and the test catches a missing file early.
- The chapter 5 diagram source names every defined `IF-nn` at least once.
- A site test: `system/contracts.html` has an `id="IF-01"`.

### Owner checkpoints

1. **Inventory and one sample.**
   The agent drafts the inventory and the full IF-01 contract (the most decided one) in the scratchpad.
   The owner confirms the inventory and the shape before the other seven are drafted.
2. **All contracts.**
   The agent drafts IF-02 to IF-08 with requirements, open questions and sources, each requirement marked with its source or as a proposal.
   The owner confirms, corrects or removes each requirement and fact.
   Only confirmed content is committed.

### Execution

The plan describes the process and the structure, not the contract text.
The drafting tasks say which sources to use for each contract, and the content comes from the drafts that the owner confirms.

Same as the foundations project: subagent-driven, one reviewer per task, a final review of the whole branch, then the commits are squashed into one with a message the owner approves, on the branch `docs-contracts`.
Every commit, also the squashed one, carries no `Co-Authored-By:` trailer (`CLAUDE.md`).

## Left to the builder

- The wording of the Contracts page lead and of the skill section.
- The exact column widths of the requirement table and the label layout of the chapter 5 arrows, as long as the diagram stays at most 1400 px wide.
- How the tests parse the sections and tables.
- The task split of the plan.
