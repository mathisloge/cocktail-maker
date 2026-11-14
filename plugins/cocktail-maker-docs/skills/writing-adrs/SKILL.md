---
name: writing-adrs
description: Use when a design decision is made or changed in any Sir-Mix-A-Lot project, also mid-coding, to decide whether it needs an ADR, where it goes, and to write or supersede it.
---

# Writing ADRs

This skill builds on `writing-style`.
An ADR (architecture decision record) records one design decision, the reasons for it and its costs.

## When a decision earns an ADR

Ask this question also in the middle of coding, each time you make a design choice.
Write an ADR when at least one of these is true:

- The decision is hard to reverse.
- It affects more than one module or part.
- It chooses between real alternatives that someone will question later.

If none is true, a code comment or the commit message is enough.
If the decision changes an existing ADR, supersede that ADR.

## Where an ADR goes

A decision that affects more than one part goes in `system`.
Anything else goes in the `adr` module of its own part.
The ADR pages are in `modules/adr/pages/` of the component.

Each component has its own prefix:

| Component | Prefix |
|---|---|
| `system` | `SYS` |
| `station` | `STA` |
| `embedded` | `EMB` |
| `mechanics` | `CM3D` |

## IDs and titles

The ID is the prefix of the component and the number from the file name.
The title holds the ID and the decision as a short phrase:

```adoc
= SYS-007: <decision as a short phrase>
```

The number comes from the file name, here `007-...adoc`.
A new ADR takes the next free number in its component.

File names never change, also when the title changes.
Numbers are never reused, also not the numbers of deprecated or superseded ADRs.

A component that is not migrated yet can still have titles in the old `ADR-NNN` form.
Give a new ADR the new form.

## Statuses

Set the status in the `:adr-status:` attribute.

- `Proposed`: the decision is open for discussion.
- `Accepted`: the decision is in effect.
- `Deprecated`: the decision is no longer in effect, and no ADR replaces it.
- `Superseded`: a newer ADR replaces the decision.

The header partial shows a warning on deprecated and superseded ADRs.

## Supersede an ADR

Fix typos and broken links in place.
When the decision itself changes, write a new ADR and keep the old one.
Then set the old ADR to superseded:

```adoc
:adr-status: Superseded
:adr-superseded-by: xref:009-new-decision.adoc[SYS-009]
```

In the Context of the new ADR, name the old ADR and link to it.

## Write an ADR

1. Copy `adr-template.adoc` from this skill folder to `modules/adr/pages/NNN-short-name.adoc` in the component.
2. Set the prefix and the number in the title.
3. Fill in the attributes and replace every placeholder.
4. Add the page to the `nav.adoc` of the component, as a nested item under chapter 9.

```adoc
* xref:architecture-decisions.adoc[]
** xref:adr:008-short-name.adoc[]
```

5. Add `* xref:adr:008-short-name.adoc[]` to the ADR list on the chapter 9 page, `architecture-decisions.adoc` in the `ROOT` module.

The template has these sections:

- Context: the problem, the facts that shape it and the ADR that this one supersedes.
- Decision Drivers: the forces that the decision must satisfy, one bullet each.
- Decision: what we do, in one or two active sentences.
- Alternatives Considered: each real alternative and why it lost.
- Consequences: the good and the bad results, one bullet each.

Name at least one bad consequence.
A decision without costs is usually a decision that nobody looked at closely.

## Before you finish

- The decision earns an ADR, and it is in the right component.
- The title has the right prefix and the number from the file name.
- The status is set, and a superseded ADR points at its successor.
- The page is in the `nav.adoc` of the component and in the ADR list on the chapter 9 page.
- The prose follows `writing-style`, and the checker reports no errors.
