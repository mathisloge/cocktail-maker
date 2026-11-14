# Docs Overhaul Piece 4: Dark Theme and D2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:**
- Make the whole docs site dark in the kiosk palette, generated from `colors.slint`.
- Pin the Antora UI bundle and add a reference page for checking the look.
- Move all 17 diagrams to D2 with one shared style, rendered in CI by a pinned Kroki.

**Architecture:**
- **Generator:** a stdlib Python script reads the `Primitives` global in `gui/ui/theme/colors.slint` and writes two files. One is `palette.css` for the docs UI. The other is `d2-style.d2`, a System partial that every diagram includes. A pre-commit check keeps both in sync.
- **Dark theme:** one override stylesheet. A built-site test parses the pinned default UI CSS and fails on any light background the overrides miss.
- **Diagrams:** each includes the shared style. They are checked locally through the public kroki.io and in CI through a pinned Kroki service container with diagram fetching on, so a broken diagram fails the build.

**Tech Stack:** Python 3.12 standard library, pytest, Antora 3 default UI with supplemental CSS, AsciiDoc, D2 via `asciidoctor-kroki`, GitHub Actions service containers.

**Spec:** `docs/superpowers/specs/2026-11-13-docs-overhaul-design.md`, section "Piece 4: dark theme and D2". The diagram rules are in `plugins/cocktail-maker-docs/skills/writing-diagrams/SKILL.md`, which implementers read before touching any diagram.

## Global Constraints

- **Colours:** come only from `palette.css` variables (`var(--cm-…)`) in every stylesheet except `palette.css`, and only from the generated `d2-style.d2` classes in diagrams. Raw hex values appear only in `colors.slint` and in the two generated files.
- **Generated files:** they are never edited by hand. Each starts with a comment naming the generator and the source.
- **Palette names:** `--cm-<primitive name>` for every primitive in `colors.slint`, for example `--cm-ink-raised`. Two derived shades:
  - `--cm-sand-muted` is the mix of sand and ink at 60 % sand, for normal edges.
  - `--cm-sand-soft` is the mix at 80 % sand, for edge labels.
  - Mix per channel, round half up, lowercase hex.
- **D2 look (spec, mockup B):**
  - **Background:** `ink`.
  - **`block`:** fill `ink-elevated`, stroke `amber`, font colour `cream`, border radius 4.
  - **`actor`:** person shape, fill `ink-elevated`, stroke `amber-light`, font colour `cream`.
  - **`external`:** fill `ink`, stroke `sand`, stroke-dash 4, font colour `sand`.
  - **`boundary`:** fill `ink-raised`, stroke `amber`, stroke-width 2, bold label, font colour `amber-light`.
  - **Normal edges:** stroke `sand-muted`, font colour `sand-soft`.
  - **`key` edge:** stroke `amber-light`, stroke-width 3, font colour `amber-light`.
  - **Font:** D2's default. Kroki offers no custom font.
- **Diagram migration:** all 17 diagrams are converted. Each keeps its content and its `target` name, and sequence diagrams use `shape: sequence_diagram`. Every converted block includes the shared style as its first line inside the block: `include::system:ROOT:partial$d2-style.d2[]`. That is the form the spike confirms.
- **Rendering:**
  - **Local:** the owner approved sending this repo's diagram sources to the public `https://kroki.io` to check that they compile.
  - **CI:** the docs build uses a pinned `yuzutech/kroki` service container, with `kroki-server-url` and `kroki-fetch-diagram` set.
- **Prose:** every new or changed sentence follows `writing-style`. The style checker reports 0 errors on `docs/system` and `docs/station`.
- **Never stage** `hw_simulator/client.cppm` or `.claude/settings.json`. `CLAUDE.md` is untracked.

## Review Focus

1. **White gaps:** the default UI sets light backgrounds on about 30 classes. A missed one shows as a white box on the dark page. Test in Task 2: every light background rule in the pinned `site.css` needs an override.
2. **Readable text:** body text, links, code, table rows and every admonition type must stay readable on the dark surfaces. Test in Task 2 with a contrast check of each palette pair the stylesheet uses for text, at a WCAG ratio of at least 4.5.
3. **Cross-component include:** a diagram in Station includes the System partial. If Antora does not resolve that include inside a diagram block, every diagram breaks. Test in Task 4 (the spike) before any migration.
4. **Generated files drift:** a changed `colors.slint` without regenerating must fail. Test in Task 1 (`--check` exits 1).
5. **Lost diagram content:** every node, edge and label of each old diagram must survive the conversion. Test in Tasks 6 and 7 with a per-diagram label list compared before and after.

