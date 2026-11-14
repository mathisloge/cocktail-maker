#!/usr/bin/env python3
"""Checks the writing style of AsciiDoc (and Markdown) files, one rule per finding."""

from __future__ import annotations

import argparse
import enum
import re
import sys
from collections.abc import Mapping, Sequence
from pathlib import Path
from typing import NamedTuple


class LineKind(enum.Enum):
    BLANK = enum.auto()
    SKIP = enum.auto()
    HEADING = enum.auto()
    LIST_ITEM = enum.auto()
    TEXT = enum.auto()


class Finding(NamedTuple):
    line: int
    rule: str
    severity: str
    message: str


DEFAULT_AVOID_WORDS = Path(__file__).resolve().parent.parent / "skills" / "writing-style" / "avoid-words.tsv"
OPT_OUT = "// adoc-style: ignore-next-line"

_DELIMITER = re.compile(r"^(-{4,}|\.{4,}|\+{4,}|/{4,}|[|,!]===+|`{3,}.*)$")
_CONTENT_DELIMITER = re.compile(r"^(={4,}|\*{4,}|_{4,}|--)$")
_ATTRIBUTE_LINE = re.compile(r"^\[.*\]$")
_ATTRIBUTE_ENTRY = re.compile(r"^:[\w-]+!?:(\s|$)")
_BLOCK_MACRO = re.compile(r"^[\w-]+::\S*\[.*\]$")
_CONDITIONAL = re.compile(r"^(ifdef|ifndef|ifeval|endif)::")
_BLOCK_TITLE = re.compile(r"^\.[^\s.]")
_HEADING = re.compile(r"^(=+|#+)\s+\S")
_BULLET = re.compile(r"^\s*(?:[*-]+|\.+|\d+\.|<\d+>)\s+")
_DESCRIPTION_TERM = re.compile(r"^\S.*?::(\s|$)")
_BREAK = re.compile(r"^('{3}|<{3})$")
_XREF_LINE = re.compile(r"^xref:\S+\[[^\]]*\]$")
_DOCUMENT_HARDBREAKS = re.compile(r"^:hardbreaks(?:-option)?:(\s|$)")
_HARDBREAKS_OPTION = re.compile(r"%hardbreaks\b|\b(?:options|opts)\s*=\s*\"?[^\"\]]*\bhardbreaks\b")
# Block styles whose line breaks do not show on the page.
# Any other style (source, literal, verse, a diagram) keeps them.
_PROSE_STYLES = frozenset(
    {
        *("note", "tip", "important", "warning", "caution"),
        *("quote", "example", "sidebar", "abstract", "partintro", "normal", "discrete", "float"),
        *("appendix", "glossary", "bibliography", "index", "preface", "colophon", "dedication"),
        *("horizontal", "qanda", "arabic", "decimal", "loweralpha", "upperalpha", "lowerroman", "upperroman"),
        *("lowergreek", "none", "unstyled", "no-bullet", "checklist"),
    }
)

_CODE_SPAN = re.compile(r"`([^`]+)`")
_INLINE_PASS_MACRO = re.compile(r"\b(?:pass|kbd):[\w,]*\[[^\]]*\]")
_INLINE_PASSTHROUGH = re.compile(r"(?<![\w+])(\+{1,3})(?!\s)(.+?)(?<!\s)\1(?![\w+])")
# A URL ends before trailing punctuation, which belongs to the sentence.
_MACRO_TARGET = re.compile(r"\b(?:xref|link|image|include|mailto):[^\s\[]*(?=\[)|https?://[^\s\[\]]*[^\s\[\].,;:!?)]")
_ATTRIBUTE_REFERENCE = re.compile(r"\{[\w-]+\}")
_ENTITY = re.compile(r"&(?:#\d+|#x[0-9a-fA-F]+|\w+);")
_SENTENCE_BOUNDARY = re.compile(r"([.!?])(\s+)(?=[A-Z0-9`*_\"(])")
_DASH = re.compile(r"\s-\s|(?<!<)--(?!>)|[–—]")
_ABBREVIATIONS = frozenset({"e.g.", "i.e.", "etc.", "vs.", "cf.", "approx.", "Fig.", "No."})
_INITIAL = re.compile(r"^[A-Z]\.$")


def classify_lines(lines: list[str]) -> list[LineKind]:
    return _classify(lines)[0]


def _breaks_matter(attribute_line: str) -> bool:
    """Whether a block attribute line gives the next block a style or option whose line breaks matter."""
    if attribute_line.startswith("[["):
        return False
    inner = attribute_line[1:-1]
    first = inner.split(",", 1)[0].strip()
    if _HARDBREAKS_OPTION.search(inner):
        return True
    if "=" in first:
        return False
    style = re.split(r"[#.%]", first, maxsplit=1)[0].strip().lower()
    return bool(style) and style not in _PROSE_STYLES


