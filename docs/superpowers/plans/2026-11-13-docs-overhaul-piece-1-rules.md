# Docs Overhaul Piece 1: Rules Plugin and Style Hook Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the repo's dev tooling to Python (uv, pytest, ruff, commitizen). Then ship the `cocktail-maker-docs` Claude Code plugin with three skills and an AsciiDoc style checker that runs as a pre-commit hook here and in other repos. Make the existing System and Station docs pass the checker, and write a handoff prompt for the 3D repo.

**Architecture:**
- **Python tooling:** a non-package `pyproject.toml` at the repo root holds the dev tools and their config, as in the 3D repo. Commitizen owns the version, and CMake reads it from `pyproject.toml`.
- **Shared content:** everything shared lives in `plugins/cocktail-maker-docs/`: the skills, the avoid-word list, the checker script and its tests.
- **Publishing:** the repo root holds the marketplace manifest and `.pre-commit-hooks.yaml`, so other repos can enable the plugin and the hook from `mathisloge/cocktail-maker`.

**Tech Stack:** Python 3.12+, uv, pytest, ruff, commitizen, pre-commit, Claude Code plugin manifests, AsciiDoc, CMake (version read only).

**Spec:** `docs/superpowers/specs/2026-11-13-docs-overhaul-design.md` (section "Piece 1: rules as a plugin, plus the hook"). Task 0 is an addition the owner asked for after the spec was approved: Python-only dev tooling with commitizen in place of semantic-release.

## Global Constraints

