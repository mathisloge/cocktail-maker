---
name: architecture-docs
description: 'Use when creating, restructuring or extending architecture documentation (arc42 chapters, building block views, runtime and deployment views, cross-cutting concepts, diagrams) in any Sir-Mix-A-Lot Antora component: system, station, embedded or mechanics.'
---

# Architecture docs

This skill builds on `writing-style`.
Follow `writing-style` for every sentence, and follow this skill for the structure of the pages.

## Components

The architecture docs are Antora components.

| Component | Holds | Source |
|---|---|---|
| `system` | the whole-machine architecture and the cross-part decisions | `docs/system/` in cocktail-maker |
| `station` | the Station software | `docs/station/` in cocktail-maker |
| `embedded` | the Pod firmware | the firmware repo, which moves to Antora later |
| `mechanics` | the mechanical parts | `docs/` in cocktail-maker-3d |

`system` is the parent.
`station`, `embedded` and `mechanics` are subprojects.
A subproject page covers only its own part and links to System for the whole machine.

## Chapters

Every component follows arc42.
The table shows which chapters each component has.

| Chapter | In System | In subprojects |
|---|---|---|
| 1 Introduction and Goals | yes | yes |
| 2 Constraints | yes | optional, only for constraints that belong to the subproject alone |
| 3 Context and Scope | yes | yes |
| 4 Solution Strategy | yes | yes |
| 5 Building Block View | yes | yes |
| 6 Runtime View | yes | yes |
| 7 Deployment View | yes | yes |
| 8 Cross-cutting Concepts | yes | yes |
| 9 Architecture Decisions | yes | yes |
| 10 Quality Requirements | yes | no, System only |
| 11 Risks and Technical Debt | yes | yes |
| 12 Glossary | yes | optional, only for terms that the subproject alone needs, with the same TSV columns, a link to the System glossary and no System term |

## Numbering

Chapter numbers are identical in every component.
The number is part of the page title, so it appears in the title and in the nav.
File names stay stable and carry no numbers.

```adoc
// building-blocks.adoc
= 5 Building Block View
```

In `nav.adoc`, list the chapters in order as `xref:building-blocks.adoc[]`.
An xref without text shows the page title, so the nav shows the number too.
Subpages and ADRs are nested `**` items under their chapter in `nav.adoc`.

```adoc
* xref:cross-cutting-concepts.adoc[]
** xref:protocol.adoc[]
* xref:architecture-decisions.adoc[]
** xref:adr:001-system-split.adoc[]
```

## Links to other components

A cross-component xref names the component in its link text.
Every component has the same numbered chapter titles, so a link without text reads like a link to a chapter of the reader's own component.

```adoc
See the xref:system::solution-strategy.adoc[System solution strategy].
```

An ADR ID such as `SYS-002` also names the component.
So does a stable ID such as `QG-01`, because stable IDs are defined only in System.

## Only in System

Quality goals and the naming conventions live only in System, and so do the terms of the System glossary.
Subprojects link to them, for example with `xref:system::quality-goals.adoc[System quality goals]`.
Subprojects never repeat them.

## Stable IDs

Items that other pages refer to carry an ID.
The ID is an AsciiDoc anchor such as `[[QG-01]]`, and it is also shown as text next to the item.

| Prefix | Item | Defined on |
|---|---|---|
| `QG-nn` | quality goal | System chapter 1, Quality Goals page (`QG-01` to `QG-05`), and System chapter 10, Quality Requirements page (further goals, such as `QG-06`) |
| `QS-nn` | quality scenario | System chapter 10, Quality Requirements page |
| `SM-nn` | safety measure | System chapter 8, Safety page |
| `IF-nn` | contract | System chapter 5, Contracts page |
| `IF-nn-Rnn` | requirement of a contract | System chapter 5, Contracts page, in the requirements table of contract `IF-nn` |

- Two digits, zero-padded, for example `QG-01`.
- An ID is never changed and never reused, even when its item is removed.
- IDs are defined only in System.
  Subprojects refer to them with `xref:system::quality-goals.adoc#QG-01[QG-01]`.
- A link to a goal names the page that defines it, so `QG-01` to `QG-05` link `quality-goals.adoc` and `QG-06` links `quality-requirements.adoc`.

## No code, no owner as a source

