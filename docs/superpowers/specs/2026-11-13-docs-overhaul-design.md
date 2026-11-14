# Documentation overhaul

## Intent

The Antora docs site (System, Station and the 3D repo's Mechanics) should read like one piece and look like the machine.
Today it doesn't:

- **Navigation:** the header is the Antora default with placeholder links (Home, Products, Services, Download). Switching between parts happens through a dropdown at the bottom of the sidebar.
- **Structure:** each part has its own layout. System uses its own page set, Station follows arc42, and Mechanics is product-style. Every part numbers its ADRs from 001, so "ADR-003" is ambiguous.
- **Diagrams:** three tools (PlantUML, Mermaid, D2) give three different looks.
- **Writing rules:** nothing written down or checked. Station hard-wraps at about 100 columns, while the other parts put one sentence per line.

What the owner wants: a clear guiding thread, more architecture documentation, nicer diagrams with D2, and a site in the kiosk's dark art-deco look.
The writing rules should live in one place that every project repo can use.

### How the parts relate

- **System** is the parent. It holds the architecture and the decisions for the whole machine.
- **Station** (this repo), **Embedded** (firmware repo, will move to Antora later) and **Mechanics** (3D repo, Pod assembly will be documented there later) are subprojects.

## Goals

- One structure across all parts: full arc42 in System and a fixed arc42 subset with the same chapter numbers in each subproject.
- A header that reflects the hierarchy and replaces the sidebar dropdown.
- A dark site in the kiosk palette. Kiosk colours, docs colours and diagram colours come from one source.
- All diagrams in D2 with one shared style.
- Writing rules and architecture rules as Claude Code skills in a plugin that other repos enable. A pre-commit hook enforces the mechanical rules.

## Non-goals

- **The start page:** it waits until the machine is built and there is something real to show. System's intro stays the start page. Later it should make readers curious and lead into the parts.
- **The missing System chapters (2, 6, 7, 10, 11):** they get placeholder pages now and are written later, one at a time.
- **The word-level rewrite:** replacing words from the avoid list in the existing text is a later pass.
- **Embedded:** no work in the firmware repo. When it moves to Antora, the skills give it the structure.
- **Mechanics:** no direct edits in the 3D repo. The deliverable is a handoff prompt for an agent working there.
- **Redirects:** no `page-aliases` for moved or renamed pages. Old URLs may break.
- **A light theme or a theme toggle:** the site is dark only, like the kiosk.
- **Switching away from Antora:** alternatives were considered and rejected (see Approaches).

## Anti-goals

- The kiosk's colours and the docs' colours drift apart.
- White or unstyled elements show through the dark theme.
- Rules that exist only as prose and erode because nothing checks them.
- Moved text that silently changes meaning. A move keeps the wording.
- Navigation that sends readers off-site without warning.

## Constraints

- **Stack:** Antora 3.x with the default UI as the base and `asciidoctor-kroki` for diagrams.
- **Build:** the site is built in CI (`.github/workflows/publish-docs.yml`) and published to GitHub Pages. The 3D repo is private and is cloned with the `DOCS_3D_TOKEN` secret.
- **Default UI:** its compiled CSS uses fixed colour values, not CSS variables. A dark theme must override each place that sets a colour.
- **Palette source:** the kiosk palette lives in `gui/ui/theme/colors.slint`, and the kiosk font is `gui/ui/theme/SpaceGrotesk.ttf`.
- **Language:** the docs are in English.

## Approaches

### Overall structure (chosen: A)

- **A, chosen. Full arc42 in System, a fixed subset in the subprojects.** The subprojects use the same chapter numbers as System. This gives the guiding thread with little duplication.
- **B. Full arc42 everywhere.** Most uniform, but it gives empty or duplicated chapters in Mechanics and Embedded (quality goals, glossary).
- **C. C4 only (context, container, component).** Lean, but it has no place for cross-cutting concepts, risks or quality. Station would also have to be rebuilt.

### Sharing the rules (chosen: plugin in this repo)

- **Chosen. A Claude Code plugin in this repo.** It is the home of System, and other repos enable it through their settings.
- **A separate small repo.** Lighter to clone, but the owner wants the rules next to the System docs.
- **Copies in each repo.** These drift apart, which is how the current inconsistency came about.

### Dark theme maintenance (chosen: A)

- **A, chosen. Pin the default UI and override colours from a generated palette.** Includes a hidden reference page that shows every element.
- **B. Fork the default UI and set its source variables.** Less CSS, but we would own its fiddly Gulp/Node build and merge upstream changes by hand.

### Generator (chosen: stay on Antora)

- **Astro + Starlight with an AsciiDoc loader.** Rejected. The loader is third-party and pre-1.0, there is no multi-repo support, and cross-component xrefs and includes would be lost.
- **Hugo Modules.** Rejected. AsciiDoc `include::` does not work across modules.
- **A newer Asciidoctor.js.** It would mainly modernise the JavaScript API and offers writers little. Antora 3.2 stays on Asciidoctor.js 2.2.

### Diagram style (chosen: B, dark)

Four mockups were compared: A kiosk light, B kiosk dark, C sketch, D D2 default.
The owner chose dark (B) and then chose to make the whole site dark so the diagrams blend in.

## Design

The work comes in four pieces, done in this order.
The rules from piece 1 shape how pieces 3 and 4 are written.

### Piece 1: rules as a plugin, plus the hook

**Plugin layout**

- `.claude-plugin/marketplace.json` at the repo root makes this repo the marketplace `cocktail-maker`.
- The plugin `cocktail-maker-docs` lives in `plugins/cocktail-maker-docs/`, with its manifest and four skills: `writing-style`, `architecture-docs`, `writing-adrs` and `writing-diagrams`.
- This repo is public, so the 3D repo (private) and others can load the marketplace and the hook from it without credentials.
- This repo enables the plugin in a committed `.claude/settings.json`. `settings.local.json` stays personal. Before the plugin is pushed, test it by adding the marketplace from the local checkout. Other repos add:

```json
{
  "extraKnownMarketplaces": {
    "cocktail-maker": { "source": { "source": "github", "repo": "mathisloge/cocktail-maker" } }
  },
  "enabledPlugins": { "cocktail-maker-docs@cocktail-maker": true }
}
```

**Skill `writing-style`**

It applies to all prose (docs, guides, READMEs, specs).

- Technical English in plain, common words. Inspired by ASD-STE100 Simplified Technical English, not bound to it.
- Short sentences in active voice.
- One term per concept, matching the glossary. For example, always "Pod", never "module" or "unit" for the same thing.
- One sentence per line.
- No semicolons.
- No dashes as sentence breaks (` - `, `–`, `—`). Hyphens in compound words such as "Station-Pod" stay allowed.
- An avoid list with replacements. It includes at least:
  - "utilize" and "leverage" → "use"
  - "facilitate" → "help" or "allow"
  - "robust" → name the concrete property
  - "in order to" → "to"
  - "seamless", "comprehensive", "crucial", "delve", "furthermore", "moreover" → drop them or say it plainly

**Skill `architecture-docs`**

It builds on `writing-style`.

- **The structure:** System uses all 12 arc42 chapters. Subprojects use chapters 1, 3, 4, 5, 6, 7, 8, 9 and 11. Chapter 2 is optional, only for constraints that belong to the subproject alone. Quality requirements, the glossary and global constraints live only in System.
- **Chapter numbers** are the same in every part.
- **Zoom links:** each block in System chapter 5 links to the building block view of its subproject. That view opens by saying where the subproject sits in the whole system.
- **Guides:** how-to content (printing, assembly, service) goes in a separate "Guides" area, apart from the architecture.
- **Placeholders:** an unwritten chapter gets a page with a "Not written yet" admonition and one or two lines on what belongs there.
- **Diagrams:** the D2 rules from piece 4, including how to include the shared style.

**Skill `writing-adrs`**

It builds on `writing-style`.

- **Template:** the existing structure with the `adr-header` partial and its statuses (Proposed, Accepted, Deprecated, Superseded).
- **When a decision earns an ADR:** the skill states this.
- **Where an ADR goes:** a decision that affects more than one part goes in System, and anything else goes in its own part.
- **ID prefix:** the title carries a component prefix: SYS, STA, EMB or CM3D (the 3D project's internal prefix), as in "SYS-007". File names stay as they are.

**Skill `writing-diagrams`** (added by the owner during piece 1)

It builds on `writing-style`, and `architecture-docs` points to it for every diagram.
In piece 1 it holds the content rules. Piece 4 adds the visual style (colours, shapes, the shared D2 style partial) to the same skill.

- **One diagram type per arc42 chapter:** context diagram in chapter 3, building blocks in chapter 5 (one zoom level per diagram), sequence diagrams in chapter 6, deployment diagram in chapter 7. Zoom levels are never mixed in one picture.
- **Title and caption:** every diagram has a title and a one-line caption that says which question it answers.
- **Boxes:** every box carries a name, a type and the technology (for example "Station · Raspberry Pi · Slint UI"). Names are exactly the glossary terms.
- **Arrows:** every arrow points one way and carries a verb or intent ("sends commands"). Physical quantities carry their unit ("230 V AC"). Protocols go in the label ("USB CDC").
- **Highlight:** at most one highlighted connection per diagram, the most important one.
- **Reading direction:** flows go left to right, hierarchies top to bottom. A diagram with more than about 10 boxes is split.
- **Consistency:** the same element keeps the same name, shape and colour in every diagram. Actors are person shapes, external systems are dashed, the machine or subproject is a framed boundary.
- **No legend boxes:** the shared style explains itself. Anything unusual goes in the caption.
- **Source and names:** the diagram source sits next to its text in the `.adoc` file. The `target` name follows `<component>-<chapter>-<topic>`.

**Pre-commit hook**

- **Where it comes from:** this repo provides it through `.pre-commit-hooks.yaml`, so other repos can use it with `repo: https://github.com/mathisloge/cocktail-maker`.
- **What it checks:** `.adoc` files.
- **What fails:** more than one sentence on a line, a semicolon, or a dash used as a sentence break.
- **What only warns:** words from the avoid list. The hook prints them and passes.
- **What it skips:** listing and literal blocks, diagram blocks, tables, attribute entries, comments, and lines that only hold an xref or include.

**Mechanical pass**

In the same piece, the existing docs in this repo are brought up to the failing rules, so the hook passes from day one.
The scope is every `.adoc` file under `docs/system/` and `docs/station/`, which is what the hook checks.
Markdown files (READMEs, `CLAUDE.md`, specs, `gui/ui/CONTRIBUTING.md`) follow the `writing-style` skill from now on, but they are not reflowed in this round.

- Most of the work is Station's hard wrap turned into one sentence per line.
- The few semicolons and dash breaks are reworded with the meaning unchanged.

**Mechanics handoff**

Once the skills exist, write a prompt that an agent in the 3D repo can run. It should:

- enable the plugin and the hook
- bring the Mechanics docs up to the mechanical rules
- add the arc42 subset skeleton next to the existing pages, under "Architecture" and "Guides"
- add the CM3D prefix to the ADR titles
- move the Mechanics diagrams to D2 with the shared style, if there are any

### Piece 2: navigation

This piece builds on the uncommitted prototype in `docs/ui/supplemental/` (header, empty `nav-explore.hbs`, footer, `site-sections.js`, `site-extra.css`) and the `supplemental_files` line in `antora-playbook.yml`.

The prototype changes as follows:

- **Header:** the site title (with the glass mark) leads to System. Station, Embedded and Mechanics follow as a visually separate "subprojects" group. The current part has an amber underline.
- **Embedded:** the tab shows only when a component named `embedded` exists. There is no off-site link to the GitHub repo.
- **Sidebar:** the component dropdown stays removed.
- **Footer:** same sections as the header, plus the source link.
- **ADR titles** in the nav show the component prefix.

Rough header layout:

```
[glass] Sir-Mix-A-Lot   System  │  Station   Embedded   Mechanics            GitHub
```

### Piece 3: restructure

**System**

System is reorganised into the 12 arc42 chapters, one page per chapter, numbered in the nav.

| arc42 chapter | Source today |
|---|---|
| 1 Introduction and Goals | `index` + `quality-goals` |
| 2 Constraints | placeholder |
| 3 Context and Scope | `system-design`: System Context |
| 4 Solution Strategy | `system-design`: Key Technology Decisions |
| 5 Building Block View | `system-design`: Conceptual Building Blocks, plus `hardware` |
| 6 Runtime View | placeholder |
| 7 Deployment View | placeholder |
| 8 Cross-cutting Concepts | the `system-design` intro to Cross-Cutting Concepts, plus subpages: `protocol` (with `system-design`: Station-Pod Communication Protocol and Physical Transport), power (`system-design`: Power Architecture), safety (`system-design`: Safety Concept), `naming-conventions` |
| 9 Architecture Decisions | the System ADRs |
| 10 Quality Requirements | placeholder (quality scenarios; the goals stay in chapter 1) |
| 11 Risks and Technical Debt | placeholder |
| 12 Glossary | `system-design`: Domain Terminology |

- `system-design.adoc` is split up and removed. Every section of it has a target in the table. Power and safety text is moved, not written new.
- **Zoom links, my call:**
  - **Station:** System chapter 5 links to Station's building block view.
  - **Mechanics:** it links to the Mechanics start page until the handoff in the 3D repo adds Mechanics's chapter 5, then it is pointed there. CI builds the 3D repo's `main`, so a link to a page that does not exist yet would only produce a warning. The start page avoids even that.
  - **Embedded:** the block stays plain text until the `embedded` component exists.
- **ADR references, my call:** every reference to an ADR ID changes to the prefixed form. That covers the ADR titles, the docs pages, `CLAUDE.md`, and the two C++ comments in `cocktail-maker/fill_level.cppm` and `core/strong_type.test.cpp`. Old IDs like "ADR-003" would otherwise stay ambiguous.
- Moved text keeps its wording, apart from the piece 1 mechanical rules.
- `adr/999-example.adoc` is removed or moved out of `pages/`, so it is no longer published.
- The ADR file `002-pysical-pod-connection.adoc` keeps its misspelled name. Renaming it would break links, and redirects are a non-goal.

**Station**

Station keeps its pages and content.
The nav follows the subset numbering: 1 `index`, 2 `constraints`, 3 `context-and-scope`, 4 `solution-strategy`, 5 `building-blocks`, 6 `runtime-view`, 7 `deployment-view`, 8 `cross-cutting-concepts`, 9 ADRs, 11 `risks-and-technical-debt`.
Station chapter 5 gets its zoom line back to System chapter 5.

**Mechanics**

Mechanics is covered by the handoff prompt from piece 1.

### Piece 4: dark theme and D2

**Palette generation**

A script reads `gui/ui/theme/colors.slint` and generates two files:

- **a CSS palette:** custom properties for the docs UI
- **a D2 style file:** the classes, the dark background and the edge styles

A pre-commit check fails when either generated file is out of date.

**Dark theme**

- **UI version:** the default UI bundle is pinned to a fixed version instead of `HEAD` with `snapshot: true`.
- **Override CSS:** it uses only palette variables, never raw hex values. It covers header, sidebar, toolbar, breadcrumbs, content, code blocks, tables, admonitions, footer and the 404 page.
- **Checklist:** the class list of the community Antora Dark Theme serves as a checklist of what needs a dark rule. It is not a dependency.
- **Font:** Space Grotesk, taken from the GUI theme. The prototype links to it rather than copying it.

**Reference page**

A published page that is not in the nav shows every element the docs use:

- headings and lists
- code and listing blocks
- tables, including the protocol tables
- all admonition types
- an xref
- a D2 diagram
- the ADR header

After a UI or palette change, this page is checked, by eye or by comparing screenshots.

**D2 migration**

- All 17 existing diagrams are converted to D2: 6 PlantUML, 10 Mermaid and the D2 diagram in `hardware.adoc`.
- They keep their content, and their `target` names stay.
- Sequence diagrams use D2's sequence diagram shape.

**D2 style**

- The generated style file sits as a partial in System. Every diagram in every repo includes it (for example `include::system::partial$d2-style.d2[]`).
- The look follows mockup B from the brainstorm. The colour names are the `colors.slint` primitives:
  - **Background:** `ink` (#0a0806).
  - **Building blocks:** fill `ink-elevated` (#1c1510), border `amber` (#c8913a), text `cream` (#f5eed8), secondary text `sand` (#d9cfae), corner radius 4.
  - **Actors (people):** person shape, fill `ink-elevated`, border `amber-light` (#e8b86d), text `cream`.
  - **External systems:** fill `ink`, dashed border `sand`, text `sand`.
  - **The machine or subproject boundary:** fill `ink-raised` (#120e0a), border `amber` 2 px, bold label in `amber-light`.
  - **Normal edges:** a muted sand line (#8a7f66) with labels in a lighter sand (#b5ab92). These two in-between shades are not in `colors.slint`. The generator derives them or the palette gains them.
  - **The key edge:** one per diagram at most, `amber-light`, 3 px, with an `amber-light` label. It marks the most important connection.
  - **Font:** Space Grotesk where D2 and Kroki allow a custom font, otherwise D2's default.

**Rendering**

- **First step of piece 4, a spike:** confirm that an AsciiDoc `include::` of the style partial works inside a D2 block, also from another component. If it does not, an Antora or Asciidoctor extension prepends the generated style to every D2 block instead. Diagrams then contain no include line.
- **CI:** renders with its own Kroki container at a pinned version (for example a GitHub Actions `services:` entry), through the `kroki-server-url` attribute. This keeps the build independent of kroki.io and keeps the D2 version stable, which the style depends on.
- **Local builds:** may keep using kroki.io.

## Left to the builder

- **Palette mapping:** which `colors.slint` tokens the generator reads, the CSS variable names, and how colours map to docs roles (links, code background, admonitions).
- **Script language:** the hook and the palette generator. Python fits the pre-commit setup here and in the 3D repo.
- **Page file names** for the new System chapters. Keep them stable and without numbers. The numbers go in titles and nav.
- **How to pin the default UI bundle:** a fixed artifact URL or a vendored zip.
- **The exact detection rules for sentence breaks** in the hook. Keep false alarms low and add an inline opt-out if needed.
- **The D2 layout engine:** whichever Kroki supports and gives readable results (elk or dagre).
- **How to bring the prototype header in line with piece 2 and the dark theme.**
- **Where the existing Mechanics pages go in the subset:** left to the agent in the 3D repo.