def _classify(lines: list[str]) -> tuple[list[LineKind], bool]:
    """Classify each line, and tell whether a document attribute turns on hard line breaks."""
    kinds: list[LineKind] = []
    open_delimiter: str | None = None
    # keep_breaks: the next block keeps its line breaks. in_kept_paragraph: such a paragraph is still open.
    keep_breaks = False
    in_kept_paragraph = False
    hardbreaks = False
    for index, line in enumerate(lines):
        stripped = line.strip()
        if index == 0 and stripped == "---":
            kinds.append(LineKind.SKIP)
            open_delimiter = stripped
        elif open_delimiter is not None:
            kinds.append(LineKind.SKIP)
            if stripped == open_delimiter or (open_delimiter.startswith("```") and stripped.startswith("```")):
                open_delimiter = None
        elif _DELIMITER.match(stripped):
            # A delimiter line also ends an open paragraph, as in Asciidoctor.
            kinds.append(LineKind.SKIP)
            open_delimiter = stripped
            keep_breaks = in_kept_paragraph = False
        elif _CONTENT_DELIMITER.match(stripped):
            kinds.append(LineKind.SKIP)
            if keep_breaks:
                open_delimiter = stripped
            keep_breaks = in_kept_paragraph = False
        elif in_kept_paragraph and not _ATTRIBUTE_LINE.match(stripped):
            kinds.append(LineKind.SKIP if stripped else LineKind.BLANK)
            in_kept_paragraph = bool(stripped)
        else:
            kind = _classify_single(line, kinds[-1] if kinds else LineKind.BLANK)
            if _ATTRIBUTE_LINE.match(stripped):
                in_kept_paragraph = False
                keep_breaks = keep_breaks or _breaks_matter(stripped)
            elif _DOCUMENT_HARDBREAKS.match(stripped):
                hardbreaks = True
            elif keep_breaks and kind is not LineKind.BLANK and not _is_block_metadata(stripped):
                keep_breaks = False
                if not _BLOCK_MACRO.match(stripped) and not _CONDITIONAL.match(stripped):
                    kind = LineKind.SKIP
                    in_kept_paragraph = True
            kinds.append(kind)
    return kinds, hardbreaks


def _is_block_metadata(stripped: str) -> bool:
    """Whether a line can stand between block attributes and their block."""
    return bool(
        _ATTRIBUTE_ENTRY.match(stripped)
        or _BLOCK_TITLE.match(stripped)
        or (stripped.startswith("//") and not stripped.startswith("///"))
    )


def _classify_single(line: str, previous: LineKind) -> LineKind:
    stripped = line.strip()
    if not stripped:
        return LineKind.BLANK
    if stripped == "+" or _BREAK.match(stripped) or _XREF_LINE.match(stripped):
        return LineKind.SKIP
    if (
        stripped.startswith("|")
        or _ATTRIBUTE_LINE.match(stripped)
        or _ATTRIBUTE_ENTRY.match(stripped)
        or _BLOCK_MACRO.match(stripped)
        or _CONDITIONAL.match(stripped)
        or _BLOCK_TITLE.match(stripped)
        or (stripped.startswith("//") and not stripped.startswith("///"))
    ):
        return LineKind.SKIP
    if _HEADING.match(line):
        return LineKind.HEADING
    if _BULLET.match(line) or _DESCRIPTION_TERM.match(line):
        return LineKind.LIST_ITEM
    if line[0] in " \t":
        # Indented text directly after text is a continuation, otherwise a literal block.
        return LineKind.TEXT if previous in (LineKind.TEXT, LineKind.LIST_ITEM) else LineKind.SKIP
    return LineKind.TEXT


def _mask(text: str) -> str:
    def blank(match: re.Match[str]) -> str:
        return "x" * len(match.group(0))

    def blank_code(match: re.Match[str]) -> str:
        return "`" + "x" * len(match.group(1)) + "`"

    text = _CODE_SPAN.sub(blank_code, text)
    for pattern in (_INLINE_PASS_MACRO, _INLINE_PASSTHROUGH, _MACRO_TARGET, _ATTRIBUTE_REFERENCE, _ENTITY):
        text = pattern.sub(blank, text)
    return text


def _split_points(masked: str) -> list[int]:
    points = []
    for match in _SENTENCE_BOUNDARY.finditer(masked):
        end = match.start() + 1
        word = masked[:end].split()[-1].lstrip("(\"'*_")
        if match.group(1) == "." and (word in _ABBREVIATIONS or _INITIAL.match(word)):
            continue
        points.append(match.end())
    return points


def split_sentences(line: str) -> list[str]:
    masked = _mask(line)
    bounds = [0, *_split_points(masked), len(line)]
    parts = (line[a:b].strip() for a, b in zip(bounds, bounds[1:], strict=False))
    return [part for part in parts if part]


def _content(line: str, kind: LineKind) -> str:
    if kind is LineKind.LIST_ITEM:
        match = _BULLET.match(line)
        if match:
            return line[match.end() :]
    return line.strip()


