from pathlib import Path

import pytest
from check_adoc_style import LineKind, check_text, classify_lines, load_avoid_words, main, reflow

REPO = Path(__file__).resolve().parents[3]


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
    assert rules("See xref:adr:002-x.adoc[SYS-002] for details.\n") == []


def test_wrapped_sentence_flagged():
    assert rules("This sentence goes\non here.\n") == [(1, "sentence-wrapped")]


def test_colon_and_hard_break_ok():
    assert rules("Steps:\n* One.\n\nLine one +\nline two.\n") == []


def test_list_items_without_period_ok():
    assert rules("* xref:a.adoc[]\n* xref:b.adoc[]\n") == []


@pytest.mark.parametrize(
    ("open_", "close"),
    [
        ("----", "----"),
        ("....", "...."),
        ("++++", "++++"),
        ("////", "////"),
        ("```", "```"),
        ("|===", "|==="),
        (",===", ",==="),
        ("!===", "!==="),
    ],
)
def test_skipped_blocks(open_, close):
    assert rules(f"[d2, target=x]\n{open_}\na; b - c. D\n{close}\n") == []


def test_skipped_lines():
    text = ':attr: a; b\n// note; x - y\ninclude::partial$x.adoc[]\n[cols="1;2"]\n.Title; here\n'
    assert rules(text) == []


def test_opt_out():
    assert rules("// adoc-style: ignore-next-line\nOne; two.\n") == []


def test_classify_lines():
    assert classify_lines(["= T", "", "Text.", "* Item.", "----", "x", "----"]) == [
        LineKind.HEADING,
        LineKind.BLANK,
        LineKind.TEXT,
        LineKind.LIST_ITEM,
        LineKind.SKIP,
        LineKind.SKIP,
        LineKind.SKIP,
    ]


def test_indented_continuation_is_text():
    kinds = classify_lines(["* Item one", "  wraps. Second.", "* Next."])
    assert kinds == [LineKind.LIST_ITEM, LineKind.TEXT, LineKind.LIST_ITEM]


@pytest.mark.parametrize("delimiter", ["====", "****", "____", "--"])
def test_content_blocks_checked_delimiters_skipped(delimiter):
    assert rules(f"{delimiter}\nA note here.\nMore here.\n{delimiter}\n") == []
    assert rules(f"{delimiter}\nOne; two.\n{delimiter}\n") == [(2, "semicolon")]


def test_indented_fence_is_skipped():
    assert rules("* Item\n  ```\n  a; b\n  ```\n") == []


def test_frontmatter_is_skipped():
    assert rules("---\nname: x; y\n---\nOne; two.\n") == [(4, "semicolon")]


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
    bad = tmp_path / "bad.adoc"
    bad.write_text("One; two.\n")
    warn = tmp_path / "warn.adoc"
    warn.write_text("We utilize it.\n")
    assert main([str(bad)]) == 1
    assert f"{bad}:1: error: semicolon:" in capsys.readouterr().out
    assert main([str(warn)]) == 0
    assert f"{warn}:1: warning: avoid-word:" in capsys.readouterr().out


def _structure(text):
    lines = text.split("\n")
    kinds = classify_lines(lines)
    fixed = [(k, line) for k, line in zip(kinds, lines, strict=True) if k not in (LineKind.TEXT, LineKind.LIST_ITEM)]
    return fixed, sum(k is LineKind.LIST_ITEM for k in kinds)


def test_reflow_paragraph():
    assert reflow("One two\nthree. Four\nfive.\n") == "One two three.\nFour five.\n"


def test_reflow_list_item():
    assert reflow("* Item one\n  wraps. Second.\n* Next.\n") == "* Item one wraps.\nSecond.\n* Next.\n"


def test_reflow_abbreviation_kept():
    assert reflow("A pump, e.g. a\nperistaltic one.\n") == "A pump, e.g. a peristaltic one.\n"


def test_reflow_untouched_lines():
    text = "= Title\n\n[source,cpp]\n----\nint a; int b;\n----\n\nLine one +\nline two.\n"
    assert reflow(text) == text


