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
| `QG-nn` | quality goal | System chapter 1, Quality Goals page |
| `QS-nn` | quality scenario | System chapter 10 |
| `SM-nn` | safety measure | System chapter 8, Safety page |
| `IF-nn` | contract | System chapter 5, Contracts page |
| `IF-nn-Rnn` | requirement of a contract | System chapter 5, Contracts page, in the requirements table of contract `IF-nn` |

- Two digits, zero-padded, for example `QG-01`.
- An ID is never changed and never reused, even when its item is removed.
- IDs are defined only in System.
  Subprojects refer to them with `xref:system::quality-goals.adoc#QG-01[QG-01]`.

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
- The prose follows `writing-style`, and the checker reports no errors.
