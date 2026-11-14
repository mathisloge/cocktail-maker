import re
from pathlib import Path

import pytest
from generate_palette import main, mix, read_primitives, render_css, render_d2

REPO = Path(__file__).resolve().parents[3]
SLINT = REPO / "gui/ui/theme/colors.slint"


def colors() -> dict[str, str]:
    return read_primitives(SLINT.read_text())


def test_read_primitives_from_the_real_file():
    found = colors()
    assert found["ink"] == "#0a0806" and found["log-gray-blue"] == "#7a8594"
    assert list(found)[:3] == ["ink", "ink-raised", "ink-elevated"]


def test_read_primitives_rejects_missing_block():
    with pytest.raises(ValueError):
        read_primitives("global Palette {}")


def test_mix():
    assert mix("#d9cfae", "#0a0806", 1.0) == "#d9cfae"
    assert mix("#ffffff", "#000000", 0.5) == "#808080"


def test_css_has_every_primitive_and_both_derived_shades():
    found = colors()
    css = render_css(found)
    assert css.splitlines()[0].startswith("/*") and "generate_palette.py" in css.splitlines()[0]
    for name, value in found.items():
        assert f"--cm-{name}: {value};" in css
    assert "--cm-sand-muted:" in css and "--cm-sand-soft:" in css


# Each edge kind: its colour and its dash, None for a solid line.
EDGE_KINDS = {
    "power": ("#d0654a", None),
    "data": ("#86aebd", 4),
    "fluid": ("#5cad75", None),
    "mechanical": ("#8c7ee0", 2),
    "interaction": ("#e8b86d", None),
    "fault": ("#e07aa8", None),
}


def d2_class(d2: str, name: str) -> str:
    return re.search(rf"^  {name}: \{{.*?^  \}}", d2, re.MULTILINE | re.DOTALL).group(0)


def contrast(a: str, b: str) -> float:
    def luminance(colour: str) -> float:
        channels = [int(colour[i : i + 2], 16) / 255 for i in (1, 3, 5)]
        r, g, b = (c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in channels)
        return 0.2126 * r + 0.7152 * g + 0.0722 * b

    high, low = sorted((luminance(a), luminance(b)), reverse=True)
    return (high + 0.05) / (low + 0.05)


def test_the_docs_primitives_are_in_colors_slint():
    found = colors()
    assert found["orange-red"] == "#d0654a" and found["violet"] == "#8c7ee0" and found["pink"] == "#e07aa8"


def test_d2_has_the_classes_and_background():
    found = colors()
    d2 = render_d2(found)
    for name in ("actor", "external", "boundary", "block", "normal", "key", *EDGE_KINDS, *PART_KINDS):
        assert re.search(rf"^\s*{name}: \{{", d2, re.MULTILINE), name
    assert 'style.fill: "#0a0806"' in d2
    allowed = set(found.values()) | {mix(found["sand"], found["ink"], p) for p in (0.6, 0.8)}
    assert set(re.findall(r"#[0-9a-f]{6}", d2)) <= allowed


def test_d2_actor_has_a_fixed_small_size():
    actor = d2_class(render_d2(colors()), "actor")
    assert "width: 64" in actor and "height: 80" in actor


def test_d2_uses_the_tala_layout_engine():
    d2 = render_d2(colors())
    assert "layout-engine: tala" in d2
    assert "elk" not in d2


def test_check_mode_detects_drift(tmp_path):
    css, d2 = tmp_path / "palette.css", tmp_path / "d2-style.d2"
    args = ["--slint", str(SLINT), "--css", str(css), "--d2", str(d2)]
    assert main(args) == 0
    assert main(["--check", *args]) == 0
    d2.write_text(d2.read_text() + "# edit\n")
    assert main(["--check", *args]) == 1


def test_d2_has_no_root_edge_glob_that_would_override_the_key_class():
    d2 = render_d2(colors())
    assert "(**" not in d2, "a root edge glob overrides the key class; edges use class: normal instead"
    normal = re.search(r"^  normal: \{.*?^  \}", d2, re.MULTILINE | re.DOTALL).group(0)
    assert '"#867f6b"' in normal and '"#b0a78c"' in normal


def test_read_primitives_ignores_block_comments():
    text = "global Primitives {\n /* out property <color> x: #111111; */\n out property <color> a: #AABBCC;\n}\n"
    assert read_primitives(text) == {"a": "#aabbcc"}


@pytest.mark.parametrize("kind", EDGE_KINDS)
def test_d2_edge_kinds_have_their_fixed_colour(kind):
    style = d2_class(render_d2(colors()), kind)
    colour, _ = EDGE_KINDS[kind]
    assert f'stroke: "{colour}"' in style and f'font-color: "{colour}"' in style
    assert contrast(colour, colors()["ink"]) >= 4.5
    assert contrast(colour, colors()["ink-elevated"]) >= 4.5


@pytest.mark.parametrize("kind", EDGE_KINDS)
def test_d2_edge_kinds_have_their_line_style(kind):
    style = d2_class(render_d2(colors()), kind)
    _, dash = EDGE_KINDS[kind]
    assert "stroke-width: 2" in style
    assert re.findall(r"stroke-dash: (\d+)", style) == ([str(dash)] if dash else [])


def test_d2_normal_edges_are_thin():
    assert "stroke-width: 1" in d2_class(render_d2(colors()), "normal")


PART_KINDS = ("power", "data", "fluid", "mechanical", "interaction")


@pytest.mark.parametrize("kind", PART_KINDS)
def test_d2_part_classes_outline_a_block_in_the_kind_colour(kind):
    found = colors()
    d2 = render_d2(found)
    part = d2_class(d2, f"{kind}-part")
    assert f'stroke: "{EDGE_KINDS[kind][0]}"' in part
    assert f'fill: "{found["ink-elevated"]}"' in part and f'font-color: "{found["cream"]}"' in part
    assert "border-radius: 4" in part
    assert "stroke-dash" not in part and "stroke-width" not in part, "a box outline is always solid"


def test_d2_has_no_fault_part():
    assert "fault-part" not in render_d2(colors())


def test_the_edge_kind_colours_are_all_different():
    assert len({colour for colour, _ in EDGE_KINDS.values()}) == len(EDGE_KINDS)


def test_d2_key_class_sets_only_width_and_a_bold_label():
    key = d2_class(render_d2(colors()), "key")
    assert "stroke-width: 3" in key and "bold: true" in key
    assert not re.search(r"#[0-9a-f]{6}", key), "key combines with a kind class, so it sets no colour"