def test_reflow_idempotent_and_word_preserving():
    text = (REPO / "docs/station/modules/ROOT/pages/building-blocks.adoc").read_text()
    out = reflow(text)
    assert reflow(out) == out
    assert out.split() == text.split()
    assert _structure(out) == _structure(text)


def test_reflow_keeps_missing_trailing_newline():
    assert reflow("A b\nc.") == "A b c."


def test_main_fix_writes_file(tmp_path):
    p = tmp_path / "a.adoc"
    p.write_text("This goes\non.\n")
    assert main(["--fix", str(p)]) == 0
    assert p.read_text() == "This goes on.\n"


def test_main_non_utf8_file(tmp_path, capsys):
    p = tmp_path / "bad.adoc"
    p.write_bytes(b"\xff\xfe bad")
    assert main([str(p)]) == 1
    assert str(p) in capsys.readouterr().out


def test_multi_word_avoid_key_ends_at_word_boundary():
    assert check_text("We act in order tomorrow.\n", load_avoid_words()) == []
    assert check_text("We act in order to pour.\n", load_avoid_words())


def test_list_continuation_plus_untouched():
    text = "* item\n+\n----\ncode\n----\n"
    assert reflow(text) == text
    assert check_text(text) == []


def test_list_continuation_plus_then_paragraph():
    assert reflow("* item one.\n+\nMore para. Two.\n") == "* item one.\n+\nMore para.\nTwo.\n"


@pytest.mark.parametrize("text", ["Do this. * Not a bullet. Done.\n", "Do this first. 1. Numbered item.\n"])
def test_reflow_never_creates_list_item(text):
    out = reflow(text)
    assert out == text
    assert _structure(out) == _structure(text)


@pytest.mark.parametrize(
    "attribute",
    [
        "[source,sh]",
        "[source%linenums,sh]",
        "[listing]",
        "[literal]",
        "[verse]",
        "[verse, Author]",
        "[pass]",
        "[stem]",
        "[latexmath]",
        "[asciimath]",
        "[d2]",
        "[plantuml, target=x]",
        "[mermaid]",
        "[graphviz]",
        "[ditaa]",
        "[svgbob]",
        "[%hardbreaks]",
        "[.role%hardbreaks]",
        "[#id%hardbreaks]",
        '[options="hardbreaks"]',
        "[options=hardbreaks]",
    ],
)
def test_styled_paragraph_untouched(attribute):
    text = f"Intro.\n\n{attribute}\nuv sync\nuv run pytest; echo done\n\nAfter one\nwrapped.\n"
    assert reflow(text) == text.replace("After one\nwrapped.", "After one wrapped.")
    assert rules(text) == [(7, "sentence-wrapped")]


def test_styled_paragraph_after_title_and_anchor_untouched():
    text = ".Install\n[source,sh]\n[#install]\nuv sync\nuv run pytest; echo done\n"
    assert reflow(text) == text
    assert rules(text) == []


@pytest.mark.parametrize("delimiter", ["____", "====", "****", "--"])
def test_styled_content_block_untouched(delimiter):
    text = f"[verse]\n{delimiter}\nThe pump turns\nand turns; it hums - softly\n{delimiter}\n\nOne; two.\n"
    assert reflow(text) == text
    assert rules(text) == [(7, "semicolon")]


@pytest.mark.parametrize(
    "attribute",
    ["[NOTE]", "[TIP]", "[WARNING]", "[quote, Author]", "[example]", "[sidebar]", "[abstract]", "[#id.role]"],
)
def test_prose_styles_still_checked_and_reflowed(attribute):
    text = f"{attribute}\nThis goes\non; here.\n"
    assert reflow(text) == f"{attribute}\nThis goes on; here.\n"
    assert rules(text) == [(2, "sentence-wrapped"), (3, "semicolon")]


@pytest.mark.parametrize("entry", [":hardbreaks-option:", ":hardbreaks:"])
def test_hardbreaks_document_attribute_disables_layout(entry):
    text = f"= Title\n{entry}\n\nRoses are red\nviolets are blue. Two here.\nOne; two.\n"
    assert reflow(text) == text
    assert rules(text) == [(6, "semicolon")]


