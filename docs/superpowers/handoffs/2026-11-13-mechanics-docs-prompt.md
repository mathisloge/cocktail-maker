# Mechanics docs: bring them in line with the Sir-Mix-A-Lot docs rules

You work in the repo `mathisloge/cocktail-maker-3d`.
Its Antora component is `mechanics`, with the docs under `docs/`.
The nav is `docs/modules/ROOT/nav.adoc` and the ADRs are under `docs/modules/adr/pages/`.
Python tooling with uv and commitizen is already set up here.

The main repo `mathisloge/cocktail-maker` has a plugin with the shared docs rules, a style checker and a shared diagram style.
Your goal is to apply these rules to the Mechanics docs.
Work in small steps and make one conventional commit for each step.
The repo checks commit messages with commitizen.

Send this prompt only after the branch of the main repo with the docs overhaul is on its `main`.
The diagrams include a style partial from the System component, and that include resolves against `main` there.
The docs build of this repo also runs against the playbook of the main repo.

## Step 1: Enable the plugin

Add this to `.claude/settings.json`, or merge it into the existing file.

```json
{
  "extraKnownMarketplaces": {
    "cocktail-maker": { "source": { "source": "github", "repo": "mathisloge/cocktail-maker" } }
  },
  "enabledPlugins": { "cocktail-maker-docs@cocktail-maker": true }
}
```

If `.gitignore` ignores `.claude/`, change that entry to these two lines so `settings.json` can be committed.

```text
.claude/*
!.claude/settings.json
```

The plugin must be at version 0.6.0 or newer.
That version holds the contract rules, the diagram rules, the shared diagram style, the rules for the layout engine, the System glossary and the rules for the quality goals of an ADR.
Then restart the session or reload the plugins.
Use these four skills from the plugin for all the work below: `writing-style`, `architecture-docs`, `writing-adrs` and `writing-diagrams`.
Read them before you change any page.

## Step 2: Add the pre-commit hook

Add this to `.pre-commit-config.yaml`.

```yaml
- repo: https://github.com/mathisloge/cocktail-maker
  rev: <commit on main>
  hooks:
    - id: adoc-style
      files: ^docs/.*\.adoc$
```

Replace `<commit on main>` with the commit on `main` of the main repo that contains this docs work, or with a release tag once one exists.
Do not use the SHA of a feature branch, because it disappears when the merge rewrites history.
The SHA must be reachable on GitHub.

## Step 3: Fix the layout

Run the checker with `--fix` on all `docs/**/*.adoc` files.
You can use a checkout of the main repo or the installed plugin copy.
The script is `plugins/cocktail-maker-docs/scripts/check_adoc_style.py`.

```sh
python3 <path-to-cocktail-maker-checkout>/plugins/cocktail-maker-docs/scripts/check_adoc_style.py --fix $(git ls-files 'docs/**/*.adoc')
```

The option `--fix` only moves line breaks.
It never changes a word.
So this command must print 0 afterwards:

```sh
git diff --word-diff=porcelain | grep -E '^[-+][^-+]' | wc -l
```

If it prints anything else, undo the change and find the cause.
Commit the layout change on its own.
It still has semicolon and dash errors, so the hook from Step 2 would reject it.
Make this one commit with `SKIP=adoc-style git commit ...`.
Never use `--no-verify` and never disable the hook in any other way.
Every later commit runs the hook as normal.

## Step 4: Reword semicolons and dashes

Run the checker without `--fix`.
Reword each remaining semicolon and dash break by hand, without changing the meaning.
Do not touch the avoid-word warnings.
List them in your final report instead.
Commit this as its own step.

## Step 5: Restructure the nav

Split `docs/modules/ROOT/nav.adoc` into two sections, "Architecture" and "Guides".

Architecture holds the arc42 subset, which is chapters 1, 3, 4, 5, 6, 7, 8, 9 and 11.
Chapter 2 is optional and you may skip it.
Use the `architecture-docs` skill for what each chapter holds.
Where no page exists yet, use the not-written-yet admonition page from that skill.
Add every new page to `nav.adoc`.
The Mechanics chapter 5 links the contracts that touch its parts, for example `xref:system::contracts.adoc#IF-05[IF-05]`.
Do not invent content.