---

## Shared commands

```bash
export PATH=<scratchpad>/node/bin:<scratchpad>/uvenv/bin:$PATH
rm -rf <scratchpad>/site && npx antora --log-failure-level warn <scratchpad>/playbook-local.yml   # clean strict build
DOCS_SITE_DIR=<scratchpad>/site uv run pytest docs/ui/tests docs/tests -q
uv run pytest -q
python3 plugins/cocktail-maker-docs/scripts/check_adoc_style.py $(git ls-files 'docs/system/*.adoc' 'docs/station/*.adoc') | grep -c ': error: '   # → 0
# compile one D2 source through kroki.io (exit 0 and an <svg in the output means it compiles)
curl -sf -X POST -H 'Content-Type: text/plain' --data-binary @<file.d2> https://kroki.io/d2/svg | head -c 200
```

---

### Task 1: Palette generator

**Files:**
- Create: `tools/palette/generate_palette.py` (executable, stdlib only), `tools/palette/tests/test_generate_palette.py`
- Generate: `docs/ui/supplemental/css/palette.css`, `docs/system/modules/ROOT/partials/d2-style.d2`
- Modify: `pyproject.toml` (`testpaths` and `pythonpath` gain `tools/palette/tests` and `tools/palette`), `.pre-commit-config.yaml` (a local hook), `docs/ui/tests/test_built_site.py` (a palette test that expected exactly seven variables now expects at least the seven names)

**Interfaces:**
- **Produces:**
  - `read_primitives(slint_text: str) -> dict[str, str]`: the `name → #rrggbb` pairs of the `global Primitives { … }` block, in file order, lowercased. It ignores comments and fails with `ValueError` if the block is missing.
  - `mix(a: str, b: str, weight_a: float) -> str`
  - `render_css(colors: dict[str, str]) -> str`
  - `render_d2(colors: dict[str, str]) -> str`
  - `main(argv) -> int` with `--check`. Without it, `main` writes both files. With it, `main` exits 1 and names the stale file when either differs from what it would generate.
- **`render_d2` output:**
  - a header comment
  - `vars: { d2-config: { layout-engine: elk } }`
  - `style.fill: "<ink>"`
  - a `classes: { … }` block with `actor`, `external`, `boundary`, `block` and `key`, with the values from Global Constraints
  - root edge defaults, expressed as a glob `(** -> **)[*].style.stroke` and `.font-color`
- **The local pre-commit hook:**
  - id `palette-generated`
  - entry `tools/palette/generate_palette.py --check`
  - `language: script`, `pass_filenames: false`
  - `files: ^(gui/ui/theme/colors\.slint|tools/palette/.*|docs/ui/supplemental/css/palette\.css|docs/system/modules/ROOT/partials/d2-style\.d2)$`

- [ ] **Step 1: Write the failing tests**

```python
def test_read_primitives_from_the_real_file():
    colors = read_primitives((REPO / "gui/ui/theme/colors.slint").read_text())
    assert colors["ink"] == "#0a0806" and colors["log-gray-blue"] == "#7a8594"
    assert list(colors)[:3] == ["ink", "ink-raised", "ink-elevated"]

def test_read_primitives_rejects_missing_block():
    with pytest.raises(ValueError): read_primitives("global Palette {}")

def test_mix():
    assert mix("#d9cfae", "#0a0806", 1.0) == "#d9cfae"
    assert mix("#ffffff", "#000000", 0.5) == "#808080"

def test_css_has_every_primitive_and_both_derived_shades(): ...
    # "--cm-ink: #0a0806;" in css; "--cm-sand-muted:" and "--cm-sand-soft:" present; first line is a generator comment

def test_d2_has_the_five_classes_and_background(): ...
    # each of actor/external/boundary/block/key appears as a class; 'style.fill: "#0a0806"' present; no class value outside the palette

def test_check_mode_detects_drift(tmp_path): ...
    # write generated files into tmp copies, edit one, main(["--check", ...paths]) == 1; unchanged == 0
```

`main` accepts optional `--slint`, `--css` and `--d2` paths for the test. The defaults are the repo paths.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `uv run pytest tools/palette -v`
Expected: `ModuleNotFoundError: generate_palette`

- [ ] **Step 3: Implement the generator, run it, and add the hook**

