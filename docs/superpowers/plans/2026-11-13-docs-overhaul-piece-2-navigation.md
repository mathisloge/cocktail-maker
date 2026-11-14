# Docs Overhaul Piece 2: Navigation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the Antora default header with the project navigation, remove the sidebar component dropdown, and add a project footer. A test over the built site guards all of it.

**Architecture:**
- **Prototype:** the uncommitted prototype in `docs/ui/supplemental/` is the starting point. A Handlebars helper splits the Antora components into the parent `system` and the subprojects. The header and footer partials render from it.
- **Colours:** come from CSS variables in one hand-written `palette.css`. Its variable names mirror `gui/ui/theme/colors.slint`, so piece 4 can generate the same file.
- **Test:** a pytest file reads a built site from `DOCS_SITE_DIR` and checks header, sidebar and footer on real pages. CI runs it after the docs build.

**Tech Stack:** Antora 3 default UI with supplemental files (Handlebars partials, a CommonJS helper, CSS), pytest with the Python standard library `html.parser`, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-11-13-docs-overhaul-design.md` (section "Piece 2: navigation"). It is the second plan on branch `docs-overhaul-piece-1`, after piece 1 (`docs/superpowers/plans/2026-11-13-docs-overhaul-piece-1-rules.md`).

## Global Constraints

- **Header:** the site title with the glass mark leads to System (the site start page). Then System, a divider, and the subprojects Station, Embedded and Mechanics as one visually separate group. The current part has an amber underline. GitHub sits at the right end.
- **Embedded:** shows only when a component named `embedded` exists. There is no off-site link to the firmware repo anywhere.
- **Unknown components:** a component that is not `system`, `station`, `embedded` or `mechanics` joins the subproject group after the known ones.
- **Sidebar:** the component dropdown (`nav-explore`) stays removed.
- **Footer:** the same sections as the header in the same order, plus the source link to `https://github.com/mathisloge/cocktail-maker`.
- **ADR prefixes:** the nav shows page titles, so the prefixes (SYS, STA, EMB, CM3D) appear when piece 3 renames the ADR titles. Piece 2 adds nothing for them.
- **Theme:** the dark theme and the generated palette are piece 4. Piece 2 styles only the header, the footer and the current sidebar item. It uses only `var(--cm-…)` colours from `palette.css`, never raw hex values outside that file.
- **Palette names:** the variable names mirror the `colors.slint` primitives: `--cm-ink`, `--cm-ink-raised`, `--cm-ink-elevated`, `--cm-amber`, `--cm-amber-light`, `--cm-cream`, `--cm-sand`. Values are copied from `colors.slint` (`#0a0806`, `#120e0a`, `#1c1510`, `#c8913a`, `#e8b86d`, `#f5eed8`, `#d9cfae`).
- **Font:** Space Grotesk comes from `gui/ui/theme/SpaceGrotesk.ttf` through a committed symlink, never a copy.
- **Prose:** all new prose follows the `writing-style` skill. This plan's files are Handlebars, JS, CSS and Python, so this only applies to comments and text shown on the page.
- **Out of scope:** the uncommitted `hw_simulator/client.cppm` (the owner's change) is never staged.

## Review Focus

1. **Nested pages:** on a page two folders deep, such as `system/adr/001-system-split.html`, every header and footer link must resolve to an existing file. Test in Task 1 (header) and Task 2 (footer).
2. **No `embedded` component:** the site shows no Embedded tab, no empty slot and no external link. Test in Task 1.
3. **Unknown component:** a component the helper does not know still appears, after the known subprojects. Test in Task 1 with a scratch component.
4. **No current component:** the 404 page (and any page without a component) renders the header with no `is-current` link. Test in Task 1, if the build writes `404.html`.
5. **Narrow screens:** the burger menu keeps working. The default UI's JavaScript needs `.navbar-burger` with `aria-controls="topbar-nav"` and a `#topbar-nav.navbar-menu`. Test in Task 1.

---

## File Structure

| Path | Responsibility |
|---|---|
| `antora-playbook.yml` | `ui.supplemental_files: ./docs/ui/supplemental` (the prototype line). |
| `docs/ui/supplemental/helpers/site-sections.js` | Splits components into `{ system, subprojects }`. |
| `docs/ui/supplemental/partials/header-content.hbs` | Header markup. |
| `docs/ui/supplemental/partials/footer-content.hbs` | Footer markup. |
| `docs/ui/supplemental/partials/nav-explore.hbs` | Empty, which removes the sidebar dropdown. |
| `docs/ui/supplemental/partials/head-styles.hbs` | Loads `site.css`, `palette.css`, `site-extra.css`. |
| `docs/ui/supplemental/css/palette.css` | The `--cm-…` colour variables (generated in piece 4). |
| `docs/ui/supplemental/css/site-extra.css` | Header, footer and sidebar styles, colours only through `var(--cm-…)`. |
| `docs/ui/supplemental/font/SpaceGrotesk.ttf` | Symlink to `../../../../gui/ui/theme/SpaceGrotesk.ttf`. |
| `docs/ui/tests/test_built_site.py` | Checks a built site from `DOCS_SITE_DIR`. |
| `docs/ui/tests/site_html.py` | Small HTML reader shared by the tests. |
| `pyproject.toml` | Adds `docs/ui/tests` to `testpaths` and to `pythonpath`. |
| `.github/workflows/publish-docs.yml` | Runs the site test after the build. |

## Local build for the tests

There is no Node in the devcontainer, and the 3D repo needs a token.
The tests run against a site built in the scratchpad:
- **Node:** `export PATH=<scratchpad>/node/bin:$PATH`.
- **Playbook:** `<scratchpad>/playbook-local.yml` points the 3D source at a local clone and reads this repo's working tree (`worktrees: true`).
- **Build:** `npx antora <scratchpad>/playbook-local.yml`, which writes `<scratchpad>/site`.
- **Run the tests:** `DOCS_SITE_DIR=<scratchpad>/site uv run pytest docs/ui/tests -v`.

For Review Focus 2 and 3, also build a second site with two scratch components:
- **Second playbook:** a copy of the playbook with `output.dir` set to `<scratchpad>/site-extra`.
- **Embedded:** a local git repo with `docs/antora.yml`, `name: embedded`, `title: Embedded`, and one page.
- **Unknown:** a local git repo with `docs/antora.yml`, `name: tools`, `title: Tools`, and one page.

The exact scratchpad path is in the controller's dispatch.

---

### Task 1: Header, helper and sidebar

**Files:**
- Modify: `docs/ui/supplemental/helpers/site-sections.js`
- Modify: `docs/ui/supplemental/partials/header-content.hbs`
- Keep: `docs/ui/supplemental/partials/nav-explore.hbs` (empty partial, as in the prototype)
- Create: `docs/ui/tests/site_html.py`, `docs/ui/tests/test_built_site.py`
- Modify: `pyproject.toml` (`testpaths`, `pythonpath`)
- Modify: `antora-playbook.yml` (commit the prototype's `supplemental_files` line)

**Interfaces:**
- **Produces, the helper `site-sections` (CommonJS, Antora UI helper signature `({ data: { root } })`):**
  - It returns `{ system, subprojects }`. Each section is `{ name, title, url }`, taken from `root.site.components[name]` (`url` is the component URL).
  - `system` is `null` when there is no `system` component.
  - `subprojects` lists `station`, `embedded` and `mechanics` in that order, each only if the component exists. Every other component follows, except `system`, sorted by title.
- **Produces, the header markup that the tests and Task 2 rely on:**
  - **Brand:** `a.navbar-item.brand`, with `href` = `relativize site.homeUrl`.
  - **Parent link:** `a.navbar-item.section-link.section-parent`, for System.
  - **Divider:** `span.section-divider` with `aria-hidden="true"`.
  - **Group:** `div.section-group` with `aria-label="Subprojects"`, holding one `a.navbar-item.section-link` per subproject.
  - **Current link:** the link of the current page's component also has class `is-current` and `aria-current="page"`.
  - **Burger and menu:** `button.navbar-burger` with `aria-controls="topbar-nav"`, and `div#topbar-nav.navbar-menu`.
  - **GitHub:** `a.navbar-item.github-link` in `div.navbar-end`.
- **Produces, `site_html.py`:**
  - `parse_page(path: Path) -> Page`.
  - `Page` has `links(css_class: str) -> list[Link]`, `has_class(css_class: str) -> bool`, and `text: str` (the whole document's text).
  - `Link` is a `NamedTuple(href: str, text: str, classes: frozenset[str], attrs: dict[str, str])`.
  - It is built on `html.parser.HTMLParser`.

- [ ] **Step 1: Build the current site and write the failing tests**

Build the main site and the extra site as in "Local build for the tests".
`test_built_site.py` skips every test with `pytest.mark.skipif` when `DOCS_SITE_DIR` is not set.
It reads which component folders exist under the site directory.
Tests, each over the pages `system/index.html`, `station/index.html` and, if they exist, `mechanics/index.html` and one ADR page under `system/adr/`:

```python
def test_brand_leads_to_system_start(page_path): ...
    # the brand href resolves (relative to the page) to <site>/system/index.html

def test_parent_then_divider_then_subprojects(page_path): ...
    # exactly one section-parent link with text "System"; one section-divider;
    # section-group link texts == expected subprojects in order:
    # ["Station", "Embedded"?, "Mechanics"?, then unknown titles sorted] based on existing component folders

def test_current_component_marked(page_path): ...
    # exactly one section-link has is-current and aria-current="page", and it is the page's component

def test_all_header_links_resolve(page_path): ...
    # every brand/section-link href (relative) points to an existing file

def test_no_dropdown_no_placeholders_no_external_embedded(page_path): ...
    # no element with class nav-panel-explore; text has none of "Products", "Services", "Download";
    # the page source does not contain "cocktail-maker-embedded"

def test_burger_menu_markup(page_path): ...
    # button.navbar-burger aria-controls == "topbar-nav"; an element with id "topbar-nav" and class navbar-menu exists

def test_404_has_no_current():  # skips if <site>/404.html does not exist
    ...
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `DOCS_SITE_DIR=<scratchpad>/site uv run pytest docs/ui/tests -v`
Expected: `test_parent_then_divider_then_subprojects` FAILS (no `section-parent` yet). The Embedded and dropdown tests fail on the prototype's external Embedded link.

- [ ] **Step 3: Rewrite the helper and the header partial to the interfaces above**

Remove the prototype's `fallbackUrl`, the external link markup and the `↗` mark.

- [ ] **Step 4: Rebuild both sites and run the tests to verify they pass**

Run: the two builds, then `DOCS_SITE_DIR=<scratchpad>/site uv run pytest docs/ui/tests -v` and `DOCS_SITE_DIR=<scratchpad>/site-extra uv run pytest docs/ui/tests -v`.
Expected:
- Both test runs pass.
- `uv run pytest` without `DOCS_SITE_DIR` reports the site tests as skipped and everything else as passed.

- [ ] **Step 5: Commit**

```bash
git add antora-playbook.yml pyproject.toml docs/ui/supplemental/helpers docs/ui/supplemental/partials/header-content.hbs docs/ui/supplemental/partials/nav-explore.hbs docs/ui/tests
git commit -m "feat(docs): add the project navigation header and drop the component dropdown"
```

---

### Task 2: Footer, styles and CI

**Files:**
- Modify: `docs/ui/supplemental/partials/footer-content.hbs`, `docs/ui/supplemental/partials/head-styles.hbs`, `docs/ui/supplemental/css/site-extra.css`
- Create: `docs/ui/supplemental/css/palette.css`
- Commit: `docs/ui/supplemental/font/SpaceGrotesk.ttf` (existing symlink, check it with `git ls-files -s` mode `120000`)
- Modify: `docs/ui/tests/test_built_site.py`, `.github/workflows/publish-docs.yml`

**Interfaces:**
- Consumes: the helper's `{ system, subprojects }` and the header markup from Task 1, plus `parse_page` and `Link` from `site_html.py`.
- **Produces, the footer markup:**
  - **Brand:** `div.footer-brand` with the site title.
  - **Sections:** `ul.footer-links` holds the sections as `a.footer-section`, System first, then the subprojects in header order.
  - **Source link:** then one `a.footer-source` to `https://github.com/mathisloge/cocktail-maker`.
- **Produces, `palette.css`:** a `:root` block with exactly the seven `--cm-…` variables and the values from Global Constraints, plus a comment saying that piece 4 generates this file from `colors.slint`.
- **Produces, `head-styles.hbs`:** loads `css/site.css`, `css/palette.css`, `css/site-extra.css`, in that order.
- **`site-extra.css`:**
  - **Kept from the prototype:** the `@font-face`, the header, the footer and the current sidebar item (`.nav-item.is-current-page > .nav-link`, `.nav-menu .title a`). Every colour is `var(--cm-…)`.
  - **New:** `.section-divider` (a thin vertical line in `--cm-sand` at reduced opacity) and `.section-group`.
  - **Removed:** the prototype's own `:root` variables, its "sidebar and content" link colours for `.doc a`, and its heading fonts. Those belong to piece 4.

- [ ] **Step 1: Write the failing tests**

Add to `test_built_site.py`:

```python
def test_footer_matches_header(page_path): ...
    # footer-section texts == ["System"] + header section-group texts; footer-source href == the GitHub URL

def test_all_footer_links_resolve(page_path): ...
    # every footer-section href (relative) points to an existing file

def test_styles_loaded_in_order(page_path): ...
    # the stylesheet hrefs end with css/site.css, css/palette.css, css/site-extra.css in this order
    # and <site>/_/css/palette.css and <site>/_/font/SpaceGrotesk.ttf exist
```

And one test without a built site:

```python
def test_site_extra_uses_only_palette_colours():
    # docs/ui/supplemental/css/site-extra.css contains no "#" hex colour and no rgb()/hsl() literal
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `DOCS_SITE_DIR=<scratchpad>/site uv run pytest docs/ui/tests -v`
Expected: the footer and style tests FAIL, and `test_site_extra_uses_only_palette_colours` FAILS on the prototype's `:root` hex values.

- [ ] **Step 3: Write the footer, `palette.css`, `head-styles.hbs` and the trimmed `site-extra.css`**

- [ ] **Step 4: Add the CI step**

In `publish-docs.yml`, after "Generate Site", add `astral-sh/setup-uv` (pinned major, as in `build-and-test.yml`) and a step `DOCS_SITE_DIR=build/site uv run pytest docs/ui/tests -v`.

- [ ] **Step 5: Rebuild both sites and run all tests**

Run: the two builds, `DOCS_SITE_DIR=… uv run pytest docs/ui/tests -v` for both sites, `uv run pytest`, and `uv run pre-commit run --all-files`.
Expected: everything passes.

- [ ] **Step 6: Commit**

```bash
git add docs/ui/supplemental/partials/footer-content.hbs docs/ui/supplemental/partials/head-styles.hbs docs/ui/supplemental/css docs/ui/supplemental/font docs/ui/tests .github/workflows/publish-docs.yml
git commit -m "feat(docs): add the project footer and palette-based navigation styles"
```

---

## Not in this plan

- **The dark theme for all pages, the generated palette, the pinned UI bundle and the reference page:** piece 4.
- **ADR title prefixes and the arc42 restructure:** piece 3.
- **The deferred start page:** a later round.
