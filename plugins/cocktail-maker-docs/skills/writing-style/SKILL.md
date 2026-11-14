---
name: writing-style
description: 'Use when writing or editing any prose in the Sir-Mix-A-Lot projects (cocktail-maker, cocktail-maker-3d, cocktail-maker-embedded): AsciiDoc docs, ADRs, guides, READMEs, design specs and explanatory comments. Covers wording, sentence layout and punctuation.'
---

# Writing style

These rules apply to all prose in the Sir-Mix-A-Lot projects.
That means AsciiDoc docs, ADRs, guides, READMEs, design specs and explanatory comments.
In code comments only the wording rules apply, because the language formatter owns the line layout (clang-format has `ReflowComments: true`).
Many readers are not native English speakers.
Write so that they understand each sentence the first time they read it.
The rules take ideas from ASD-STE100 Simplified Technical English.
They are not that standard, so do not apply its full rule set or its word list.

## Plain, common words

Write technical English in plain, common words.
When two words mean the same, use the short and common one.
Technical terms are fine when the reader needs them.

```text
Wrong: The dispatcher leverages the registry to facilitate Pod lookups.
Right: The dispatcher uses the registry to find a Pod.
```

The file `avoid-words.tsv` in this skill folder lists words to replace, each with what to use instead.
Read it before you write.
It is the only copy of the list, so add new words there and not here.

## Short, active sentences

Keep sentences short, with one idea in each.
Use the active voice and name who does the action.

```text
Wrong: The fill level is updated by the Station after the dispense has been confirmed by the Pod.
Right: The Pod confirms the dispense.
       Then the Station updates the fill level.
```

## One term per concept

Use one term for each concept, and keep it in every sentence and on every page.
A new word makes the reader think of a new thing.
If the glossary or the naming conventions page has the term, use that term and its spelling.

```text
Wrong: The Station sends the pour command to each Pod.
       Each module then pours its ingredient.
Right: The Station sends the pour command to each Pod.
       Each Pod then pours its ingredient.
```

## One sentence per line

Start each sentence on a new line.
Never wrap a sentence over two lines.
A diff then shows exactly which sentence changed.
The page looks the same, because AsciiDoc and Markdown join the lines of a paragraph.

```text
Wrong: The Pod reports its weight. The Station
       checks it against the recipe.
Right: The Pod reports its weight.
       The Station checks it against the recipe.
```

## No semicolons

Do not use semicolons in prose.
Make two sentences, or join the parts with a word such as "and", "but", "so" or "because".
Semicolons in code, inline code and URLs are fine.

```text
Wrong: The Pod is busy; the Station waits.
Right: The Pod is busy, so the Station waits.
```

## No dashes as sentence breaks

Do not use a dash to break a sentence.
This covers ` - `, `–`, `—` and the AsciiDoc ` -- `.
Use a comma, a colon, brackets or a new sentence.
Hyphens in compound words such as "Station-Pod" are fine.

```text
Wrong: Each Pod exchange has its own timeout – a slow Pod cannot block the others.
Right: Each Pod exchange has its own timeout, so a slow Pod cannot block the others.
```

## Check a file

The checker is in the `scripts` folder of this plugin, two levels above this skill folder.
It needs Python 3.10 or newer.
Run it on a file:

```sh
python3 <plugin>/scripts/check_adoc_style.py <file>
```

Here `<plugin>` is the plugin folder, for example `plugins/cocktail-maker-docs` in the cocktail-maker repo.
The checker prints each finding with its line number.
Errors are two sentences on one line, a sentence wrapped over lines, a semicolon and a dash break.
Fix every error.
Warnings are words from the avoid list.
Replace them too, unless no plain word says the same thing.

To fix the line layout, add `--fix`:

```sh
python3 <plugin>/scripts/check_adoc_style.py --fix <file>
```

`--fix` only moves line breaks and never changes a word.
You must reword semicolons and dashes by hand.
Read the diff after `--fix`.

The checker skips code blocks, literal blocks, diagram blocks, tables, comments and attribute entries.
It also skips paragraphs whose line breaks show on the page, such as `[source]`, `[verse]` or `[%hardbreaks]` paragraphs.
In AsciiDoc, the line `// adoc-style: ignore-next-line` turns the check off for the next line.
Use it only when no rewording works.
In Markdown, the checker reads a `---` rule in the text as a dash, so use a heading instead.

## Glossary terms

Use each glossary term exactly, and never one of its avoided synonyms.
The glossaries are the `partials/glossary.tsv` files under `docs/` of the repository and the plugin copy `glossary-system.tsv` next to this skill.
The checker warns with the rule `glossary-term` and names the term to use.
When a synonym is correct in its other meaning, for example "module" for a C++ module, put `// adoc-style: ignore-next-line` above the line.
A glossary TSV keeps the tabs of empty cells, because Asciidoctor does not pad a short row.
A repo with a trailing-whitespace hook excludes the glossary files from it with `exclude: glossary.*\.tsv$`.

## Before you finish

- Each line holds one sentence.
- No sentence has a semicolon or a dash break.
- No word from the avoid list is left without a reason.
- Each concept has one term on every page.
- The sentences are short and active.