- [ ] **Step 4: Compile the generated `d2-style.d2` through kroki.io**

Compile it with a one-node test diagram appended (`x: Station {class: block}`).
Expected: an `<svg` response.

- [ ] **Step 5: Run the tests, the clean strict build, the site tests and `uv run pre-commit run --all-files`**

Expected: all pass.

- [ ] **Step 6: Commit**

```bash
git add tools/palette pyproject.toml .pre-commit-config.yaml docs/ui/supplemental/css/palette.css docs/system/modules/ROOT/partials/d2-style.d2 docs/ui/tests/test_built_site.py
git commit -m "feat(docs): generate the docs palette and the D2 style from colors.slint"
```

---

### Task 2: Pinned UI bundle and the dark theme

**Files:**
- Modify: `antora-playbook.yml` (`ui.bundle.url` pinned, `snapshot` removed)
- Create: `docs/ui/supplemental/css/dark.css`
- Modify: `docs/ui/supplemental/partials/head-styles.hbs` (load `dark.css` after `site-extra.css`), `docs/ui/supplemental/css/site-extra.css` (the sidebar rules from piece 2 that assumed a light sidebar, now amber text on dark)
- Create: `docs/ui/tests/test_dark_theme.py`

**Interfaces:**
- Consumes: `palette.css` from Task 1, and `SITE_DIR`, `parse_page` and the `site_test` gating from `docs/ui/tests`.
- **Pinning:** the bundle URL points to one fixed build of `antora-ui-default`. Use the job artifact URL of the build that `HEAD` resolves to today, found through the GitLab API. If no stable URL can be found, vendor the zip under `docs/ui/bundle/ui-bundle.zip` and point the playbook at it. Write the chosen URL or path, and how to update it, as a comment in the playbook.
- **`dark.css` scope (spec):** header, sidebar, toolbar, breadcrumbs, content, code blocks, tables, admonitions, footer, 404 page, plus the page-versions and TOC panels. Colours only via `var(--cm-…)`.

- [ ] **Step 1: Write the failing tests**

```python
@site_test
def test_every_light_background_has_a_dark_override():
    # parse <site>/_/css/site.css: collect selectors whose declarations set background/background-color
    # to a light colour (relative luminance > 0.5, ignoring transparent/inherit/none);
    # for each such selector, dark.css or site-extra.css must contain a rule for the same selector
    # (exact selector text after whitespace normalisation). Print every missing selector on failure.

def test_text_colour_pairs_are_readable():
    # for each (text, background) palette pair listed in dark.css as a /* contrast: --cm-x on --cm-y */ comment,
    # the WCAG contrast ratio of the generated values is >= 4.5

def test_dark_and_extra_css_use_only_palette_colours():
    # dark.css and site-extra.css contain no hex, rgb(), hsl() literal and no named colour other than
    # transparent, currentColor, inherit, initial
```

- [ ] **Step 2: Pin the bundle, rebuild, and run the tests to verify they fail**

Expected: the override test lists the missing selectors, and the other two tests fail because `dark.css` does not exist.

- [ ] **Step 3: Write `dark.css`, move the piece 2 sidebar rules onto the dark sidebar, and load `dark.css`**

- [ ] **Step 4: Rebuild clean and run all tests**

Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add antora-playbook.yml docs/ui/supplemental docs/ui/tests docs/ui/bundle 2>/dev/null
git commit -m "feat(docs): pin the UI bundle and make the whole site dark"
```

---

### Task 3: Reference page, page titles and the 404 page

**Files:**
- Create: `docs/system/modules/ROOT/pages/style-reference.adoc` (published, not in the nav)
- Create: `docs/ui/supplemental/partials/head-title.hbs`
- Modify: `docs/ui/tests/test_dark_theme.py`

**Interfaces:**
- **The reference page** holds one of each element:
  - headings to level 4
  - ordered, unordered and description lists
  - inline code, a listing block and a `[source,cpp]` block
  - a table with a header and a table copied from `protocol.adoc`
  - all five admonition types
  - an example block, a sidebar block and a quote block
  - an xref
  - a D2 diagram (after Task 4, a placeholder line until then)
  - the ADR header partial with Superseded status, through local attributes
- **The browser tab title** is `<page title> · <component title> · <site title>`, for example `5 Building Block View · Station · Sir-Mix-A-Lot Docs`. On a page without a component it is `<page title> · <site title>`.

- [ ] **Step 1: Write the failing tests**

```python
@site_test
def test_reference_page_has_every_element():
    # system/style-reference.html exists and contains: h2..h5, ol, ul, dl, pre, code, table.tableblock,
    # admonitionblock note/tip/important/caution/warning, exampleblock, sidebarblock, quoteblock