def load_avoid_words(path: Path = DEFAULT_AVOID_WORDS) -> dict[str, str]:
    words: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        word, _, replacement = line.partition("\t")
        words[word.strip().lower()] = replacement.strip()
    return words


def _avoid_word_findings(number: int, masked: str, avoid_words: Mapping[str, str]) -> list[Finding]:
    hits: list[tuple[int, Finding]] = []
    for word, replacement in avoid_words.items():
        parts = word.split()
        suffix = r"\w*" if len(parts) == 1 else r"\b"
        pattern = re.compile(r"\b" + r" ".join(re.escape(part) for part in parts) + suffix, re.IGNORECASE)
        for match in pattern.finditer(masked):
            message = f'Avoid "{match.group(0)}", use: {replacement}.'
            hits.append((match.start(), Finding(number, "avoid-word", "warning", message)))
    return [finding for _, finding in sorted(hits, key=lambda hit: hit[0])]


def _line_is_finished(masked: str) -> bool:
    """Whether a line ends a sentence or a hard line break, so the next line must not join it."""
    end = masked.rstrip()
    return end.endswith(" +") or end.rstrip("*_'\")]").endswith((".", "!", "?", ":"))


def check_text(text: str, avoid_words: Mapping[str, str] | None = None) -> list[Finding]:
    lines = text.splitlines()
    kinds, hardbreaks = _classify(lines)
    findings: list[Finding] = []
    ignored: set[int] = set()
    for index, line in enumerate(lines):
        if line.strip() == OPT_OUT:
            ignored.add(index + 1)
    for index, (line, kind) in enumerate(zip(lines, kinds, strict=True)):
        if kind in (LineKind.BLANK, LineKind.SKIP) or index in ignored:
            continue
        number = index + 1
        content = _content(line, kind)
        masked = _mask(content)
        if ";" in masked:
            findings.append(Finding(number, "semicolon", "error", "Avoid semicolons, split into sentences."))
        if _DASH.search(masked):
            findings.append(Finding(number, "dash", "error", "Avoid dashes, use a comma or a new sentence."))
        if avoid_words:
            findings.extend(_avoid_word_findings(number, masked, avoid_words))
        if kind is LineKind.HEADING or hardbreaks:
            # With hard line breaks on, every line break shows on the page, so the layout rules do not apply.
            continue
        if len(split_sentences(content)) > 1:
            findings.append(Finding(number, "sentence-per-line", "error", "Put each sentence on its own line."))
        next_is_text = index + 1 < len(kinds) and kinds[index + 1] is LineKind.TEXT
        if next_is_text and not _line_is_finished(masked):
            findings.append(Finding(number, "sentence-wrapped", "error", "Do not wrap a sentence over lines."))
    return sorted(findings, key=lambda finding: finding.line)


def _run_prefix(line: str, kind: LineKind) -> str:
    if kind is LineKind.LIST_ITEM:
        match = _BULLET.match(line)
        if match:
            return match.group(0)
    return line[: len(line) - len(line.lstrip())]


def reflow(text: str) -> str:
    """Rejoin each text run and put every sentence on its own line, changing nothing but line breaks."""
    lines = text.split("\n")
    kinds, hardbreaks = _classify(lines)
    if hardbreaks:
        return text
    out: list[str] = []
    index = 0
    while index < len(lines):
        if kinds[index] not in (LineKind.TEXT, LineKind.LIST_ITEM):
            out.append(lines[index])
            index += 1
            continue
        first = lines[index]
        prefix = _run_prefix(first, kinds[index])
        words = [first[len(prefix) :].strip()]
        index += 1
        while not _line_is_finished(_mask(words[-1])) and index < len(lines) and kinds[index] is LineKind.TEXT:
            words.append(lines[index].strip())
            index += 1
        sentences = split_sentences(" ".join(words)) or [""]
        out.append(prefix + sentences[0])
        refused = False
        for position in range(1, len(sentences)):
            remainder = " ".join(sentences[position:])
            # A new line must not turn into a list item, heading or other block line.
            if refused or classify_lines([remainder])[0] is not LineKind.TEXT:
                out[-1] += " " + sentences[position]
                refused = True
            else:
                out.append(sentences[position])
    return "\n".join(out)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Check the writing style of AsciiDoc files.")
    parser.add_argument("files", nargs="*", type=Path)
    parser.add_argument("--fix", action="store_true", help="rewrite files to fix what can be fixed")
    args = parser.parse_args(argv)
    avoid_words = load_avoid_words()
    failed = False
    for path in args.files:
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as error:
            print(f"{path}: error: {error}")
            failed = True
            continue
        if args.fix:
            fixed = reflow(text)
            if fixed != text:
                path.write_text(fixed, encoding="utf-8")
                text = fixed
        for finding in check_text(text, avoid_words):
            print(f"{path}:{finding.line}: {finding.severity}: {finding.rule}: {finding.message}")
            failed = failed or finding.severity == "error"
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