Guides holds printing, pump assembly and service, and purchasing.

You decide where each existing page fits.
If a page fits nowhere, tell the owner and do not delete it.

Mechanics chapter 5 must open with its place in chapter 5 of the System docs.
Link the System chapter 5 page with `xref:system::building-blocks.adoc[System building block view]`.

## Step 6: Prefix the ADR titles

Change each ADR title to the form `= CM3D-011: Title`.
The prefix is `CM3D` for every ADR in this repo, and the number stays as it is.
File names stay the same.
Update every reference in the repo, such as `xref:` links, nav entries, other ADRs and READMEs.
Use `grep` to find them all, and build the docs to check that no link is broken.

## Step 7: Diagrams

Search `docs/` for every diagram block.
Look for `[plantuml`, `[mermaid`, `[bpmn` and `[d2` blocks, and for image files that are drawn by hand.
A recent check found no diagram block at all.
So there are two cases.

- **Diagrams exist by now:** convert every one of them as described below.
- **No diagram exists:** add no diagram and no placeholder.
  The rules above are then all that is needed, and every future diagram follows `writing-diagrams`.

Say in your final report which case applied.

### Convert to D2

Convert every PlantUML, Mermaid and BPMN diagram to D2.
Keep the content and the `target` name of each diagram.
Use `shape: sequence_diagram` for sequence diagrams.
If a Mermaid or BPMN diagram is ever kept as it is, the CI of the main repo cannot render it, because its Kroki container is the core image only.
So every diagram must be D2.

Each block follows this form.

```text
.<Title of the diagram>
[d2, target="<same name as before>", format="svg", link=self]
----
include::system:ROOT:partial$d2-style.d2[]

<the diagram>
----
<One-line caption that names the question of the diagram.>
```

These rules come from the shared style.

- The include is the first line inside the delimiters of every block.
  It resolves across components, so no extension is needed.
  Never copy the partial into this repo.
- The shared style sets the TALA layout engine, and TALA is the default.
  For a long flowing graph or a DAG, such as a chain or a sequence of steps, a diagram may choose `elk` (or `dagre`) with exactly the line `vars: { d2-config: { layout-engine: elk } }` right after the include.
  Keep the other engine only when the diagram reads clearly better and stays at most 1400 px wide.
  A sequence diagram never sets an engine, and `writing-diagrams` has the details.
- Give every shape, container and edge its class as `writing-diagrams` lists them.
  Shapes take `actor`, `external`, `boundary`, `block` or a part class such as `mechanical-part`, and a detailed diagram gives every part its part class.
  Edges take one kind class (`power`, `data`, `fluid`, `mechanical`, `interaction`, `fault`) or `normal`.
  At most one edge per diagram adds `key` after its kind, as in `{class: [mechanical; key]}`.
  A shape or an edge without a class renders in default colours.
- The System page Diagram Conventions is the legend of the site.
  A caption may link it with `xref:system::diagram-conventions.adoc[diagram conventions]`, and no diagram draws a legend box.
- Do not write colours, fonts or sizes in a diagram.
  They come from the classes.
- Keep `link=self` on the block, so a click opens the diagram at full size.
- Keep the rendered SVG at most 1400 px wide.
  Break long labels with `\n` or set `direction: down` when it is wider.

Each converted diagram keeps or gains a `.Title` line above the block and a one-line caption below it.
Box labels follow "name · type · technology" as in `writing-diagrams`.
Keep the existing `target` values.
Use `writing-diagrams` for the choice of diagram type, the content of boxes and arrows, and the naming.
Commit the diagrams of each page, or of each small group of pages, as one step.

### Check the conversion

Check each diagram twice.

First compare the label lists.
For every diagram, list all node names, edge labels and notes of the old source and of the new source.
The two lists must match, or each difference must be a deliberate change that you name in the report.
Do not drop a node or an edge.

Then check that the source compiles.
The System partial is not part of this repo.
It lives in `mathisloge/cocktail-maker` at `docs/system/modules/ROOT/partials/d2-style.d2`.
Read it from `main` there, for example from `https://raw.githubusercontent.com/mathisloge/cocktail-maker/main/docs/system/modules/ROOT/partials/d2-style.d2`, or from a local checkout.
Put its text in front of the block body, save the result as `diagram.d2` outside the repo, and send it to `https://kroki.io`.
The docs of all Sir-Mix-A-Lot repos are public, so kroki.io is fine for this repo too.