@site_test
def test_reference_page_not_in_nav():
    # no nav link to style-reference.html in system/index.html

@site_test
def test_page_title_names_the_component():
    # <title> of station/building-blocks.html == "5 Building Block View · Station · Sir-Mix-A-Lot Docs"
```

- [ ] **Step 2: Run the tests to verify they fail**

- [ ] **Step 3: Write the page and the head title partial**

- [ ] **Step 4: Rebuild clean, run all tests and the style check**

Expected: all pass, 0 style errors.

- [ ] **Step 5: Commit**

```bash
git add docs/system/modules/ROOT/pages/style-reference.adoc docs/ui/supplemental/partials/head-title.hbs docs/ui/tests
git commit -m "feat(docs): add the style reference page and component names in page titles"
```

---

### Task 4: Include spike

**Files:**
- Modify: `docs/station/modules/ROOT/pages/context-and-scope.adoc`, the first Station diagram, converted to D2 with the shared include
- Possibly create, only if the spike fails: `docs/ui/extensions/d2-style.js` (an Asciidoctor preprocessor that prepends the partial's text to every `[d2]` block), registered in `antora-playbook.yml` under `asciidoc.extensions`
- Modify: `docs/tests/test_docs_structure.py`

**Interfaces:**
- **Produces the include mechanism every later diagram uses.** Default: the first line inside the block is `include::system:ROOT:partial$d2-style.d2[]`. Fallback: no include line, and the extension adds the style.
- **Produces a test helper** `kroki_sources(html_path) -> list[str]`. It decodes the D2 sources from the Kroki image URLs in a built page (`/d2/svg/<base64url(zlib)>`).

- [ ] **Step 1: Write the failing test**

```python
@site_test
def test_d2_diagrams_carry_the_shared_style():
    # for station/context-and-scope.html: every decoded d2 source contains the generated d2-style.d2 text
```

- [ ] **Step 2: Convert the Station context diagram to D2 with the include line, rebuild, and run the test**

- [ ] **Step 3: If the test fails because the include is not resolved, switch to the extension fallback**

Record which mechanism holds in the report. Rebuild and rerun.

- [ ] **Step 4: Compile the decoded source through kroki.io**

Expected: `<svg`.

- [ ] **Step 5: Commit**

```bash
git add docs/station/modules/ROOT/pages/context-and-scope.adoc docs/tests antora-playbook.yml docs/ui/extensions 2>/dev/null
git commit -m "feat(docs): render the first diagram in D2 with the shared style"
```

---

### Task 5: The `writing-diagrams` visual style section

**Files:**
- Modify: `plugins/cocktail-maker-docs/skills/writing-diagrams/SKILL.md`, and `.claude-plugin/marketplace.json` plus `plugins/cocktail-maker-docs/.claude-plugin/plugin.json` (version `0.3.0`)

The "Visual style" section now says:
- The shared style lives in `docs/system/modules/ROOT/partials/d2-style.d2`, generated from `colors.slint`, and is never edited by hand.
- Every diagram uses the mechanism that Task 4 confirmed: the include line, or nothing because the extension adds the style.
- Delete the instruction about the empty local `classes` block.
- The class table stays.
- Change the AsciiDoc block example to match.
- Add one sentence on rendering: CI renders through its own Kroki, and an author can check a diagram through kroki.io.

- [ ] **Step 1: Edit the skill and bump the versions**

- [ ] **Step 2: Run `uv run pytest -q`**

Expected: the plugin layout tests and the writer-guidance scans pass.

- [ ] **Step 3: Commit**

```bash
git add plugins/cocktail-maker-docs .claude-plugin/marketplace.json
git commit -m "docs: describe the shared D2 style in the writing-diagrams skill"
```

---

### Task 6: System diagrams to D2

**Files:** the 9 diagrams in `docs/system/modules/ROOT/pages/`:
- `context-and-scope.adoc` (1)
- `building-blocks.adoc` (1)
- `power.adoc` (1)
- `hardware.adoc` (1, already D2, restyled)
- `naming-conventions.adoc` (1)
- `protocol.adoc` (4, sequence diagrams)

Also modify `docs/tests/test_docs_structure.py`.

**Interfaces:**
- Consumes: the include mechanism and `kroki_sources` from Task 4, and the class names from `writing-diagrams`.

- [ ] **Step 1: Record the label list of every old diagram**

Write one text file per diagram target into the scratchpad. It holds every node name, edge label and participant of the old diagram.

- [ ] **Step 2: Write the failing test**

```python
@site_test
def test_system_diagrams_are_d2():
    # no [plantuml or [mermaid block under docs/system; every [d2 block's decoded source carries the shared style