An architecture page names no function, method, class, type, file or code constant, and it quotes no value from the code.
The code is not a source.
A source is a docs page or an ADR.
A decision that no ADR records gets an ADR (see `writing-adrs`), and the page links that ADR.
Never cite a source such as "owner decision", and never list a person as an owner of a side or a decision.
A statement that only the code supports is left out, or it becomes an open question without code names.
Protocol message names stay, because the protocol page defines them.
An ADR that chooses a library may name that library, but no file, function or class.
A docs test checks the System pages, ADRs included, for file names, C++ scope names and phrases such as "code only", "owner decision", "user decision", "owner decided" and "owner confirmed".

## Zoom from System into a subproject

System chapter 5 shows the whole machine as blocks.
Every block in it links to the chapter 5 of its subproject.
Use a cross-component xref, such as `xref:station::building-blocks.adoc[Station building block view]`.

Each subproject chapter 5 opens with one or two sentences on where it sits in System chapter 5.
These sentences link back to System chapter 5.

```adoc
= 5 Building Block View

The Station is the host block in the xref:system::building-blocks.adoc[System building block view].
This page shows the blocks inside the Station.
```

## Guides

How-to content goes into a separate "Guides" section of the nav.
Printing, assembly and service are examples.
Never put how-to content into an arc42 chapter.
A chapter says what a part is and why it is like that.
A guide says how to do a task.

```adoc
.Guides
* xref:print-the-parts.adoc[]
```

## Unwritten chapters

A chapter that has no content yet still gets its page.
The page holds this exact admonition:

```adoc
[NOTE]
====
Not written yet.
====
```

After the admonition, write one or two lines on what belongs in the chapter.

## Contracts

A contract states what crosses the border between two blocks or parties, what each side guarantees and what the design must reach there.
All contracts live on the Contracts page of System chapter 5.
Each one is a level-2 section in this form.

```adoc
[[IF-05]]
== IF-05: Bottle to Pod

Sides:: <the two blocks or parties, and which docs describe each side>
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

- Keep the six level-3 sections and their order, and give `Kind` and `Status` one of the listed values.
- A section that has nothing yet says so in one line, for example "Not designed yet." under Guarantees and limits.
- Values carry units, and a value from a source that is marked "re-measure" or "assumption" keeps that mark.
- A requirement states an outcome, never a mechanism, and it names at least one defined `QG-nn` goal, also a technical one.
  A requirement without a goal has no reason to exist.
- A requirement is one table row on one line, with no `|` inside a cell.
  Its ID starts with the ID of its contract, such as `IF-06-R01` inside `IF-06`.
- "Met by" is `open` or a link to what fulfils the requirement, such as an ADR, the protocol page or a Mechanics page.
  A link to a System or Station page must name a page that exists.
- Write only what the owner confirmed, and let the owner confirm the goals of each requirement.
- A decision changes a contract in this order.
  First write the ADR.
  Then update "Met by" and the guarantees of the contract.
  Never change an ID and never reuse one, also when a requirement is dropped.
- Subprojects only link contracts, with `xref:system::contracts.adoc#IF-05[IF-05]`, and never define them.

## Runtime scenarios

A runtime scenario shows in which order the blocks and the people act in one situation, and which contract each step relies on.
A chapter 6 page holds the scenarios of normal operation.
A fault page under chapter 6 holds the scenarios of faults.
Each scenario is a level-2 section with an explicit kebab-case anchor, so other pages can link it.

```adoc
[[switching-on]]
== Switching On

<one or two sentences: the trigger and the end state>

.Switching on
[d2, target="system-6-switching-on", format="svg", link=self]
----
include::system:ROOT:partial$d2-style.d2[]
shape: sequence_diagram
...
----
<one-line caption that names the question of the diagram>

=== Contracts
* xref:contracts.adoc#IF-03[IF-03], xref:contracts.adoc#IF-03-R04[IF-03-R04]: <one line on how the scenario relies on it>

=== Open
* <one line per open behaviour, with the open requirement it depends on, or "Nothing open.">

=== Details
* <links to the Station sequence, the protocol pattern or the Mechanics page that show the details, or "No further details.">
```

- Keep the three level-3 sections and their order.
- Each scenario has exactly one D2 block, with `shape: sequence_diagram` and the target `<component>-6-<anchor>`, as the `writing-diagrams` skill names it.
  In System the target is `system-6-<anchor>`, so it never clashes with a subproject target.
- Take the participants from chapter 3 and chapter 5, and show only the ones the scenario needs.
- One arrow may stand for a whole exchange.
  The protocol page or the runtime view of the subproject shows the messages.