```sh
curl -sf -X POST -H 'Content-Type: text/plain' --data-binary @diagram.d2 https://kroki.io/d2/svg | head -c 200
```

A local renderer works too, for example when you are offline.

```sh
# a local Kroki, then POST the source to it in the same way
docker run -p 8000:8000 yuzutech/kroki:0.36.0
curl -sf -X POST -H 'Content-Type: text/plain' --data-binary @diagram.d2 http://localhost:8000/d2/svg | head -c 200

# or a local D2 binary
d2 --layout=tala diagram.d2 diagram.svg
```

Use `d2` v0.9.0 or newer, which bundles TALA, or Kroki 0.36.0.
If a local renderer has no TALA, the compile check may use `--layout=elk`, or a temporary copy of the style with `layout-engine: elk`.
The check is only about whether the diagram compiles.
Never change the shared partial for this check.
Exit code 0 and an `<svg` in the output mean that the source compiles.

## What not to do

- Do not change the meaning of any text, and do not invent technical facts.
- Do not rename or delete ADR files.
- Do not use `--no-verify` and do not disable the hook in any other way.
- Do not reword the avoid-word warnings.
  The `glossary-term` warnings are fixed in Step 8.
- Do not copy the shared style partial into this repo, and set a layout engine in a diagram only as `writing-diagrams` describes.
- Do not add a diagram that no page needs.
- Do not use semicolons or dash breaks in your own prose, such as commit messages and nav text.
- Do not edit the main repo `mathisloge/cocktail-maker`.

## Step 8: Glossary terms and the quality goals of the ADRs

The System docs have a glossary in `docs/system/modules/ROOT/partials/glossary.tsv` of the main repo.
The plugin ships a copy, and the checker warns with `glossary-term` when a page uses a word that the glossary avoids.

- Use the System glossary terms exactly, for example Pod, Dispenser, Pump, Valve, Load cell and Operator.
  Replace each word that a `glossary-term` warning names with the term from the warning, without changing the meaning.
  If a word has a correct second meaning there, put `// adoc-style: ignore-next-line` above the line and name the case in your report.
- Mechanics may add its own glossary for mechanical terms that only this repo needs.
  Put it in `docs/modules/ROOT/partials/glossary.tsv`, with the same five tab-separated columns as the System file: `term`, `definition`, `level`, `code` and `avoid`.
  The level of every row is `Mechanics`.
  Keep the trailing tabs of empty cells, because Asciidoctor does not pad a short row.
  If the repo has a trailing-whitespace hook, exclude the glossary files from it with `exclude: glossary.*\.tsv$`.
  Render it on a page `= 12 Glossary` that links the System glossary with `xref:system::glossary.adoc[System glossary]`, and add the page to `nav.adoc`.
  Never redefine a System term, and never use a System term or a word that the System glossary avoids as a Mechanics term.
  Show the owner the rows before you commit them, and commit only the rows that the owner confirmed.
- Every CM3D ADR gets the section `== Quality Goals` directly after `== Context`, as `writing-adrs` describes.
  It names the System quality goals that the decision serves or affects, from `QG-01` to `QG-06`, with one line each on how.
  Link each ID as `xref:system::quality-goals.adoc#QG-01[QG-01]`.
  The consequences get the subsection `=== Effect on the Quality Goals`.
  Draft the references for all ADRs first and show them to the owner.
  Apply only the references that the owner confirmed, and leave the decision text unchanged.

Commit the glossary terms, the Mechanics glossary and the ADR goals as separate steps.

## Step 9: When you finish

Run `pre-commit run --all-files` and the docs build.
Both must pass.
If a page of this repo has a diagram, its include line needs the System component in the build.
The preview playbook of this repo lists only `mechanics`, so the include does not resolve there.
Then add a source for `docs/system` from the `main` branch of `https://github.com/mathisloge/cocktail-maker.git` to the preview playbook, or build with the playbook of the main repo.
Then report each commit, which case of Step 7 applied, the label-list comparison for each converted diagram, the avoid-word warnings, what you could not place, and any question for the owner.