def test_hardbreaks_entry_in_listing_does_not_count():
    text = "----\n:hardbreaks-option:\n----\n\nThis goes\non.\n"
    assert rules(text) == [(5, "sentence-wrapped")]
    assert reflow(text) == "----\n:hardbreaks-option:\n----\n\nThis goes on.\n"


@pytest.mark.parametrize("url", ["https://github.com/x", "https://github.com/x/y.git", "http://a.b/c?d=e"])
@pytest.mark.parametrize("end", [".", "!", "?"])
def test_url_at_sentence_end(url, end):
    text = f"The firmware lives at {url}{end}\nNext sentence here.\n"
    assert rules(text) == []
    assert reflow(text) == text


def test_url_sentence_end_mid_line_splits():
    assert rules("See https://a.b/c. Next here.\n") == [(1, "sentence-per-line")]
    assert reflow("See https://a.b/c. Next here.\n") == "See https://a.b/c.\nNext here.\n"


@pytest.mark.parametrize("tail", [",", ";", ":", ")"])
def test_url_trailing_punctuation_is_prose(tail):
    found = rules(f"Go (see https://a.b/c{tail} now).\n")
    assert found == ([(1, "semicolon")] if tail == ";" else [])


def test_xref_only_line_is_skipped():
    text = "See the page:\n\nxref:a.adoc[A]\nThe next line is here.\n"
    assert classify_lines(text.split("\n"))[2] is LineKind.SKIP
    assert rules(text) == []
    assert reflow(text) == text


@pytest.mark.parametrize("line", ["'''", "<<<"])
def test_breaks_are_skipped(line):
    text = f"One here.\n\n{line}\n\nTwo here.\n"
    assert classify_lines(text.split("\n"))[2] is LineKind.SKIP
    assert rules(text) == []
    assert reflow(text) == text


@pytest.mark.parametrize(
    "line",
    [
        "Type +a;b+ here.",
        "Type ++a;b++ here.",
        "Type +++a;b+++ here.",
        "Type pass:[a;b] here.",
        "Type pass:q,a[a;b] here.",
        "Press kbd:[Ctrl+;] now.",
        "Press kbd:[Ctrl+-] or kbd:[Ctrl+--] now.",
    ],
)
def test_passthrough_and_kbd_masked(line):
    assert rules(line + "\n") == []


def test_plus_signs_in_prose_are_not_passthrough():
    assert rules("Use C++ and C++; it works.\n") == [(1, "semicolon")]
    assert rules("Add a + b; done.\n") == [(1, "semicolon")]


@pytest.mark.parametrize("line", ["A --> B is the flow.", "A <-- B is the flow.", "A <--> B is the flow."])
def test_arrows_are_not_dashes(line):
    assert rules(line + "\n") == []


def test_dash_next_to_arrow_still_flagged():
    assert rules("A --> B -- then C.\n") == [(1, "dash")]


@pytest.mark.parametrize(
    "text",
    [
        "Clang crashed.\nstdexec is used instead.\n",
        "It needs this:\nthe pump.\n",
        "Is it busy?\nmaybe not.\n",
        "Read **this.**\nthen go.\n",
        "Line one +\nline two.\n",
    ],
)
def test_reflow_never_joins_a_finished_line(text):
    assert rules(text) == []
    assert reflow(text) == text


def test_styled_paragraph_ends_at_delimiter():
    text = "[source,sh]\nuv sync\n----\nx; y\nz - w\n----\nThis goes\non.\n"
    assert rules(text) == [(7, "sentence-wrapped")]
    assert reflow(text) == text.replace("This goes\non.", "This goes on.")


def test_styled_paragraph_ends_at_attribute_line():
    text = "[verse]\nThe pump\nturns\n[NOTE]\nThis goes\non.\n"
    assert rules(text) == [(5, "sentence-wrapped")]
    assert reflow(text) == "[verse]\nThe pump\nturns\n[NOTE]\nThis goes on.\n"