```

- [ ] **Step 3: Convert each diagram by the `writing-diagrams` rules**

Keep the content and the `target`. Mark the most important connection `key`, at most one.

- [ ] **Step 4: Compile every converted source through kroki.io, and compare each label list with the new source**

Every old label appears, possibly reworded only to the glossary term. List the rewordings in the report.

- [ ] **Step 5: Rebuild clean and run all tests and the style check**

- [ ] **Step 6: Commit**

```bash
git add docs/system docs/tests
git commit -m "docs: move the System diagrams to D2"
```

---

### Task 7: Station diagrams to D2

**Files:** the remaining 7 Station diagrams:
- `building-blocks.adoc` (1)
- `deployment-view.adoc` (1)
- `runtime-view.adoc` (5, sequence diagrams)

Also modify `docs/tests/test_docs_structure.py` and add the matching Station test.

Steps as in Task 6, with `test_station_diagrams_are_d2`, and the commit message `docs: move the Station diagrams to D2`.
Once this task is done, no `plantuml` or `mermaid` block is left anywhere under `docs/`.

---

### Task 8: Kroki in CI

**Files:**
- Modify: `.github/workflows/publish-docs.yml`, `docs/ui/tests/test_dark_theme.py` (or `docs/tests`)

**Interfaces:**
- **Service:** the build job gets `services: kroki: image: yuzutech/kroki:<pinned tag>`, with `ports: ["8000:8000"]`. Use the current stable tag, and write it as a comment so it can be bumped.
- **Build attributes:** "Generate Site" passes `--attribute kroki-server-url=http://localhost:8000 --attribute kroki-fetch-diagram=true`. Fetched SVGs land in the site, and a diagram that does not compile fails the build.
- **Wait step:** before the build, a step waits until `http://localhost:8000/health` answers, for at most 60 s.

- [ ] **Step 1: Write a test for the CI build output**

The test is skipped unless `DOCS_KROKI_FETCHED=1`.
It checks that no built page references `kroki.io` and that every diagram `<img>` points to a local `.svg` file that exists.
In the workflow, the "Check Site Navigation" step sets `DOCS_KROKI_FETCHED=1`.

- [ ] **Step 2: Edit the workflow**

- [ ] **Step 3: Validate**

Run `uv run pre-commit run check-yaml --all-files` and `uv run pytest -q`.
The new test is skipped locally.
The CI run itself happens after the owner pushes.

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/publish-docs.yml docs/ui/tests docs/tests
git commit -m "ci(docs): render diagrams with a pinned Kroki and fail on broken diagrams"
```

---

### Task 9: Second Mechanics handoff

**Files:**
- Create: `docs/superpowers/handoffs/2026-11-13-mechanics-diagrams-prompt.md`

The prompt is for the agent in `mathisloge/cocktail-maker-3d` (component `mechanics`). It is self-contained and follows `writing-style`. It tells the agent to:
- update the plugin to version 0.3.0
- read `writing-diagrams`
- convert every PlantUML and Mermaid diagram under `docs/` to D2 with the shared style mechanism from Task 4, keeping content and `target` names
- compare label lists before and after
- check each diagram through kroki.io
- send it only after the cocktail-maker branch with piece 4 is on `main`, because the include resolves against the System partial there

- [ ] **Step 1: Write it**

- [ ] **Step 2: Run the style checker on it**

Expected: 0 errors.

- [ ] **Step 3: Commit**

```bash
git add docs/superpowers/handoffs/2026-11-13-mechanics-diagrams-prompt.md
git commit -m "docs: add the handoff prompt for the Mechanics diagrams"
```

---

## Not in this plan

- **Screenshot comparison in CI:** no browser in the devcontainer. The reference page is checked by eye after each UI or palette change.
- **Writing the placeholder chapters and the start page:** later rounds.

---

### Task 10: Edge kinds in colour (owner request)

Added by the owner after the piece 4 final review.
Diagrams carry information through colour again, with one fixed meaning per colour across all diagrams.

**Colours (owner choice):**

| Edge class | Meaning | Primitive | Hex |
|---|---|---|---|
| `power` | electrical power | `orange-red` (new) | `#d0654a` |
| `data` | data and communication | `slate-light` | `#86aebd` |
| `fluid` | liquid flow | `green` | `#5cad75` |
| `mechanical` | physical, mechanical links | `violet` (new) | `#8c7ee0` |
| `interaction` | a person using the machine | `amber-light` | `#e8b86d` |
| `fault` | faults and error paths | `pink` (new) | `#e07aa8` |

