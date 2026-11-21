# Docs layout on wide screens and a compact ADR header

Date: 2026-11-21.

## Intent

On a wide screen the docs page sticks to the left, and more than half of the screen stays empty.
The text is small for the distance at which one reads a large screen.
The ADR header table takes much height for three values.

## Goals

* The article and its table of contents stand centred in the space right of the nav.
* The type grows in steps on wide screens, and the line length stays at about 75 characters.
* An ADR shows its status, date and authors in one line, with the status as a chip.
  A superseded ADR names its successor in the same line.

## Non-goals

* A wider text column.
  46rem is about 75 characters, which is the best length for running text.
* Wide diagrams and tables that break out of the column.
  This is a later step.
* Any change below 1800 px, apart from the ADR header and the centring.

## Approaches

The user chose the preview `wide.html` of this session.
The layout and the type are CSS only in `docs/ui/supplemental/css/site-extra.css`.
The ADR header needs a new `adr-header.adoc` partial, because CSS cannot turn the table into a line.

## Design

* `.content` centres its children, and `.doc` takes `flex: 0 1 46rem`.
* `html` gets `font-size: 1.1875em` from 1800 px and `1.25em` from 2200 px.
  The bundle sets 1.125em from 1024 px.
* The table of contents uses `--cm-sand` instead of `--cm-sand-soft`.
* The partial writes one paragraph with the role `adr-meta`.
  It holds a span with the roles `adr-status` and `adr-status-<status>`, then "by <successor>" for a superseded ADR, then the date and the authors, separated by " · ".
  The warning boxes for Deprecated and Superseded go, because the chip says the same.
* The status colours: Accepted `--cm-green`, Proposed `--cm-slate-light`, Deprecated and Superseded `--cm-ember-light`, each on `--cm-ink-raised`.
* Both copies of the partial, in `docs/system` and in `docs/station`, stay identical.

## Constraints

* Palette colours only, and a contrast comment on every rule that sets a text colour.
* The site test of the style reference page checks the Superseded header, so it changes from the table row and the warning to the chip and the successor link.
* The `writing-adrs` skill in `plugins/cocktail-maker-docs` describes the header, so its sentence changes too.