- **Prose rules:**
  - technical English in plain common words
  - short active sentences
  - one term per concept
  - one sentence per line
  - no semicolons
  - no dashes as sentence breaks (` - `, `–`, `—`, and AsciiDoc's ` -- `). Hyphens in compound words such as "Station-Pod" are allowed.
- **Severity:** avoid-list words only warn. Every other rule fails the hook.
- **Skipped by the hook:** listing and literal blocks, diagram blocks, tables, attribute entries, comments, and lines that only hold an xref, include or other block macro.
- **Names:** marketplace `cocktail-maker`, plugin `cocktail-maker-docs`, skills `writing-style`, `architecture-docs` and `writing-adrs`, hook id `adoc-style`.
- **Mechanical pass scope:** every `.adoc` file under `docs/system/` and `docs/station/`, and nothing else. Moved or reflowed text keeps its meaning.
- **Out of scope here:** ADR renames and ID prefixes belong to piece 3. The `writing-adrs` skill only describes the prefix rule.
- **Checker dependencies:** the checker script imports only the Python standard library. That way `language: script` works in any repo with `python3`. pytest and ruff are dev tools only.
- **Python tooling layout:** it mirrors the 3D repo (`mathisloge/cocktail-maker-3d`): uv, a `dev` dependency group, `[tool.commitizen]` with `version_provider = "pep621"` and `tag_format = "v$version"`, and the commitizen commit-msg hook.
- **Node:** it stays only for Antora (`antora`, `asciidoctor-kroki` in `package.json`). No release or commit tooling stays on Node.
- **Prose in this plan's files:** all new prose (skills, handoff prompt) follows the prose rules itself.

## Review Focus

1. Abbreviations, decimals and version numbers ("e.g.", "i.e.", "Fig. 3", "1.2", "v2.0") must not count as sentence ends. Test in Task 1.
2. Semicolons and dashes inside inline code, URLs and HTML entities (`` `using X = Y;` ``, `&amp;`, `https://a.b/c;d`) must not be flagged. Test in Task 1.
3. Every delimited block type (`----`, `....`, `++++`, `////`, `` ``` ``, `|===`, `,===`, `!===`), including ones preceded by block attributes like `[d2, target=x]`, must be skipped as a whole. Test in Task 1.
4. `--fix` must only move line breaks. It never changes words, list markers, headings, block attributes or hard line breaks (` +`), and running it twice gives the same result. Test in Task 3.
5. A pre-release or suffixed version in `pyproject.toml` (`1.2.0a1`, `1.2.0-rc.1`) must still configure CMake with `1.2.0`. Test in Task 0.

---

## File Structure

| Path | Responsibility |
|---|---|
| `pyproject.toml` | Dev tool dependencies and config (pytest, ruff, commitizen). The project version. |
| `uv.lock` | Locked dev tools. |
| `package.json`, `package-lock.json` | Antora only. |
| `CMakeLists.txt` | Reads the version from `pyproject.toml`. |
| `cmake/read_project_version.cmake` | `cm_read_project_version(<pyproject> <out_var>)`. |
| `tests/cmake/read_project_version_test.cmake` | `cmake -P` test for the function. |
| `.devcontainer/Dockerfile`, `.devcontainer/devcontainer.json` | uv in place of the pre-commit venv. |
| `.github/workflows/build-and-test.yml` | Runs pytest. |
| `CLAUDE.md` | Lint and test commands use uv. |
| `.claude-plugin/marketplace.json` | Makes this repo the marketplace `cocktail-maker`. |
| `.pre-commit-hooks.yaml` | Publishes the `adoc-style` hook for other repos. |
| `.pre-commit-config.yaml` | Commitizen, ruff, check-toml, and the local `adoc-style` hook. |
| `.claude/settings.json` | Enables the plugin for this repo (committed, shared). |
| `.gitignore` | Gains `.superpowers/` (brainstorm mockups). |
| `plugins/cocktail-maker-docs/.claude-plugin/plugin.json` | Plugin manifest. |
| `plugins/cocktail-maker-docs/skills/writing-style/SKILL.md` | Prose rules. |
| `plugins/cocktail-maker-docs/skills/writing-style/avoid-words.tsv` | Avoid list, read by the skill and the checker. |
| `plugins/cocktail-maker-docs/skills/architecture-docs/SKILL.md` | arc42 structure rules. |
| `plugins/cocktail-maker-docs/skills/writing-adrs/SKILL.md` | ADR rules. |
| `plugins/cocktail-maker-docs/skills/writing-adrs/adr-template.adoc` | ADR skeleton. |
| `plugins/cocktail-maker-docs/scripts/check_adoc_style.py` | The checker and `--fix` reflow (executable). |
| `plugins/cocktail-maker-docs/tests/test_check_adoc_style.py` | Checker tests. |
| `plugins/cocktail-maker-docs/tests/test_plugin_layout.py` | Manifest and skill layout tests. |
| `docs/superpowers/handoffs/2026-11-13-mechanics-docs-prompt.md` | Prompt for the agent in the 3D repo. |

From Task 1 on, all tests run with `uv run pytest -v`.
pytest finds them through `testpaths`, and `pythonpath` makes `import check_adoc_style` work.

---

### Task 0: Python dev tooling with commitizen

**Files:**
- Create: `pyproject.toml`, `uv.lock`, `cmake/read_project_version.cmake`, `tests/cmake/read_project_version_test.cmake`
- Modify: `CMakeLists.txt:6-13`, `package.json`, `package-lock.json`, `.pre-commit-config.yaml`, `.devcontainer/Dockerfile:54-64`, `.devcontainer/devcontainer.json` (`postCreateCommand`), `.github/workflows/build-and-test.yml`, `CLAUDE.md` (Lint / format section)

**Interfaces:**
- Produces:
  - `uv run pytest`, `uv run pre-commit`, `uv run cz` for every later task.
  - CMake function `cm_read_project_version(<pyproject_path> <out_var>)`. It sets `<out_var>` to the leading `X.Y.Z` of the first `version = "..."` line in the `[project]` table. It fails with `FATAL_ERROR` when there is none.

**`pyproject.toml` (exact):**

```toml
[project]
name = "cocktail-maker"
version = "1.0.0"
description = "Dev tooling of the Sir-Mix-A-Lot Station: docs plugin checks, tests and release versioning"
requires-python = ">=3.12"
dependencies = []

[dependency-groups]
dev = ["pytest", "ruff", "pre-commit", "commitizen"]

[tool.uv]
package = false

[tool.pytest.ini_options]
testpaths = ["plugins/cocktail-maker-docs/tests"]
addopts = "--import-mode=importlib"
pythonpath = ["plugins/cocktail-maker-docs/scripts"]

[tool.ruff]
line-length = 120

[tool.ruff.lint]
select = ["E", "F", "I", "UP", "B"]

[tool.commitizen]
name = "cz_conventional_commits"
version_provider = "pep621"
tag_format = "v$version"
```

**`package.json` after the change:**
- It keeps `name`, `private` and the `antora` and `asciidoctor-kroki` devDependencies.
- It drops `version`, the `release` block, and the `semantic-release`, `@semantic-release/*` and `conventional-changelog-conventionalcommits` devDependencies.
- `package-lock.json` is regenerated with `npm install`. There is no Node in the devcontainer, so use a portable Node on `PATH` in the session scratchpad.

**`.pre-commit-config.yaml` changes:**
- **Commitizen:** replace the `compilerla/conventional-pre-commit` repo with `commitizen-tools/commitizen`, `rev: v4.19.3`, hook `commitizen`, `stages: [commit-msg]`.
- **check-toml:** add `check-toml` to the `pre-commit-hooks` list.
- **Ruff:** add `astral-sh/ruff-pre-commit` `rev: v0.17.4` with `ruff-check` (`args: [--fix]`) and `ruff-format`, both limited to `files: ^plugins/`. Existing Python under `tools/` is not reformatted.

**Devcontainer:**
- **Dockerfile:** install uv from the official image at a pinned version (`COPY --from=ghcr.io/astral-sh/uv:<version> /uv /uvx /bin/`), using the release current at implementation time. Remove the `/opt/pre-commit` venv and its symlink. Keep `python3` and `python3-venv`, because pre-commit's python hooks need them.
- **`postCreateCommand`:** replace `pre-commit install` with `uv sync && uv run pre-commit install`. The `safe.directory` and `usermod` parts stay.

**CI:** add a job `python-tooling` to `build-and-test.yml` that runs `astral-sh/setup-uv` (pinned major) and `uv run pytest -v`.

**CLAUDE.md:** the Lint / format section says `uv run pre-commit run --all-files`. Add one line on Python tests (`uv run pytest`) and one on releases (`uv run cz bump`).

- [ ] **Step 1: Write the failing CMake test**

In `tests/cmake/read_project_version_test.cmake`:
- `include()` the function file.
- Write temp `pyproject.toml` files into `${CMAKE_CURRENT_BINARY_DIR}` and assert with `if(NOT ...) message(FATAL_ERROR ...)`:
  - `version = "1.0.0"` gives `1.0.0`.
  - `version = "1.2.0a1"` gives `1.2.0`.
  - `version = "1.2.0-rc.1"` gives `1.2.0`.
  - A `[tool.x]` table with its own `version = "9.9.9"` after `[project]` still gives the `[project]` value.

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake -P tests/cmake/read_project_version_test.cmake`
Expected: an error that `cmake/read_project_version.cmake` cannot be included

- [ ] **Step 3: Implement `cm_read_project_version` in `cmake/read_project_version.cmake`**

The function reads the file with `file(STRINGS)` and tracks whether it is inside `[project]` (from `[project]` until the next `[` header). It takes the first `version = "..."` there and extracts `^[0-9]+\.[0-9]+\.[0-9]+`. It also adds `<pyproject_path>` to `CMAKE_CONFIGURE_DEPENDS`.

- [ ] **Step 4: Run the CMake test to verify it passes**

Run: `cmake -P tests/cmake/read_project_version_test.cmake`
Expected: exit 0, no output

- [ ] **Step 5: Switch `CMakeLists.txt` to the function**

Replace lines 6-13 with `include(cmake/read_project_version.cmake)` and `cm_read_project_version("${CMAKE_SOURCE_DIR}/pyproject.toml" BASE_VERSION)`.

- [ ] **Step 6: Create `pyproject.toml`, slim `package.json`, update pre-commit, devcontainer, CI and CLAUDE.md**

- [ ] **Step 7: Lock and check the tooling**

Run: `uv lock && uv sync && uv run cz version --project && uv run pre-commit run --all-files`
Expected:
- `cz version --project` prints `1.0.0`.
- Every hook passes or is skipped.
- `ruff` finds no Python under `plugins/` yet.

If `uv` is not available in this session, stop here. Ask the owner to rebuild the devcontainer, then continue with this step.

- [ ] **Step 8: Verify that CMake still configures with the same version**

Run: `cmake --preset developer 2>&1 | grep 'Project version'`
Expected: `-- Project version: 1.0.0`

- [ ] **Step 9: Verify that the Antora deps still install**

Run (portable Node on `PATH`): `npm install && npx antora --version`
Expected: Antora prints its version, and `git diff package-lock.json` drops only semantic-release packages.

- [ ] **Step 10: Commit**

```bash
git add pyproject.toml uv.lock package.json package-lock.json CMakeLists.txt cmake/read_project_version.cmake tests/cmake .pre-commit-config.yaml .devcontainer .github/workflows/build-and-test.yml CLAUDE.md
git commit -m "build: move dev tooling to uv and replace semantic-release with commitizen"
```

The commit message passes the new commitizen hook.

- [ ] **Step 11: Checkpoint**

Tell the owner to rebuild the devcontainer before Task 1, unless Step 7 already ran with uv in this session.

---

### Task 1: Checker line rules

**Files:**
- Create: `plugins/cocktail-maker-docs/scripts/check_adoc_style.py`
- Test: `plugins/cocktail-maker-docs/tests/test_check_adoc_style.py`

**Interfaces:**
- Consumes: `uv run pytest` from Task 0
- Produces:
  - **`class LineKind(enum.Enum)`:** `BLANK`, `SKIP`, `HEADING`, `LIST_ITEM`, `TEXT`
  - **`classify_lines(lines: list[str]) -> list[LineKind]`:** one kind per input line.
    - `SKIP`: lines inside skipped blocks and the delimiters themselves, block attribute lines (`[...]`), attribute entries (`:name: value`), comments (`//`), block macros (`name::target[...]`), conditionals (`ifdef::`, `endif::`, `ifeval::`), block titles (`.Title`), and table rows (`|...`).
    - `HEADING`: `=` and `#` headings.
    - `LIST_ITEM`: lines starting with `*`, `-`, `.` (ordered), `<n>.` or a description term (`term::`), followed by a space.
  - **`class Finding(NamedTuple)`:** `line: int` (1-based), `rule: str`, `severity: str` (`"error"` or `"warning"`), `message: str`
  - **`check_text(text: str, avoid_words: Mapping[str, str] | None = None) -> list[Finding]`:**
    - Rules: `semicolon` and `dash` (on TEXT, LIST_ITEM and HEADING), `sentence-per-line` and `sentence-wrapped` (on TEXT and LIST_ITEM).
    - Findings are sorted by line.
    - `avoid_words` is used in Task 2. `None` means no avoid-word check.
  - **`split_sentences(line: str) -> list[str]`:**
    - It splits after `.`, `!` or `?` followed by whitespace and an uppercase letter, digit, `` ` ``, `*`, `_`, `"` or `(`.
    - It never splits after a known abbreviation (`e.g.`, `i.e.`, `etc.`, `vs.`, `cf.`, `approx.`, `Fig.`, `No.`) or a single capital initial.
    - Inline code spans, macro targets (`xref:...[`, `link:...[`, `https?://...`) and attribute references (`{name}`) are masked before splitting.

**Rule details:**
- **`sentence-wrapped`:** a TEXT or LIST_ITEM line whose next line is TEXT (a continuation) and whose masked end, after stripping trailing `*_`'")]`, is not `.`, `!`, `?` or `:`, and does not end in ` +`.
- **Opt-out:** a comment line `// adoc-style: ignore-next-line` suppresses all findings for the following line.

- [ ] **Step 1: Write the failing tests**

```python
import pytest
from check_adoc_style import LineKind, check_text, classify_lines

def rules(text):
    return [(f.line, f.rule) for f in check_text(text)]

def test_semicolon_flagged():
    assert rules("One; two.\n") == [(1, "semicolon")]

def test_semicolon_in_code_entity_url_ignored():
    assert rules("Use `using X = Y;` here.\nA &amp; B.\nSee https://a.b/c;d now.\n") == []

@pytest.mark.parametrize("line", ["A - b.", "A – b.", "A — b.", "A -- b.", "A—b."])
def test_dash_variants_flagged(line):
    assert rules(line + "\n") == [(1, "dash")]

def test_hyphen_and_list_marker_ok():
    assert rules("The Station-Pod link.\n\n- Item one.\n") == []

def test_heading_punctuation_checked():
    assert rules("== A; B\n") == [(1, "semicolon")]

def test_two_sentences_flagged():
    assert rules("One. Two.\n") == [(1, "sentence-per-line")]

def test_abbreviations_and_numbers_ok():
    assert rules("A pump, e.g. a peristaltic one.\nVersion 1.2 is out.\nSee Fig. 3 here.\nUse v2.0 now.\n") == []

def test_attribute_reference_end_is_real():
    assert rules("It uses {cpp}20. It works.\n") == [(1, "sentence-per-line")]

def test_macro_targets_ok():
    assert rules("See xref:adr:002-x.adoc[ADR-002] for details.\n") == []

def test_wrapped_sentence_flagged():
    assert rules("This sentence goes\non here.\n") == [(1, "sentence-wrapped")]

def test_colon_and_hard_break_ok():
    assert rules("Steps:\n* One.\n\nLine one +\nline two.\n") == []

def test_list_items_without_period_ok():
    assert rules("* xref:a.adoc[]\n* xref:b.adoc[]\n") == []

@pytest.mark.parametrize("open_,close", [("----", "----"), ("....", "...."), ("++++", "++++"), ("////", "////"),
                                         ("```", "```"), ("|===", "|==="), (",===", ",==="), ("!===", "!===")])
def test_skipped_blocks(open_, close):
    assert rules(f"[d2, target=x]\n{open_}\na; b - c. D\n{close}\n") == []

def test_skipped_lines():
    assert rules(':attr: a; b\n// note; x - y\ninclude::partial$x.adoc[]\n[cols="1;2"]\n.Title; here\n') == []

def test_opt_out():
    assert rules("// adoc-style: ignore-next-line\nOne; two.\n") == []

def test_classify_lines():
    assert classify_lines(["= T", "", "Text.", "* Item.", "----", "x", "----"]) == [
        LineKind.HEADING, LineKind.BLANK, LineKind.TEXT, LineKind.LIST_ITEM,
        LineKind.SKIP, LineKind.SKIP, LineKind.SKIP]
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `uv run pytest -v`
Expected: collection error `ModuleNotFoundError: No module named 'check_adoc_style'`

- [ ] **Step 3: Implement `LineKind`, `classify_lines`, `Finding`, `split_sentences` and `check_text` in `scripts/check_adoc_style.py`**

The file starts with `#!/usr/bin/env python3`, gets `chmod +x`, and imports only the standard library.
Delimited blocks close on the same delimiter string that opened them.
Masking replaces spans with same-length placeholder characters, so positions stay valid.

- [ ] **Step 4: Run tests to verify they pass**

Run: `uv run pytest -v && uv run ruff check plugins && uv run ruff format --check plugins`
Expected: all tests PASS, and ruff reports nothing

- [ ] **Step 5: Commit**

```bash
git add plugins/cocktail-maker-docs/scripts/check_adoc_style.py plugins/cocktail-maker-docs/tests/test_check_adoc_style.py
git commit -m "feat(docs): add AsciiDoc style checker line rules"
```

---

### Task 2: Avoid-word warnings and command line

**Files:**
- Create: `plugins/cocktail-maker-docs/skills/writing-style/avoid-words.tsv`
- Modify: `plugins/cocktail-maker-docs/scripts/check_adoc_style.py`
- Test: `plugins/cocktail-maker-docs/tests/test_check_adoc_style.py`

**Interfaces:**
- Consumes: `check_text`, `Finding` from Task 1
- Produces:
  - **`DEFAULT_AVOID_WORDS: Path`:** `../skills/writing-style/avoid-words.tsv` relative to the script.
  - **`load_avoid_words(path: Path = DEFAULT_AVOID_WORDS) -> dict[str, str]`:** reads a UTF-8 file with one `word<TAB>replacement` per line. Lines starting with `#` and blank lines are ignored, and keys are lowercased.
  - **`check_text(..., avoid_words=...)`:** adds `avoid-word` findings with severity `"warning"`.
    - A key matches case-insensitively at a word start, followed by any word characters, so "utilize" also catches "utilized".
    - Multi-word keys match across single spaces.
    - Masked spans (code, macros) are not checked.
    - The message names the replacement.
  - **`main(argv: Sequence[str] | None = None) -> int`:**
    - It takes positional `files` and the flag `--fix` (implemented in Task 3, a no-op until then).
    - It prints one line per finding as `<path>:<line>: <severity>: <rule>: <message>`.
    - It returns 1 if any finding is an error, otherwise 0.

**`avoid-words.tsv` content (exact keys, from the spec):**

| word | replacement |
|---|---|
| utilize | use |
| leverage | use |
| facilitate | help or allow |
| robust | name the concrete property |
| in order to | to |
| seamless | drop it or say it plainly |
| comprehensive | drop it or say it plainly |
| crucial | drop it or say it plainly |
| delve | drop it or say it plainly |
| furthermore | drop it or say it plainly |
| moreover | drop it or say it plainly |

- [ ] **Step 1: Write the failing tests**

```python
from check_adoc_style import load_avoid_words, main

def test_load_avoid_words():
    words = load_avoid_words()
    assert words["utilize"] == "use"
    assert words["in order to"] == "to"

def test_avoid_word_is_warning():
    found = check_text("We Utilized pumps in order to pour.\n", load_avoid_words())
    assert [(f.line, f.rule, f.severity) for f in found] == [(1, "avoid-word", "warning")] * 2

def test_avoid_word_in_code_ignored():
    assert check_text("Call `utilize()` here.\n", load_avoid_words()) == []

def test_main_exit_codes_and_format(tmp_path, capsys):
    bad = tmp_path / "bad.adoc"; bad.write_text("One; two.\n")
    warn = tmp_path / "warn.adoc"; warn.write_text("We utilize it.\n")
    assert main([str(bad)]) == 1
    assert f"{bad}:1: error: semicolon:" in capsys.readouterr().out
    assert main([str(warn)]) == 0
    assert f"{warn}:1: warning: avoid-word:" in capsys.readouterr().out
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `uv run pytest -v`
Expected: `ImportError: cannot import name 'load_avoid_words'`

- [ ] **Step 3: Create `avoid-words.tsv`, then implement `load_avoid_words`, the avoid-word rule and `main`**

`main` loads the default avoid words and reads each file as UTF-8.
It is called from `if __name__ == "__main__": sys.exit(main())`.

- [ ] **Step 4: Run tests to verify they pass**

Run: `uv run pytest -v && uv run ruff check plugins && uv run ruff format --check plugins`
Expected: all tests PASS, and ruff reports nothing

- [ ] **Step 5: Commit**

```bash
git add plugins/cocktail-maker-docs
git commit -m "feat(docs): warn on avoid-list words and add the checker CLI"
```

---

### Task 3: `--fix` reflow

**Files:**
- Modify: `plugins/cocktail-maker-docs/scripts/check_adoc_style.py`
- Test: `plugins/cocktail-maker-docs/tests/test_check_adoc_style.py`

**Interfaces:**
- Consumes: `classify_lines`, `LineKind` and `split_sentences` from Task 1, and `main` from Task 2
- Produces: `reflow(text: str) -> str`.
  - Each run of one TEXT or LIST_ITEM line plus its following TEXT continuation lines is joined with single spaces.
  - Continuation indentation is dropped, and a line ending in ` +` ends the run.
  - The run is then split with `split_sentences`, and the list marker stays on the first line.
  - All other lines are returned unchanged.
  - With `--fix`, `main` writes `reflow(text)` back when it differs, then reports the findings of the new text.

- [ ] **Step 1: Write the failing tests**

```python
from pathlib import Path
from check_adoc_style import reflow

REPO = Path(__file__).resolve().parents[3]

def test_paragraph():
    assert reflow("One two\nthree. Four\nfive.\n") == "One two three.\nFour five.\n"

def test_list_item():
    assert reflow("* Item one\n  wraps. Second.\n* Next.\n") == "* Item one wraps.\nSecond.\n* Next.\n"

def test_abbreviation_kept():
    assert reflow("A pump, e.g. a\nperistaltic one.\n") == "A pump, e.g. a peristaltic one.\n"

def test_untouched_lines():
    text = "= Title\n\n[source,cpp]\n----\nint a; int b;\n----\n\nLine one +\nline two.\n"
    assert reflow(text) == text

def test_idempotent_and_word_preserving():
    text = (REPO / "docs/station/modules/ROOT/pages/building-blocks.adoc").read_text()
    out = reflow(text)
    assert reflow(out) == out
    assert out.split() == text.split()

def test_main_fix_writes_file(tmp_path):
    p = tmp_path / "a.adoc"; p.write_text("This goes\non.\n")
    assert main(["--fix", str(p)]) == 0
    assert p.read_text() == "This goes on.\n"
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `uv run pytest -v`
Expected: `ImportError: cannot import name 'reflow'`

- [ ] **Step 3: Implement `reflow` and wire `--fix` into `main`**

- [ ] **Step 4: Run tests to verify they pass**

Run: `uv run pytest -v && uv run ruff check plugins && uv run ruff format --check plugins`
Expected: all tests PASS, and ruff reports nothing

- [ ] **Step 5: Commit**

```bash
git add plugins/cocktail-maker-docs
git commit -m "feat(docs): add --fix to reflow AsciiDoc to one sentence per line"
```

---

### Task 4: Plugin, marketplace and the `writing-style` skill

**Files:**
- Create: `.claude-plugin/marketplace.json`
- Create: `plugins/cocktail-maker-docs/.claude-plugin/plugin.json`
- Create: `plugins/cocktail-maker-docs/skills/writing-style/SKILL.md`
- Create: `.claude/settings.json`
- Modify: `.gitignore` (add `.superpowers/`)
- Test: `plugins/cocktail-maker-docs/tests/test_plugin_layout.py`

**Interfaces:**
- Consumes: `check_text` and `load_avoid_words` from Task 2, and `avoid-words.tsv`
- Produces: the plugin layout that Task 5 adds skills to. `test_plugin_layout.py` defines the module-level helpers `REPO: Path`, `SKILLS: Path`, `skill_dirs() -> list[Path]` (every directory under `skills/` that holds a `SKILL.md`) and `parse_frontmatter(path: Path) -> dict[str, str]` (the `key: value` lines between the first two `---` lines).

**Manifest values:**
- **`marketplace.json`:**
  - `"name": "cocktail-maker"`
  - `"owner": {"name": "Mathis Logemann"}`
  - one plugin with `"name": "cocktail-maker-docs"`, `"source": "./plugins/cocktail-maker-docs"`, `"description": "Writing, architecture and ADR guidelines for the Sir-Mix-A-Lot projects."` and `"version": "0.1.0"`
- **`plugin.json`:** the same `name`, `description` and `version`, plus `"repository": "https://github.com/mathisloge/cocktail-maker"`.
- **`.claude/settings.json`:** `extraKnownMarketplaces.cocktail-maker.source = {"source": "github", "repo": "mathisloge/cocktail-maker"}` and `enabledPlugins["cocktail-maker-docs@cocktail-maker"] = true`. `.claude/settings.local.json` stays as it is.

**`writing-style/SKILL.md` requirements:**
- **Frontmatter:**
  - `name: writing-style`
  - `description: Use when writing or editing any prose in the Sir-Mix-A-Lot projects (cocktail-maker, cocktail-maker-3d, cocktail-maker-embedded): AsciiDoc docs, ADRs, guides, READMEs, design specs and explanatory comments. Covers wording, sentence layout and punctuation.`
- **Body:** every prose rule from Global Constraints, each with one short wrong/right example.
- **Readers:** non-native English speakers. ASD-STE100 is named as inspiration, not as a strict standard.
- **Avoid list:** the skill points to `avoid-words.tsv` in the same folder and does not copy it.
- **Checking:** it says how to check a file, with `python3 <plugin>/scripts/check_adoc_style.py <file>` and `--fix` for line layout. It also says that semicolons and dashes need rewording by hand.

- [ ] **Step 1: Write the failing tests**

```python
import json
from pathlib import Path
from check_adoc_style import check_text

REPO = Path(__file__).resolve().parents[3]
SKILLS = REPO / "plugins/cocktail-maker-docs/skills"

def test_marketplace_points_at_plugin():
    market = json.loads((REPO / ".claude-plugin/marketplace.json").read_text())
    assert market["name"] == "cocktail-maker"
    entry = market["plugins"][0]
    assert entry["name"] == "cocktail-maker-docs"
    plugin = json.loads((REPO / entry["source"] / ".claude-plugin/plugin.json").read_text())
    assert plugin["name"] == entry["name"]

def test_skill_frontmatter():
    for d in skill_dirs():
        meta = parse_frontmatter(d / "SKILL.md")
        assert meta["name"] == d.name
        assert 0 < len(meta["description"]) <= 1024

def test_skills_follow_the_style():
    for d in skill_dirs():
        body = (d / "SKILL.md").read_text().split("---", 2)[2]
        assert [f for f in check_text(body) if f.severity == "error"] == [], d.name

def test_settings_enable_plugin():
    settings = json.loads((REPO / ".claude/settings.json").read_text())
    assert settings["enabledPlugins"]["cocktail-maker-docs@cocktail-maker"] is True
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `uv run pytest -v`
Expected: `test_marketplace_points_at_plugin` fails with `FileNotFoundError`

- [ ] **Step 3: Create the manifests, `settings.json`, the `.gitignore` line and `writing-style/SKILL.md`**

- [ ] **Step 4: Run tests to verify they pass**

Run: `uv run pytest -v && uv run ruff check plugins && uv run ruff format --check plugins`
Expected: all tests PASS, and ruff reports nothing

- [ ] **Step 5: Commit**

```bash
git add .claude-plugin .claude/settings.json .gitignore plugins/cocktail-maker-docs
git commit -m "feat(docs): add the cocktail-maker-docs plugin with the writing-style skill"
```

---

### Task 5: `architecture-docs` and `writing-adrs` skills

**Files:**
- Create: `plugins/cocktail-maker-docs/skills/architecture-docs/SKILL.md`
- Create: `plugins/cocktail-maker-docs/skills/writing-adrs/SKILL.md`
- Create: `plugins/cocktail-maker-docs/skills/writing-adrs/adr-template.adoc`
- Modify: `plugins/cocktail-maker-docs/tests/test_plugin_layout.py`

**Interfaces:**
- Consumes: `skill_dirs()`, `SKILLS`, `check_text` and the layout tests from Task 4

**`architecture-docs/SKILL.md` requirements:**
- **Description:** `Use when creating, restructuring or extending architecture documentation (arc42 chapters, building block views, runtime and deployment views, cross-cutting concepts, diagrams) in any Sir-Mix-A-Lot Antora component: system, station, embedded or mechanics.`
- **Builds on:** it says it builds on `writing-style`.
- **Component map:** `system` is the parent and holds the whole-machine architecture and the cross-part decisions. `station`, `embedded` and `mechanics` are subprojects.
- **Chapter table:**

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
  | 12 Glossary | yes | no, System only |

- **Numbering:** chapter numbers are identical in every component and appear in titles and nav. File names stay stable and carry no numbers.
- **Zoom rule:** every block in System chapter 5 links to its subproject's chapter 5 with a cross-component xref such as `xref:station::building-blocks.adoc[]`. Each subproject's chapter 5 opens with one or two sentences on where it sits in System chapter 5, with a link back.
- **Guides:** how-to content (printing, assembly, service) goes in a separate "Guides" nav section, never in arc42 chapters.
- **Unwritten chapter:** a page with this exact admonition, followed by one or two lines on what belongs there:

  ```
  [NOTE]
  ====
  Not written yet.
  ====
  ```

- **Diagrams:** all diagrams are D2, rendered through Kroki. A short "Diagram style" section states that the shared style is defined in System's D2 style partial, which piece 4 adds.
- **What lives only in System:** quality goals, the glossary and naming conventions. Subprojects link to them and never repeat them.

**`writing-adrs/SKILL.md` requirements:**
- **Description:** `Use when a design decision is made or changed in any Sir-Mix-A-Lot project, also mid-coding, to decide whether it needs an ADR, where it goes, and to write or supersede it.`
- **Builds on:** it says it builds on `writing-style`.
- **When a decision earns an ADR:** it is hard to reverse, it affects more than one module or part, or it chooses between real alternatives that someone will question later.
- **Where an ADR goes:** a decision that affects more than one part goes in `system`. Anything else goes in its own part's `adr` module.
- **IDs:**
  - The prefixes are `SYS`, `STA`, `EMB` and `MEC`.
  - The title format is `= SYS-007: <decision as a short phrase>`, with the number taken from the file name (`007-...adoc`).
  - File names never change, and numbers are never reused.
- **Statuses:** Proposed, Accepted, Deprecated and Superseded. To supersede an ADR, set `:adr-superseded-by:` on the old one, and name the old one in the new ADR's Context.
- **Writing an ADR:** copy `adr-template.adoc`, fill it in, and add the page to the component's `nav.adoc` under the ADR list.
- **`adr-template.adoc`:**
  - It's based on `docs/system/modules/adr/pages/999-example.adoc`.
  - Title `= SYS-NNN: <Decision>`, with the same attribute block and comments, and `include::partial$adr-header.adoc[]`.
  - Sections `== Context`, `=== Decision Drivers`, `== Decision`, `== Alternatives Considered` and `== Consequences` (with `* **Good:**` and `* **Bad:**` bullets).
  - Placeholder text only, one sentence per line.

- [ ] **Step 1: Write the failing tests**

Add to `test_plugin_layout.py`:

```python
def test_expected_skills_present():
    assert {d.name for d in skill_dirs()} == {"writing-style", "architecture-docs", "writing-adrs"}

def test_adr_template_passes_style():
    text = (SKILLS / "writing-adrs/adr-template.adoc").read_text()
    assert [f for f in check_text(text) if f.severity == "error"] == []
    assert "include::partial$adr-header.adoc[]" in text
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `uv run pytest -v`
Expected: `test_expected_skills_present` FAILS, because only `writing-style` is found

- [ ] **Step 3: Write both skills and the template**

- [ ] **Step 4: Run tests to verify they pass**

Run: `uv run pytest -v`
Expected: all tests PASS. That includes `test_skills_follow_the_style` for the new skills.

- [ ] **Step 5: Commit**

```bash
git add plugins/cocktail-maker-docs
git commit -m "feat(docs): add architecture-docs and writing-adrs skills"
```

---

### Task 6: Reflow the existing docs

**Files:**
- Modify: every `.adoc` file under `docs/system/` and `docs/station/` that `--fix` changes

**Interfaces:**
- Consumes: `check_adoc_style.py --fix` from Task 3

- [ ] **Step 1: Record the starting findings**

Run: `python3 plugins/cocktail-maker-docs/scripts/check_adoc_style.py $(git ls-files 'docs/system/*.adoc' 'docs/station/*.adoc') | grep -c ': error: '`
Expected: a non-zero count. Note it in the commit message body.

- [ ] **Step 2: Apply the reflow**

Run: `python3 plugins/cocktail-maker-docs/scripts/check_adoc_style.py --fix $(git ls-files 'docs/system/*.adoc' 'docs/station/*.adoc')`

- [ ] **Step 3: Verify that only line breaks changed**

Run: `git diff --word-diff=porcelain -- docs/system docs/station | grep -E '^[-+][^-+]' | wc -l`
Expected: `0`

- [ ] **Step 4: Verify that no line-layout errors remain**

Run: the Step 1 command, without `-c` and filtered with `grep -E 'sentence-(per-line|wrapped)'`
Expected: no output.

If a line was split wrongly, add the case as a failing test in Task 3's tests and fix `split_sentences` first. Never hand-edit around a checker bug.

- [ ] **Step 5: Commit**

```bash
git add docs/system docs/station
git commit -m "docs: put one sentence per line in the System and Station docs"
```

---

### Task 7: Reword semicolons and dashes

**Files:**
- Modify: the `.adoc` files under `docs/system/` and `docs/station/` that still report `semicolon` or `dash`. Known ones:
  - Station ADRs `002`, `003` and `004`
  - System ADRs `001`, `002`, `003` and `006`
  - `station/modules/ROOT/pages/building-blocks.adoc`
  - `system/modules/ROOT/pages/system-design.adoc`

**Interfaces:**
- Consumes: the checker CLI

**Rewording rules:**
- **Semicolon between two clauses:** split into two sentences on two lines, or join with "and", "but" or "so".
- **Dash used as a parenthesis:** use commas, parentheses or a separate sentence.
- **Definition bullets like `* *Catalogs* — the recipe ...`:** use `* *Catalogs:* the recipe ...`.
- The meaning, every technical fact and every xref stay unchanged.
- Leave avoid-list warnings alone in this task. They belong to the later rewrite pass.

- [ ] **Step 1: List the remaining errors**

Run: `python3 plugins/cocktail-maker-docs/scripts/check_adoc_style.py $(git ls-files 'docs/system/*.adoc' 'docs/station/*.adoc') | grep ': error: '`
Expected: only `semicolon` and `dash` findings

- [ ] **Step 2: Reword each finding by the rules above**

- [ ] **Step 3: Verify that no errors remain**

Run: the Step 1 command
Expected: no output

- [ ] **Step 4: Verify the docs still build**

Run the local Antora build (see "Local Antora build" below).
Expected: Antora exits 0 with no new `ERROR` lines.

- [ ] **Step 5: Commit**

```bash
git add docs/system docs/station
git commit -m "docs: replace semicolons and dash breaks in the System and Station docs"
```

---

### Task 8: Wire the hook

**Files:**
- Create: `.pre-commit-hooks.yaml`
- Modify: `.pre-commit-config.yaml` (add to the existing `repo: local` block)

**Interfaces:**
- Consumes: the checker CLI. All existing docs pass after Task 7.
- Produces: the hook id `adoc-style` that other repos use.

`.pre-commit-hooks.yaml`:

```yaml
- id: adoc-style
  name: AsciiDoc writing style
  description: Checks AsciiDoc prose against the cocktail-maker writing-style rules.
  entry: plugins/cocktail-maker-docs/scripts/check_adoc_style.py
  language: script
  files: \.adoc$
  verbose: true
```

In `.pre-commit-config.yaml`, add this to the `repo: local` hooks:

```yaml
      - id: adoc-style
        name: AsciiDoc writing style
        entry: plugins/cocktail-maker-docs/scripts/check_adoc_style.py
        language: script
        files: ^docs/(system|station)/.*\.adoc$
        verbose: true
```

- [ ] **Step 1: Prove that the hook fails on a violation**

Append `One; two.` to `docs/system/modules/ROOT/pages/index.adoc` and run `uv run pre-commit run adoc-style --files docs/system/modules/ROOT/pages/index.adoc`.
Expected: `Failed`, with `index.adoc:<n>: error: semicolon:`.
Then restore the file with `git checkout docs/system/modules/ROOT/pages/index.adoc`.

- [ ] **Step 2: Run the hook on everything**

Run: `uv run pre-commit run adoc-style --all-files`
Expected: `Passed` (warnings may be printed)

- [ ] **Step 3: Run the whole hook suite and the tests**

Run: `uv run pre-commit run --all-files && uv run pytest`
Expected: every hook passes or is skipped, and all tests pass

- [ ] **Step 4: Commit**

```bash
git add .pre-commit-hooks.yaml .pre-commit-config.yaml
git commit -m "build(docs): run the adoc-style hook and publish it for other repos"
```

---

### Task 9: Mechanics handoff prompt

**Files:**
- Create: `docs/superpowers/handoffs/2026-11-13-mechanics-docs-prompt.md`

**Interfaces:**
- Consumes: the plugin and hook names from Tasks 4 and 8. The pre-commit `rev` is the commit SHA of Task 8, because no release tag holds the hook yet.

The prompt is written for an agent in the private repo `mathisloge/cocktail-maker-3d`, whose Antora component is `mechanics` under `docs/`.
It tells the agent to:
- **Enable the plugin:** add the `extraKnownMarketplaces` and `enabledPlugins` block from Task 4 to `.claude/settings.json`.
- **Add the hook:** add `adoc-style` to `.pre-commit-config.yaml` with `repo: https://github.com/mathisloge/cocktail-maker`, `rev: <Task 8 SHA>` and `files: ^docs/.*\.adoc$`.
- **Load the skills:** use `writing-style`, `architecture-docs` and `writing-adrs` for all the work below.
- **Fix the layout:** run the hook's `--fix` reflow on `docs/**/*.adoc`, and confirm with `git diff --word-diff` that only line breaks changed. Then reword the remaining semicolon and dash findings without changing meaning.
- **Restructure the nav:** split `docs/modules/ROOT/nav.adoc` into an "Architecture" section and a "Guides" section.
  - Architecture holds the arc42 subset chapters 1, 3, 4, 5, 6, 7, 8, 9 and 11, with "Not written yet" pages where nothing exists.
  - Guides holds printing, pump assembly and service, and purchasing.
  - The agent decides where the existing pages fit.
  - Chapter 5 opens with its place in System chapter 5. Link the System chapter 5 page once piece 3 lands, and the System start page until then.
- **ADR titles:** prefix them with `MEC`, as in `= MEC-011: ...`. File names stay, and every reference in the repo is updated.
- **Diagrams:** do not convert existing diagrams yet. The D2 style comes with piece 4 in this repo.
- **Commits:** use conventional commits. The repo already checks them with commitizen.

The prompt follows `writing-style` itself.

- [ ] **Step 1: Write the prompt file**

- [ ] **Step 2: Check it against the style rules**

Run: `python3 plugins/cocktail-maker-docs/scripts/check_adoc_style.py docs/superpowers/handoffs/2026-11-13-mechanics-docs-prompt.md`
Expected: no `error` lines

- [ ] **Step 3: Commit**

```bash
git add docs/superpowers/handoffs/2026-11-13-mechanics-docs-prompt.md
git commit -m "docs: add the handoff prompt for the Mechanics docs"
```

- [ ] **Step 4: Show the prompt to the owner in the final report, ready to paste.**

---

## Local Antora build

There is no Node in the devcontainer, and the 3D repo needs a token.
For a local check:
- Use a portable Node on `PATH`.
- Use a scratch playbook that points the 3D source at a local clone.
- Write output into the scratchpad.

Expected: Antora exits 0. Pre-existing warnings are fine, and new `ERROR` lines are not.

## Not in this plan

- **Piece 2 (navigation), piece 3 (restructure, ADR renames, zoom links) and piece 4 (dark theme, palette generator, D2 migration, Kroki in CI):** each gets its own plan once this one is done.
- **The uncommitted prototype in `docs/ui/supplemental/` and the `supplemental_files` line in `antora-playbook.yml`:** left alone until piece 2.
- **Replacing avoid-list words in existing text:** this belongs to the later rewrite pass.
- **Release automation in CI:** semantic-release was never run in CI, and `cz bump` stays a manual step.