**Files:**
- Modify: `gui/ui/theme/colors.slint` (three new primitives `orange-red: #d0654a`, `violet: #8c7ee0` and `pink: #e07aa8`, unused by the GUI)
- Modify: `tools/palette/generate_palette.py`, `tools/palette/tests/test_generate_palette.py`; regenerate `palette.css` and `d2-style.d2`
- Modify: `docs/tests/test_docs_structure.py` (edge class rule), `docs/ui/tests/test_dark_theme.py` if the palette test lists colours
- Modify: the diagrams whose edges have a kind (at least the hardware diagram, which had these kinds colour-coded before; power, building blocks, context and the protocol and runtime sequences where an edge clearly is power, data, fluid, mechanical, interaction or a fault)
- Modify: `docs/system/modules/ROOT/pages/style-reference.adoc` (a diagram that shows all six kinds and the key edge)
- Modify: `plugins/cocktail-maker-docs/skills/writing-diagrams/SKILL.md`, both plugin manifests (version `0.4.0`), and the minimum plugin version in `docs/superpowers/handoffs/2026-11-13-mechanics-docs-prompt.md`

**Rules:**
- Every edge carries exactly one kind class or `normal`. `normal` (muted sand) stays for edges without a kind.
- `key` no longer sets a colour. It sets stroke-width 3 and a bold label, and it combines with the kind class: `{class: [power; key]}`. At most one `key` per diagram. Confirm on kroki.io first that D2 0.9.0 applies both classes of an array (the kind colour and the key width). If it does not, report it and stop.
- Each edge kind class sets `style.stroke` and `style.font-color` to its colour. Every colour has a WCAG contrast of at least 4.5 on `ink` and on `ink-elevated`.
- Colour is never the only cue (owner change, red-green colour blindness): `data` is dashed (`stroke-dash: 4`), `mechanical` has short dashes (`stroke-dash: 2`), `power`, `fluid`, `interaction` and `fault` are solid, all kinds have width 2, and `normal` has width 1.
- In a D2 class array the later class wins, so `key` comes last: `{class: [data; key]}`. With `[key; data]`, the width of `data` resets the key width (checked on kroki.io).
- Boxes carry meaning through the outline only (owner addition): `power-part`, `data-part`, `mechanical-part` (load cell included), `fluid-part` and `interaction-part` are the `block` look with the outline in the kind colour, always solid, and there is no fault box class. Detailed diagrams (hardware, power) give every part its part class; overview diagrams use one only when a box clearly belongs to one area.
- A Diagram Conventions page (owner addition) under chapter 1, after Quality Goals, is the site's legend: a legend diagram with every shape and edge class, and tables with a CSS sample per class (`.edge-<class>` and `.shape-<class>` in `site-extra.css`, palette variables only). Tests keep the tables and the samples in step with `d2-style.d2`. The System start page links it.
- Diagram content stays the same. Only edge classes change.
- The colour meanings are fixed site-wide. The skill lists them in a table, and the reference page shows them. Diagrams still have no legend box. A caption may name the kinds a diagram uses.

- [ ] **Step 1: Prove class arrays on kroki.io** (a probe with `{class: [power; key]}` and a plain `{class: data}` edge, check stroke colours and widths in the SVG)
- [ ] **Step 2: Failing tests** (generator: the six classes with their colours, `key` without a colour; structure: every edge has exactly one of `normal`, `power`, `data`, `fluid`, `mechanical`, `interaction`, `fault`, optionally plus `key`, at most one `key` per diagram)
- [ ] **Step 3: Add the primitives, change the generator, regenerate, apply the kinds to the diagrams, update the reference page, the skill, the versions and the handoff**
- [ ] **Step 4: Verify** (all diagrams compile on kroki.io, rendered PNGs at 800 px look distinct, normal and fetched site tests, `uv run pytest`, palette check, style check, pre-commit)
- [ ] **Step 5: Commit** `feat(docs): colour diagram edges by kind`