- Show behaviour that is not decided as open, with the `(open, <ID>)` label that the `writing-diagrams` skill describes.
  `<ID>` is a defined `IF-nn-Rnn` or `IF-nn`, and a label may name several IDs, separated by commas.
  Use a `fault` edge only for a fault that occurs.
- Every `Contracts` entry and every `Open` bullet is one line and names a defined `IF-nn` or `IF-nn-Rnn`.
  Only when nothing is open, the `Open` section is exactly the line "Nothing open.".
- Every ID of an `(open, <ID>)` label also appears in the `Open` section, as a whole ID.
  A bullet that names only `IF-01-R11` does not cover `IF-01`.
- A scenario anchor is lowercase kebab-case and never looks like an ID.
- The first sentence of a fault scenario names its end state, or says that the end state is open.
  Never end in a safe state that nobody decided.
- Write only what the owner confirmed, and mark every step as taken from a source or as an assumption in the draft.
- A subproject keeps the software details in its own runtime view and links the System scenario.

## Quality scenarios

A quality scenario makes a quality goal measurable.
The Quality Requirements page of System chapter 10 holds one section per scenario, with an explicit anchor.
The page also lists the further goals (goals below the top goals of chapter 1, such as `QG-06`) in the section `== Further Quality Goals`.
The top goals are the table of the Quality Goals page, and the further goals use the same table form.

```adoc
[cols="1,2,5,3", options="header"]
|===
| ID | Quality Goal | Scenario | Q42

| [[QG-06]]QG-06
| *Maintainability*
| <the goal as one sentence>
| `#maintainable`
|===
```

In both tables the order of the rows is the priority.

The heading is level 2, or level 3 under a `== Scenarios` section.
The tags are the nine tags of the arc42 quality model Q42, see SYS-014: `#efficient`, `#flexible`, `#maintainable`, `#operable`, `#reliable`, `#safe`, `#secure`, `#suitable` and `#usable`.

```adoc
[[QS-01]]
== QS-01: <short title>

Goal:: xref:quality-goals.adoc#QG-01[QG-01]
Q42:: #safe
Source:: <what triggers it>
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

- Keep the twelve fields and their order.
  A value may run over several lines, with one sentence per line, until the next field.
- A scenario has exactly one goal, and `Q42` names one or more tags, at least one of them a tag of that goal.
  `Goal` links the page that defines the goal: `quality-requirements.adoc` for `QG-06` and `quality-goals.adoc` for `QG-01` to `QG-05`.
- A scenario states what the machine must reach, never a mechanism.
- `Source` names a trigger, never an author.
- `State` is `Target` (only the owner sets it), `Provisional` (a docs page or an ADR states the value, and it is still to be confirmed on the assembled machine), `Open` or `Withdrawn`.
- `Value` is `none` exactly when `State` is `Open` or `Withdrawn`.
  Otherwise it is a number and a unit, or a count with a noun, such as `0 drinks`.
- `Checks` holds only xrefs: to a defined `IF-nn` or `IF-nn-Rnn`, to a scenario anchor on the runtime view or the fault scenarios page, and to any other existing page of System or Station, with or without an anchor.
  An ADR is such a page.
  Mechanics pages are accepted without a check.
- A withdrawn scenario keeps its section and its ID.
- The chapter also holds the quality tree and a traceability table, and both agree with the sections.
  The table columns are `QS | Goal | Q42 | Artifact | Verification | State | Value`.
  The tree has one block per goal, and each scenario node sits inside the block of its goal.

## Diagrams

All diagrams are D2, rendered through Kroki.
Write each diagram as a `[d2]` block on the page.

### Diagram style

Follow the `writing-diagrams` skill for every diagram.
It covers the diagram type per chapter, boxes, arrows, layout, names and the shared style.

## Before you finish

- Each page title carries the chapter number, and no file name does.
- The page is in the right component and in the right chapter.
- Every System block in chapter 5 links to its subproject, and each subproject chapter 5 links back.
- Every cross-component xref names the component in its link text.
- How-to content is in a guide and not in a chapter.
- No subproject repeats the quality goals, the naming conventions or a System glossary term.
- Every diagram is a D2 block.
- Every quality scenario has the twelve fields in order, and a value that fits its state.
- No page names code, and no source is an owner decision: each source is a docs page or an ADR.
- The prose follows `writing-style`, and the checker reports no errors.
