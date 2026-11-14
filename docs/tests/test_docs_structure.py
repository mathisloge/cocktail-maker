"""Checks the arc42 structure of the docs.

The source tests read the AsciiDoc files and need no built site.
The site tests need a built site: build it first and point `DOCS_SITE_DIR` at its output folder.
Without `DOCS_SITE_DIR`, the site tests are skipped.
Every build fetches its diagrams (`kroki-fetch-diagram` in the playbook), so a built site holds them as SVG files.
"""

import os
import re
import shutil
import subprocess
from html.parser import HTMLParser
from pathlib import Path
from typing import NamedTuple

import pytest
from check_adoc_style import GlossaryRow, load_glossary

REPO = Path(__file__).resolve().parents[2]

SYSTEM_PAGES = REPO / "docs" / "system" / "modules" / "ROOT" / "pages"

D2_STYLE = REPO / "docs" / "system" / "modules" / "ROOT" / "partials" / "d2-style.d2"

SITE_DIR = Path(os.environ["DOCS_SITE_DIR"]).resolve() if os.environ.get("DOCS_SITE_DIR") else None

site_test = pytest.mark.skipif(SITE_DIR is None, reason="DOCS_SITE_DIR is not set")

SYSTEM_CHAPTERS = [
    "1 Introduction and Goals",
    "2 Constraints",
    "3 Context and Scope",
    "4 Solution Strategy",
    "5 Building Block View",
    "6 Runtime View",
    "7 Deployment View",
    "8 Cross-cutting Concepts",
    "9 Architecture Decisions",
    "10 Quality Requirements",
    "11 Risks and Technical Debt",
    "12 Glossary",
]

# The page of each System chapter, in chapter order.
SYSTEM_CHAPTER_FILES = [
    "index.adoc",
    "constraints.adoc",
    "context-and-scope.adoc",
    "solution-strategy.adoc",
    "building-blocks.adoc",
    "runtime-view.adoc",
    "deployment-view.adoc",
    "cross-cutting-concepts.adoc",
    "architecture-decisions.adoc",
    "quality-requirements.adoc",
    "risks-and-technical-debt.adoc",
    "glossary.adoc",
]

# The System chapters that have no content yet.
SYSTEM_PLACEHOLDERS = [
    "constraints.adoc",
    "runtime-view.adoc",
    "deployment-view.adoc",
    "quality-requirements.adoc",
    "risks-and-technical-debt.adoc",
]

PLACEHOLDER = "[NOTE]\n====\nNot written yet.\n====\n"


class _NavTitleParser(HTMLParser):
    """Collects the texts of the entries in the sidebar nav, in document order."""

    def __init__(self) -> None:
        super().__init__()
        self.titles: list[str] = []
        self._depth = 0  # how deep inside the `nav-menu` element the parser is, 0 outside it
        self._entry: list[str] | None = None
        self._entry_tag = ""

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        classes = dict(attrs).get("class", "") or ""
        if self._depth == 0:
            if tag == "nav" and "nav-menu" in classes.split():
                self._depth = 1
            return
        self._depth += 1
        if self._entry is None and (
            (tag == "a" and "nav-link" in classes.split()) or (tag == "span" and "nav-text" in classes.split())
        ):
            self._entry = []
            self._entry_tag = tag

    def handle_endtag(self, tag: str) -> None:
        if self._depth == 0:
            return
        if self._entry is not None and tag == self._entry_tag:
            self.titles.append(" ".join("".join(self._entry).split()))
            self._entry = None
        self._depth -= 1

    def handle_data(self, data: str) -> None:
        if self._entry is not None:
            self._entry.append(data)


def nav_titles(site_dir: Path, component: str) -> list[str]:
    """The titles of the sidebar nav entries of `component`, in nav order."""
    parser = _NavTitleParser()
    parser.feed((site_dir / component / "index.html").read_text(encoding="utf-8"))
    parser.close()
    return parser.titles


def first_line(path: Path) -> str:
    return path.read_text(encoding="utf-8").splitlines()[0]


@pytest.mark.parametrize(("file", "title"), list(zip(SYSTEM_CHAPTER_FILES, SYSTEM_CHAPTERS, strict=True)))
def test_system_chapter_pages_exist_with_numbered_titles(file: str, title: str) -> None:
    path = SYSTEM_PAGES / file
    assert path.is_file(), f"{file} is missing"
    assert first_line(path) == f"= {title}"


def test_system_design_is_gone() -> None:
    assert not (SYSTEM_PAGES / "system-design.adoc").exists()
    linking = [
        path.relative_to(REPO).as_posix()
        for path in sorted((REPO / "docs").rglob("*.adoc"))
        if "system-design.adoc" in path.read_text(encoding="utf-8")
    ]
    assert linking == [], f"pages still link system-design.adoc: {linking}"


@pytest.mark.parametrize("file", SYSTEM_PLACEHOLDERS)
def test_placeholders_use_the_admonition(file: str) -> None:
    path = SYSTEM_PAGES / file
    assert path.is_file(), f"{file} is missing"
    assert PLACEHOLDER in path.read_text(encoding="utf-8")


@site_test
def test_system_nav_lists_chapters_in_order() -> None:
    assert [t for t in nav_titles(SITE_DIR, "system") if t[:1].isdigit()] == SYSTEM_CHAPTERS


STATION_PAGES = REPO / "docs" / "station" / "modules" / "ROOT" / "pages"

STATION_CHAPTERS = [
    "1 Introduction and Goals",
    "2 Constraints",
    "3 Context and Scope",
    "4 Solution Strategy",
    "5 Building Block View",
    "6 Runtime View",
    "7 Deployment View",
    "8 Cross-cutting Concepts",
    "9 Architecture Decisions",
    "11 Risks and Technical Debt",
]

# The page of each Station chapter, in chapter order.
STATION_CHAPTER_FILES = [
    "index.adoc",
    "constraints.adoc",
    "context-and-scope.adoc",
    "solution-strategy.adoc",
    "building-blocks.adoc",
    "runtime-view.adoc",
    "deployment-view.adoc",
    "cross-cutting-concepts.adoc",
    "architecture-decisions.adoc",
    "risks-and-technical-debt.adoc",
]


@pytest.mark.parametrize(("file", "title"), list(zip(STATION_CHAPTER_FILES, STATION_CHAPTERS, strict=True)))
def test_station_chapter_titles(file: str, title: str) -> None:
    path = STATION_PAGES / file
    assert path.is_file(), f"{file} is missing"
    assert first_line(path) == f"= {title}"


def test_zoom_links_both_ways() -> None:
    system = (SYSTEM_PAGES / "building-blocks.adoc").read_text(encoding="utf-8")
    assert "xref:station::building-blocks.adoc[" in system
    assert "xref:mechanics::building-blocks.adoc[" in system
    station = (STATION_PAGES / "building-blocks.adoc").read_text(encoding="utf-8").splitlines()[:6]
    assert any("xref:system::building-blocks.adoc[" in line for line in station)
    linking = [
        path.relative_to(REPO).as_posix()
        for path in sorted((REPO / "docs" / "system").rglob("*.adoc"))
        if "xref:embedded::" in path.read_text(encoding="utf-8")
    ]
    assert linking == [], f"System pages link the embedded component: {linking}"


@site_test
def test_station_nav_lists_chapters_in_order() -> None:
    assert [t for t in nav_titles(SITE_DIR, "station") if t[:1].isdigit()] == STATION_CHAPTERS


ADR_PAGES = {
    "SYS": REPO / "docs" / "system" / "modules" / "adr" / "pages",
    "STA": REPO / "docs" / "station" / "modules" / "adr" / "pages",
}

UNPREFIXED_ADR_ID = re.compile(r"\bADR[- ]?\d{3}\b")


@pytest.mark.parametrize("prefix", sorted(ADR_PAGES))
def test_adr_titles_carry_the_component_prefix(prefix: str) -> None:
    pages = sorted(ADR_PAGES[prefix].glob("[0-9][0-9][0-9]-*.adoc"))
    assert pages, f"no ADR pages found for {prefix}"
    wrong = [
        f"{path.name}: {first_line(path)}"
        for path in pages
        if not re.match(rf"^= {prefix}-{path.name[:3]}: \S", first_line(path))
    ]
    assert wrong == [], f"ADR titles without the {prefix} prefix: {wrong}"


# The plugin skills and the handoffs that tell writers how the docs are built.
# The checker test is in the list because its samples look like ADR references too.
WRITER_GUIDANCE = [
    *sorted((REPO / "plugins").rglob("*.md")),
    *sorted((REPO / "plugins").rglob("*.adoc")),
    REPO / "plugins" / "cocktail-maker-docs" / "tests" / "test_check_adoc_style.py",
    *sorted((REPO / "docs" / "superpowers" / "handoffs").glob("*.md")),
]

# Names of pages that the restructure removed.
REMOVED_PAGES = ["999-example", "system-design.adoc"]


def test_no_unprefixed_adr_ids_left() -> None:
    files = [
        *sorted((REPO / "docs" / "system").rglob("*.adoc")),
        *sorted((REPO / "docs" / "station").rglob("*.adoc")),
        REPO / "cocktail-maker" / "fill_level.cppm",
        REPO / "core" / "strong_type.test.cpp",
        *WRITER_GUIDANCE,
    ]
    found = [
        f"{path.relative_to(REPO).as_posix()}:{number}: {match.group(0)}"
        for path in files
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1)
        for match in UNPREFIXED_ADR_ID.finditer(line)
    ]
    assert found == [], f"unprefixed ADR IDs: {found}"


def test_writer_guidance_names_no_removed_pages() -> None:
    found = [
        f"{path.relative_to(REPO).as_posix()}:{number}: {name}"
        for path in WRITER_GUIDANCE
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1)
        for name in REMOVED_PAGES
        if name in line
    ]
    assert found == [], f"writer guidance names removed pages: {found}"


def test_example_adr_is_gone() -> None:
    assert not (ADR_PAGES["SYS"] / "999-example.adoc").exists()


CROSS_COMPONENT_XREF = re.compile(r"xref:(?:system|station|mechanics|embedded):[^\[\s]*\[([^\]]*)\]")

# Words that name a component in a link text: the component names, the ADR prefixes and the stable System IDs.
COMPONENT_NAMES = re.compile(r"\b(?:System|Station|Mechanics|Embedded|SYS-|STA-|EMB-|CM3D-|QG-|QS-|SM-|IF-)")


def test_cross_component_xrefs_name_the_component() -> None:
    files = [
        *sorted((REPO / "docs" / "system").rglob("*.adoc")),
        *sorted((REPO / "docs" / "station").rglob("*.adoc")),
    ]
    found = [
        f"{path.relative_to(REPO).as_posix()}:{number}: {match.group(0)}"
        for path in files
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1)
        for match in CROSS_COMPONENT_XREF.finditer(line)
        if not COMPONENT_NAMES.search(match.group(1))
    ]
    assert found == [], f"cross-component xrefs without the component in the link text: {found}"


@pytest.mark.parametrize("prefix", sorted(ADR_PAGES))
def test_chapter_9_lists_every_adr(prefix: str) -> None:
    chapter = (ADR_PAGES[prefix].parents[1] / "ROOT" / "pages" / "architecture-decisions.adoc").read_text(
        encoding="utf-8"
    )
    missing = [
        path.name
        for path in sorted(ADR_PAGES[prefix].glob("[0-9][0-9][0-9]-*.adoc"))
        if f"* xref:adr:{path.name}[]" not in chapter.splitlines()
    ]
    assert missing == [], f"ADRs missing from the {prefix} chapter 9 page: {missing}"


D2_STYLE_INCLUDE = "include::system:ROOT:partial$d2-style.d2[]"
D2_BLOCK_START = re.compile(r"^\[d2\b")
D2_EDGE = re.compile(r"<->|->|<-|--")
# The edge kinds of the shared style, one fixed colour each. `normal` is the muted edge without a kind.
D2_EDGE_KINDS = frozenset({"normal", "power", "data", "fluid", "mechanical", "interaction", "fault"})
# `class: data` or `class: [data; key]`.
D2_CLASS = re.compile(r"\bclass:\s*(\[[^\]]*\]|[\w-]+)")


def d2_blocks(path: Path) -> list[tuple[int, list[str]]]:
    """The `[d2]` blocks of an AsciiDoc file, as the line number of the block attribute line and the source lines."""
    lines = path.read_text(encoding="utf-8").splitlines()
    blocks = []
    number = 0
    while number < len(lines):
        if D2_BLOCK_START.match(lines[number]) and number + 1 < len(lines):
            delimiter = lines[number + 1]
            end = lines.index(delimiter, number + 2)
            blocks.append((number + 1, lines[number + 2 : end]))
            number = end
        number += 1
    return blocks


# A quoted D2 string, `"..."` or `'...'`, with backslash escapes inside.
# A quote starts a string only at the start of a value, so the apostrophe in `Pod's` starts none.
D2_QUOTED = re.compile(r"""(?<![^\s:{};,.<>\-\[(])(?:"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')""")


def without_quoted(line: str) -> str:
    """A D2 line with every quoted string emptied to `""`, so a `#`, a brace or an arrow in a label does not count."""
    return D2_QUOTED.sub('""', line)


def d2_edges(source: list[str]) -> list[str]:
    """The edge statements of a D2 source, each with its `{...}` map joined onto one line.

    Quoted strings come back emptied to `""`, so a label cannot hide or fake a class.
    """
    edges = []
    statement: list[str] = []
    depth = 0
    for raw in source:
        line = without_quoted(raw).split("#", 1)[0].strip() if not raw.lstrip().startswith("include::") else ""
        if statement:
            statement.append(line)
            depth += line.count("{") - line.count("}")
            if depth <= 0:
                edges.append(" ".join(statement))
                statement = []
            continue
        if not D2_EDGE.search(line):
            continue
        depth = line.count("{") - line.count("}")
        if depth > 0:
            statement = [line]
        else:
            edges.append(line)
    return edges


def d2_edge_classes(edge: str) -> list[list[str]]:
    """The classes of each `class:` in a D2 edge statement, `class: data` as `[["data"]]`, `class: [data; key]` as
    `[["data", "key"]]`."""
    return [[name.strip() for name in value.strip("[]").split(";") if name.strip()] for value in D2_CLASS.findall(edge)]


def d2_edge_has_a_kind(edge: str) -> bool:
    """An edge has one `class:` with exactly one edge kind, optionally followed by `key`.

    In a class array the later class wins, so `key` comes last to set its width over the width of the kind.
    """
    match d2_edge_classes(edge):
        case [[kind]] | [[kind, "key"]]:
            return kind in D2_EDGE_KINDS
    return False


def d2_edge_is_key(edge: str) -> bool:
    return any("key" in classes for classes in d2_edge_classes(edge))


D2_SHAPE_CLASSES = frozenset(
    {
        "actor",
        "external",
        "boundary",
        "block",
        *(f"{kind}-part" for kind in ("power", "data", "fluid", "mechanical", "interaction")),
    }
)
# The keys of a D2 map that set something on the map instead of declaring a shape.
D2_RESERVED_KEYS = frozenset(
    {
        "class",
        "classes",
        "direction",
        "shape",
        "style",
        "vars",
        "label",
        "near",
        "icon",
        "tooltip",
        "link",
        "width",
        "height",
    }
)
D2_KEY = re.compile(r"^([\w.-]+)\s*:")


def d2_shapes(source: list[str]) -> list[tuple[str, list[str]]]:
    """The shapes and containers of a D2 source, each with its key and the classes of its `class:`.

    A shape takes `{class: x}` on its line, and a container takes a `class: x` line inside its braces.
    """
    shapes = []
    stack: list[list] = []  # [key or None, classes] for each open map; None for a map that is not a shape
    for raw in source:
        if raw.lstrip().startswith("include::"):
            continue
        line = without_quoted(raw).split("#", 1)[0].strip()
        if not line:
            continue
        opened, closed = line.count("{"), line.count("}")
        key = D2_KEY.match(line)
        inside_a_shape = all(frame[0] is not None for frame in stack)
        if line == "}":
            frame = stack.pop()
            if frame[0] is not None:
                shapes.append((frame[0], frame[1]))
            continue
        if key and key.group(1) == "class" and stack and stack[-1][0] is not None:
            stack[-1][1] = [name for classes in d2_edge_classes(line) for name in classes]
            continue
        is_shape = (
            inside_a_shape
            and key is not None
            and not D2_EDGE.search(line)
            and not set(key.group(1).split(".")) & D2_RESERVED_KEYS
        )
        if opened > closed:
            stack.append([key.group(1) if is_shape else None, []])
        elif is_shape:
            shapes.append((key.group(1), [name for classes in d2_edge_classes(line) for name in classes]))
    return shapes


def d2_shape_has_a_class(classes: list[str]) -> bool:
    return len(classes) == 1 and classes[0] in D2_SHAPE_CLASSES


def converted_d2_blocks() -> list[tuple[str, list[str]]]:
    """The `[d2]` blocks in System and Station that include the shared style, by `file:line`."""
    files = [
        *sorted((REPO / "docs" / "system").rglob("*.adoc")),
        *sorted((REPO / "docs" / "station").rglob("*.adoc")),
    ]
    return [
        (f"{path.relative_to(REPO).as_posix()}:{number}", source)
        for path in files
        for number, source in d2_blocks(path)
        if any(line.strip() == D2_STYLE_INCLUDE for line in source)
    ]


def test_converted_d2_diagrams_exist() -> None:
    assert converted_d2_blocks(), "no D2 block includes the shared style"


def test_d2_edges_carry_a_class() -> None:
    found = [
        f"{where}: {edge}"
        for where, source in converted_d2_blocks()
        for edge in d2_edges(source)
        if not d2_edge_has_a_kind(edge)
    ]
    assert found == [], f"D2 edges without exactly one edge kind class (optionally plus key): {found}"


def test_d2_diagrams_have_at_most_one_key_edge() -> None:
    found = [
        f"{where}: {keys} key edges"
        for where, source in converted_d2_blocks()
        if (keys := sum(1 for edge in d2_edges(source) if d2_edge_is_key(edge))) > 1
    ]
    assert found == [], f"D2 diagrams with more than one key edge: {found}"


def test_d2_shapes_carry_a_shape_class() -> None:
    found = [
        f"{where}: {key} {classes}"
        for where, source in converted_d2_blocks()
        for key, classes in d2_shapes(source)
        if not d2_shape_has_a_class(classes)
    ]
    assert found == [], f"D2 shapes and containers without exactly one shape class: {found}"


@pytest.mark.parametrize("page", ["hardware.adoc", "power.adoc"])
def test_detailed_diagrams_give_every_part_a_part_class(page: str) -> None:
    """A detailed diagram colours the outline of every part, so only its frames keep a plain class."""
    found = [
        key
        for _, source in d2_blocks(SYSTEM_PAGES / page)
        for key, classes in d2_shapes(source)
        if classes == ["block"]
    ]
    assert found == [], f"{page}: parts with the plain block class: {found}"


def test_d2_shapes_reader() -> None:
    source = [
        "include::system:ROOT:partial$d2-style.d2[]",
        "vars: {",
        "  d2-config: {",
        "    layout-engine: tala",
        "  }",
        "}",
        "direction: right",
        "shape: sequence_diagram",
        'a: "A # not a comment {" {class: block}',
        'b: "B" {class: data-part}',
        "frame: Frame {",
        "  class: boundary",
        "  inner: Inner {class: power-part}",
        "  bare: Bare",
        "  inner -> bare: x {class: power}",
        "}",
        "open: Open {",
        "  nested: Nested {class: block}",
        "}",
        "a.note: A note {class: boundary}",
        "a -> b: y {",
        "  class: data",
        "}",
        "c: C {class: fault-part}",
    ]
    assert d2_shapes(source) == [
        ("a", ["block"]),
        ("b", ["data-part"]),
        ("inner", ["power-part"]),
        ("bare", []),
        ("frame", ["boundary"]),
        ("nested", ["block"]),
        ("open", []),
        ("a.note", ["boundary"]),
        ("c", ["fault-part"]),
    ]
    assert [d2_shape_has_a_class(classes) for _, classes in d2_shapes(source)] == [
        True,
        True,
        True,
        False,
        True,
        True,
        False,
        True,
        False,
    ]


def test_d2_edges_see_the_class_after_a_hash_in_a_quoted_label() -> None:
    edges = d2_edges(['a -> b: "C# sharp" {class: normal}', "c -> d: has no class"])
    assert [edge for edge in edges if not d2_edge_has_a_kind(edge)] == ["c -> d: has no class"]


def test_d2_edges_see_the_class_after_a_brace_in_a_quoted_label() -> None:
    edges = d2_edges(['a -> b: "x {" {class: normal}', "c -> d: has no class"])
    assert [edge for edge in edges if not d2_edge_has_a_kind(edge)] == ["c -> d: has no class"]


def test_d2_edges_handle_escaped_quotes_and_apostrophes() -> None:
    edges = d2_edges(
        [
            r'a -> b: "says \"C# {\" twice" {class: normal}',
            "c -> d: reads the Pod's status {class: normal}",
            "e -> f: 'x {' {class: [fluid; key]}",
            "g -> h: has no class",
        ]
    )
    assert [edge for edge in edges if not d2_edge_has_a_kind(edge)] == ["g -> h: has no class"]


def test_d2_edge_kind_rule() -> None:
    assert d2_edge_has_a_kind("a -> b: x {class: normal}")
    assert d2_edge_has_a_kind("a -> b: x {class: power}")
    assert d2_edge_has_a_kind("a -> b: x {class: [data; key]}")
    assert d2_edge_has_a_kind("a -> b: x { class: [ fault ; key ] }")
    assert not d2_edge_has_a_kind("a -> b: x {class: [key; fault]}")
    assert not d2_edge_has_a_kind("a -> b: x {class: key}")
    assert not d2_edge_has_a_kind("a -> b: x {class: [data; fluid]}")
    assert not d2_edge_has_a_kind("a -> b: x {class: [data; block]}")
    assert not d2_edge_has_a_kind("a -> b: x {class: [data; key; key]}")
    assert not d2_edge_has_a_kind("a -> b: x {class: data; class: key}")
    assert not d2_edge_has_a_kind("a -> b: x {class: block}")
    assert not d2_edge_has_a_kind("a -> b: x")


def test_d2_edge_is_key_sees_a_key_in_a_class_array() -> None:
    assert d2_edge_is_key("a -> b: x {class: [power; key]}")
    assert not d2_edge_is_key("a -> b: x {class: power}")
    assert not d2_edge_is_key("a -> b: keyboard {class: data}")


D2_STYLE = REPO / "docs" / "system" / "modules" / "ROOT" / "partials" / "d2-style.d2"
DIAGRAM_CONVENTIONS = SYSTEM_PAGES / "diagram-conventions.adoc"


def d2_style_classes() -> dict[str, str]:
    """The classes of the generated shared style, each with its body."""
    style = D2_STYLE.read_text(encoding="utf-8").split("\nclasses: {\n", 1)[1]
    return dict(re.findall(r"^  ([\w-]+): \{\n(.*?)^  \}", style, re.MULTILINE | re.DOTALL))


def test_diagram_conventions_page_exists() -> None:
    assert DIAGRAM_CONVENTIONS.is_file()
    assert first_line(DIAGRAM_CONVENTIONS) == "= Diagram Conventions"


def test_diagram_conventions_are_nested_under_chapter_1() -> None:
    nav = (SYSTEM_PAGES.parent / "nav.adoc").read_text(encoding="utf-8").splitlines()
    start = nav.index("* xref:index.adoc[]")
    assert nav[start + 1 : start + 3] == ["** xref:quality-goals.adoc[]", "** xref:diagram-conventions.adoc[]"]


def test_diagram_conventions_list_every_class_of_the_shared_style() -> None:
    page = DIAGRAM_CONVENTIONS.read_text(encoding="utf-8")
    legend = [line for _, source in d2_blocks(DIAGRAM_CONVENTIONS) for line in source]
    used = {name for edge in d2_edges(legend) for classes in d2_edge_classes(edge) for name in classes}
    used |= set(re.findall(r"\bclass: ([\w-]+)", "\n".join(legend)))
    classes = d2_style_classes()
    assert {"actor", "block", "boundary", "external", "normal", "key", *D2_EDGE_KINDS} <= set(classes)
    assert [name for name in classes if f"| `{name}`" not in page] == [], "classes missing from the tables"
    assert sorted(set(classes) - used) == [], "classes missing from the legend diagram"


# The word of the conventions tables for each `stroke-dash` of the shared style.
D2_DASH_WORDS = {None: "solid", "4": "dashed", "2": "short dashes"}


def conventions_rows() -> dict[str, str]:
    """The rows of the class tables of the Diagram Conventions page, by class name."""
    rows = {}
    for row in re.split(r"\n\s*\n", DIAGRAM_CONVENTIONS.read_text(encoding="utf-8")):
        name = re.search(r"^\| `([\w-]+)`$", row, re.MULTILINE)
        if name and row.startswith("| [."):
            rows[name.group(1)] = row
    return rows


def test_diagram_conventions_rows_match_the_shared_style() -> None:
    """Each row names the outline or stroke colour of its class and, for an edge, its line style."""
    rows = conventions_rows()
    problems = []
    for name, body in d2_style_classes().items():
        row = rows.get(name, "")
        stroke = re.search(r'stroke: "(#[0-9a-f]{6})"', body)
        dash = re.search(r"stroke-dash: (\d+)", body)
        width = re.search(r"stroke-width: (\d+)", body)
        wanted = [f"`{stroke.group(1)}`"] if stroke else []
        if "fill:" in body:
            wanted += ["dashed"] if dash else []
        elif stroke:
            wanted.append(f"| {D2_DASH_WORDS[dash and dash.group(1)]}, width {width.group(1)}")
        else:
            wanted.append(f"width {width.group(1)}")
        problems += [f"{name}: {text}" for text in wanted if text not in row]
    assert problems == [], f"rows that do not match d2-style.d2: {problems}"


def test_conventions_rows_reader() -> None:
    rows = conventions_rows()
    assert "`#d0654a`" in rows["power"] and "`#d0654a`" in rows["power-part"]
    assert "`#86aebd`" not in rows["power"]


SITE_EXTRA_CSS = REPO / "docs" / "ui" / "supplemental" / "css" / "site-extra.css"


def css_rules_for(css: str, name: str) -> list[str]:
    """The bodies of the CSS rules whose selector names the class `name`."""
    css = re.sub(r"/\*.*?\*/", "", css, flags=re.DOTALL)
    return [
        body
        for selectors, body in re.findall(r"([^{}]+)\{([^{}]*)\}", css)
        if re.search(rf"\.{name}(?![\w-])", selectors)
    ]


def test_diagram_conventions_samples_match_the_shared_style() -> None:
    """Each class of d2-style.d2 has a sample rule in site-extra.css that draws in the same colours."""
    css = SITE_EXTRA_CSS.read_text(encoding="utf-8")
    problems = []
    for name, body in d2_style_classes().items():
        kind = "shape" if "fill:" in body else "edge"
        rules = css_rules_for(css, f"{kind}-{name}")
        if not rules:
            problems.append(f"no .{kind}-{name} rule")
            continue
        drawn = {
            PALETTE[variable]
            for rule in [*rules, *css_rules_for(css, f"{kind}-sample")]
            for variable in re.findall(r"var\(--cm-([a-z0-9-]+)\)", rule)
        }
        wanted = set(re.findall(r'(?:fill|stroke): "(#[0-9a-f]{6})"', body))
        if wanted - drawn:
            problems.append(f".{kind}-{name} does not draw in {sorted(wanted - drawn)}")
    assert problems == [], problems


def test_system_start_page_links_the_diagram_conventions() -> None:
    assert "xref:diagram-conventions.adoc[" in (SYSTEM_PAGES / "index.adoc").read_text(encoding="utf-8")


def test_d2_edges_skip_arrows_and_classes_in_quoted_labels() -> None:
    source = ['smps: "SMPS\\n230 V AC -> 24 V DC" {class: block}', 'a -> b: "{class: key}"']
    assert d2_edges(source) == ['a -> b: ""']


def d2_blocks_without_the_style(component: str) -> list[str]:
    """The `[d2]` blocks of a component that do not start with the shared style include, by `file:line`."""
    return [
        f"{path.relative_to(REPO).as_posix()}:{number}"
        for path in sorted((REPO / "docs" / component).rglob("*.adoc"))
        for number, source in d2_blocks(path)
        if source[:1] != [D2_STYLE_INCLUDE]
    ]


@pytest.mark.parametrize("component", ["system", "station"])
def test_d2_blocks_start_with_the_style_include(component: str) -> None:
    found = d2_blocks_without_the_style(component)
    assert found == [], f"D2 blocks whose first line is not {D2_STYLE_INCLUDE}: {found}"


# The layout engines a diagram may choose. The shared style already sets `tala`, so an override never repeats it.
D2_LAYOUT_ENGINES = frozenset({"elk", "dagre"})
# The one form of a layout engine override, a top-level line after the include.
D2_ENGINE_OVERRIDE = re.compile(r"^vars: \{ d2-config: \{ layout-engine: ([\w-]+) \} \}$")


def d2_engine_problems(source: list[str]) -> list[str]:
    """What is wrong with the layout engine lines of a D2 block, by source line."""
    problems = []
    include = source.index(D2_STYLE_INCLUDE) if D2_STYLE_INCLUDE in source else None
    sequence = any(line.strip() == "shape: sequence_diagram" for line in source)
    for index, line in enumerate(source):
        if "layout-engine" not in line and "d2-config" not in line:
            continue
        override = D2_ENGINE_OVERRIDE.match(line)
        if not override:
            problems.append(f"not the form `vars: {{ d2-config: {{ layout-engine: <engine> }} }}`: {line.strip()}")
        elif override.group(1) not in D2_LAYOUT_ENGINES:
            problems.append(f"unknown engine {override.group(1)}, use one of {sorted(D2_LAYOUT_ENGINES)}")
        if include is None or index < include:
            problems.append(f"before the style include, so the style overrides it: {line.strip()}")
        if sequence:
            problems.append(f"in a sequence diagram, which ignores the engine: {line.strip()}")
    return problems


@pytest.mark.parametrize("component", ["system", "station"])
def test_d2_layout_engine_overrides(component: str) -> None:
    found = [
        f"{path.relative_to(REPO).as_posix()}:{number}: {problem}"
        for path in sorted((REPO / "docs" / component).rglob("*.adoc"))
        for number, source in d2_blocks(path)
        for problem in d2_engine_problems(source)
    ]
    assert found == [], "D2 layout engine overrides that break the rules:\n" + "\n".join(found)


def test_d2_engine_problems_reader() -> None:
    elk = "vars: { d2-config: { layout-engine: elk } }"
    assert d2_engine_problems([D2_STYLE_INCLUDE, elk, "direction: right"]) == []
    assert d2_engine_problems([D2_STYLE_INCLUDE, "vars: { d2-config: { layout-engine: dagre } }"]) == []
    assert d2_engine_problems([D2_STYLE_INCLUDE, "a: A {class: block}"]) == []
    assert d2_engine_problems([elk, D2_STYLE_INCLUDE]) == [
        f"before the style include, so the style overrides it: {elk}"
    ]
    assert d2_engine_problems([D2_STYLE_INCLUDE, "vars: { d2-config: { layout-engine: grid } }"]) == [
        "unknown engine grid, use one of ['dagre', 'elk']"
    ]
    assert d2_engine_problems([D2_STYLE_INCLUDE, "vars: { d2-config: { layout-engine: tala } }"]) == [
        "unknown engine tala, use one of ['dagre', 'elk']"
    ]
    assert d2_engine_problems([D2_STYLE_INCLUDE, "shape: sequence_diagram", elk]) == [
        f"in a sequence diagram, which ignores the engine: {elk}"
    ]
    assert d2_engine_problems([D2_STYLE_INCLUDE, "vars: {", "  d2-config: {", "    layout-engine: elk"]) == [
        "not the form `vars: { d2-config: { layout-engine: <engine> } }`: d2-config: {",
        "not the form `vars: { d2-config: { layout-engine: <engine> } }`: layout-engine: elk",
    ]


OLD_DIAGRAM_BLOCK = re.compile(r"^\[(?:plantuml|mermaid)\b")


def assert_diagrams_are_d2(component: str, name: str) -> None:
    """No PlantUML or Mermaid block is left in a component, and every diagram of its built pages is a D2 SVG.

    The source tests check that each block starts with the shared style, and the palette test checks that it applied.
    """
    files = sorted((REPO / "docs" / component).rglob("*.adoc"))
    old = [
        f"{path.relative_to(REPO).as_posix()}:{number}"
        for path in files
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1)
        if OLD_DIAGRAM_BLOCK.match(line)
    ]
    assert old == [], f"PlantUML or Mermaid blocks left in {name}: {old}"
    diagrams = fetched_diagrams(component)
    assert len(diagrams) == sum(len(d2_blocks(path)) for path in files), f"not every {name} D2 block is in the site"
    not_d2 = [path.name for path in diagrams if D2_SVG_MARKER not in path.read_text(encoding="utf-8")]
    assert not_d2 == [], f"{name} diagrams that are not D2: {not_d2}"


@site_test
def test_system_diagrams_are_d2() -> None:
    assert_diagrams_are_d2("system", "System")


@site_test
def test_station_diagrams_are_d2() -> None:
    assert_diagrams_are_d2("station", "Station")


# A diagram that asciidoctor-kroki fetched at build time: `<div class="imageblock kroki">` with an `<img>` in it.
# The `<img>` may sit in the `<a>` that opens the diagram at full size.
FETCHED_DIAGRAM_IMG = re.compile(
    r'<div class="imageblock kroki[^"]*">\s*<div class="content">\s*(?:<a [^>]*>\s*)?<img src="([^"]*)"'
)

# A diagram block with its full-size link, as the link target and the image source.
DIAGRAM_LINK = re.compile(
    r'<div class="imageblock kroki[^"]*">\s*<div class="content">\s*(?:<a class="image" href="([^"]*)"[^>]*>\s*)?'
    r'<img src="([^"]*)"'
)


def built_page(component: str, adoc: Path) -> Path:
    """The built page of an AsciiDoc page, `docs/<component>/modules/<module>/pages/<page>.adoc`."""
    module, _, page = adoc.relative_to(REPO / "docs" / component / "modules").as_posix().partition("/pages/")
    folder = "" if module == "ROOT" else f"{module}/"
    return SITE_DIR / component / f"{folder}{page.removesuffix('.adoc')}.html"


@site_test
@pytest.mark.parametrize("component", ["system", "station"])
def test_fetched_diagrams_are_local_svg_files(component: str) -> None:
    """With `kroki-fetch-diagram`, every D2 block is a local SVG file in its page and no page points to kroki.io."""
    remote = [
        page.relative_to(SITE_DIR).as_posix()
        for page in sorted(SITE_DIR.rglob("*.html"))
        if "kroki.io" in page.read_text(encoding="utf-8")
    ]
    assert remote == [], f"built pages that still reference kroki.io: {remote}"
    total = 0
    for adoc in sorted((REPO / "docs" / component).rglob("*.adoc")):
        expected = len(d2_blocks(adoc))
        page = built_page(component, adoc)
        sources = FETCHED_DIAGRAM_IMG.findall(page.read_text(encoding="utf-8")) if page.exists() else []
        assert len(sources) == expected, (
            f"{page.relative_to(SITE_DIR).as_posix()}: {len(sources)} diagram images for {expected} D2 blocks"
        )
        for source in sources:
            assert source.endswith(".svg") and "://" not in source, f"{page}: not a local SVG file: {source}"
            assert (page.parent / source).is_file(), f"{page}: missing file {source}"
        total += expected
    assert total > 0, f"no D2 block in {component}"


def fetched_diagrams(component: str) -> list[Path]:
    """The SVG files of the fetched diagrams that the built pages of `component` show, in page order."""
    files = []
    for adoc in sorted((REPO / "docs" / component).rglob("*.adoc")):
        page = built_page(component, adoc)
        if page.exists():
            files += [page.parent / source for source in FETCHED_DIAGRAM_IMG.findall(page.read_text(encoding="utf-8"))]
    return files


# D2 writes its version on the root `<svg>` of every diagram.
D2_SVG_MARKER = 'data-d2-version="'

D2_BLEND_EXTENSION = REPO / "docs" / "ui" / "extensions" / "d2-blend.js"

# The rule that docs/ui/extensions/d2-blend.js puts in place of the D2 rule `mix-blend-mode: multiply; opacity: 0.5`.
D2_VISIBLE_BLEND_RULE = ".blend {\n  mix-blend-mode: lighten;\n}"

D2_BLEND_GROUP = re.compile(r'class="[^"]*\bblend\b[^"]*"')


@site_test
def test_sequence_diagram_groups_are_visible() -> None:
    """The site build rewrote the blend rule of every D2 SVG, so the group frames show on the ink background."""
    diagrams = [path for path in sorted(SITE_DIR.rglob("*.svg")) if D2_SVG_MARKER in path.read_text(encoding="utf-8")]
    assert diagrams, "no D2 diagram in the site"
    multiply = [path.name for path in diagrams if "mix-blend-mode: multiply" in path.read_text(encoding="utf-8")]
    assert multiply == [], f"D2 diagrams that still blend with multiply: {multiply}"
    groups = [path for path in diagrams if D2_BLEND_GROUP.search(path.read_text(encoding="utf-8"))]
    assert groups, "no D2 diagram with a sequence diagram group in the site"
    unchanged = [path.name for path in groups if D2_VISIBLE_BLEND_RULE not in path.read_text(encoding="utf-8")]
    assert unchanged == [], f"D2 diagrams with a group but without the lighten rule: {unchanged}"


def test_d2_blend_extension_and_this_test_agree_on_the_rule() -> None:
    assert f"const VISIBLE_RULE = {D2_VISIBLE_BLEND_RULE!r}" in D2_BLEND_EXTENSION.read_text(encoding="utf-8")


@pytest.mark.skipif(shutil.which("node") is None, reason="needs Node.js")
def test_d2_blend_extension_unit_tests() -> None:
    result = subprocess.run(
        ["node", "--test", str(D2_BLEND_EXTENSION.with_name("d2-blend.test.js"))],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr


PALETTE_CSS = REPO / "docs" / "ui" / "supplemental" / "css" / "palette.css"

# The generated palette: the name after `--cm-` and its hex value.
PALETTE = dict(re.findall(r"--cm-([a-z0-9-]+):\s*(#[0-9a-f]{6})\s*;", PALETTE_CSS.read_text(encoding="utf-8")))

# D2 cuts the edge labels out of the edges with a `<mask>`, drawn in white, black and black with some opacity.
# A mask is never visible, so these colours are allowed inside one and nowhere else.
MASK_COLOURS = re.compile(r"white|black|rgba\(0,0,0,[0-9.]+\)")

SVG_TAG = re.compile(r"<(/?)([a-zA-Z][a-zA-Z0-9]*)\b([^>]*)>")
SVG_COLOUR = re.compile(r'\b(fill|stroke)="([^"]*)"')


def off_palette_colours(svg: str, palette: dict[str, str]) -> list[str]:
    """The `fill` and `stroke` values of a D2 SVG that are not palette colours, in document order.

    `none` is allowed everywhere, and the mask colours only inside a `<mask>`.
    The first `<rect>` is the background of the diagram, so it must have the `ink` fill.
    """
    allowed = set(palette.values()) | {"none"}
    svg = re.sub(r"<style\b.*?</style>", "", svg, flags=re.DOTALL)
    found = []
    masks = 0
    first_rect = True
    for match in SVG_TAG.finditer(svg):
        closing, tag, attrs = match.groups()
        if tag == "mask":
            masks += -1 if closing else 0 if attrs.rstrip().endswith("/") else 1
            continue
        if closing:
            continue
        colours = SVG_COLOUR.findall(attrs)
        if tag == "rect" and first_rect:
            first_rect = False
            fill = dict(colours).get("fill", "").lower()
            if fill != palette["ink"]:
                found.append(f'the first rect has fill="{fill}", not the ink colour {palette["ink"]}')
                colours = [colour for colour in colours if colour[0] != "fill"]
        for name, value in colours:
            value = value.lower()
            if value not in allowed and not (masks and MASK_COLOURS.fullmatch(value)):
                found.append(f'{name}="{value}"')
    return found


@site_test
@pytest.mark.parametrize("component", ["system", "station"])
def test_fetched_diagrams_use_only_palette_colours(component: str) -> None:
    """Every diagram draws in the shared style, so a shape without a class shows up in the D2 default colours."""
    files = fetched_diagrams(component)
    assert files, f"no fetched diagram in {component}"
    found = [
        f"{path.relative_to(SITE_DIR).as_posix()}: {', '.join(sorted(set(problems)))}"
        for path in files
        if (problems := off_palette_colours(path.read_text(encoding="utf-8"), PALETTE))
    ]
    assert found == [], "diagrams with colours outside the palette:\n" + "\n".join(found)


@site_test
@pytest.mark.parametrize("component", ["system", "station"])
def test_diagrams_open_at_full_size(component: str) -> None:
    """A click on a diagram opens its image on its own, because labels shrink when the page scales a wide diagram."""
    found = []
    total = 0
    for adoc in sorted((REPO / "docs" / component).rglob("*.adoc")):
        page = built_page(component, adoc)
        if not page.exists():
            continue
        for href, src in DIAGRAM_LINK.findall(page.read_text(encoding="utf-8")):
            total += 1
            if href.replace("&amp;", "&") != src.replace("&amp;", "&"):
                found.append(f"{page.relative_to(SITE_DIR).as_posix()}: {src[:80]} links to {href[:80] or 'nothing'}")
    assert total > 0, f"no diagram in {component}"
    assert found == [], "diagrams without a link to their own image:\n" + "\n".join(found)


# The widest diagram that stays legible in the content column, in SVG pixels.
# The column is about 800 px wide, so a 1400 px diagram shows its labels at a bit more than half size.
MAX_DIAGRAM_WIDTH = 1400

# Diagrams that cannot fit `MAX_DIAGRAM_WIDTH` without a change of content, by `target`, with the reason.
WIDE_DIAGRAMS: dict[str, str] = {}

SVG_VIEWBOX = re.compile(r'<svg\b[^>]*\bviewBox="[-0-9.]+ [-0-9.]+ ([0-9.]+) [0-9.]+"')


def diagram_target(path: Path) -> str:
    """The `target` of a fetched diagram file, `<target>-<hash>.svg`."""
    return re.sub(r"-[0-9a-f]{40}$", "", path.stem)


@site_test
@pytest.mark.parametrize("component", ["system", "station"])
def test_fetched_diagrams_fit_the_page(component: str) -> None:
    files = fetched_diagrams(component)
    assert files, f"no fetched diagram in {component}"
    found = []
    for path in files:
        match = SVG_VIEWBOX.search(path.read_text(encoding="utf-8"))
        assert match, f"{path.name}: no viewBox on the root svg"
        width = float(match.group(1))
        if width > MAX_DIAGRAM_WIDTH and diagram_target(path) not in WIDE_DIAGRAMS:
            found.append(f"{diagram_target(path)}: {width:g} px")
    assert found == [], f"diagrams wider than {MAX_DIAGRAM_WIDTH} px:\n" + "\n".join(found)


def test_off_palette_colours_accepts_palette_colours_and_mask_cut_outs() -> None:
    svg = (
        '<svg><svg class="d2-svg"><rect fill="#0a0806" stroke-width="0" />'
        '<rect fill="#1C1510" stroke="#c8913a" /><path fill="none" stroke="#867f6b" />'
        '<mask id="m"><rect fill="white"></rect><rect fill="black"></rect><rect fill="rgba(0,0,0,0.75)"></rect></mask>'
        "</svg></svg>"
    )
    assert off_palette_colours(svg, PALETTE) == []


def test_off_palette_colours_finds_an_unclassed_shape() -> None:
    svg = '<svg><rect fill="#0a0806" /><rect fill="#F7F8FE" stroke="#0D32B2" /><text fill="#0A0F25">b</text></svg>'
    assert off_palette_colours(svg, PALETTE) == ['fill="#f7f8fe"', 'stroke="#0d32b2"', 'fill="#0a0f25"']


def test_off_palette_colours_finds_mask_colours_outside_a_mask() -> None:
    svg = '<svg><rect fill="#0a0806" /><mask><rect fill="white" /></mask><rect fill="white" /></svg>'
    assert off_palette_colours(svg, PALETTE) == ['fill="white"']


def test_off_palette_colours_needs_an_ink_background() -> None:
    svg = '<svg><rect fill="#1c1510" /><rect fill="#0a0806" /></svg>'
    assert off_palette_colours(svg, PALETTE) == ['the first rect has fill="#1c1510", not the ink colour #0a0806']


SYSTEM_GLOSSARY = SYSTEM_PAGES.parent / "partials" / "glossary.tsv"

PLUGIN_GLOSSARY = REPO / "plugins" / "cocktail-maker-docs" / "skills" / "writing-style" / "glossary-system.tsv"

GLOSSARY_COLUMNS = ["term", "definition", "level", "code", "avoid"]


def glossary_problems(system: list[GlossaryRow], others: dict[str, list[GlossaryRow]]) -> list[str]:
    """Report glossary rows that break the rules: avoided synonyms, redefined System terms and duplicates."""
    problems: list[str] = []
    glossaries = {"System": system, **others}
    terms = {row.term.lower(): name for name, rows in glossaries.items() for row in rows}
    seen: dict[str, str] = {}
    for name, rows in glossaries.items():
        for row in rows:
            key = row.term.lower()
            if key in seen:
                problems.append(f"{name}: duplicate term '{row.term}' (also in {seen[key]})")
            seen.setdefault(key, name)
    for name, rows in glossaries.items():
        for row in rows:
            for synonym in row.avoid:
                if synonym.lower() in terms:
                    problems.append(
                        f"{name}: '{synonym}' is avoided for '{row.term}' but is a term in {terms[synonym.lower()]}"
                    )
    owners: dict[str, str] = {}
    for name, rows in glossaries.items():
        for row in rows:
            for synonym in row.avoid:
                owner = owners.setdefault(synonym.lower(), row.term)
                if owner != row.term:
                    problems.append(f"{name}: '{synonym}' is avoided for both '{owner}' and '{row.term}'")
    for name, rows in glossaries.items():
        for row in rows:
            for synonym in row.avoid:
                for other_name, other_rows in glossaries.items():
                    for other in other_rows:
                        if other is not row and re.search(rf"\b{re.escape(synonym)}\b", other.term, re.IGNORECASE):
                            problems.append(
                                f"{name}: '{synonym}' is avoided for '{row.term}' but is a word of the term "
                                f"'{other.term}' in {other_name}"
                            )
    system_avoided = {synonym.lower() for row in system for synonym in row.avoid}
    system_terms = {row.term.lower() for row in system}
    for name, rows in others.items():
        for row in rows:
            if row.term.lower() in system_terms:
                problems.append(f"{name}: '{row.term}' redefines a System term")
            elif row.term.lower() in system_avoided:
                problems.append(f"{name}: '{row.term}' is a System synonym")
    return problems


def test_glossary_problems_reader() -> None:
    system = [GlossaryRow("Pod", "d", "System", "", ("module", "unit"))]
    assert glossary_problems(system, {"station": [GlossaryRow("Recipe", "d", "Station", "", ("cocktail",))]}) == []
    overlap = glossary_problems(system, {"station": [GlossaryRow("Unit", "d", "Station", "", ())]})
    assert any("'Unit' is a System synonym" in p for p in overlap)
    assert any("'unit' is avoided for 'Pod'" in p for p in overlap)
    redefined = glossary_problems(system, {"station": [GlossaryRow("pod", "d", "Station", "", ())]})
    assert any("redefines a System term" in p for p in redefined)
    assert any("duplicate term" in p for p in redefined)
    duplicate = glossary_problems(
        [GlossaryRow("Pod", "d", "System", "", ()), GlossaryRow("Pod", "e", "System", "", ())], {}
    )
    assert duplicate == ["System: duplicate term 'Pod' (also in System)"]
    own = glossary_problems([GlossaryRow("Pod", "d", "System", "", ("pod",))], {})
    assert len(own) == 1
    shared = glossary_problems(
        [GlossaryRow("Pod", "d", "System", "", ("unit",)), GlossaryRow("Station", "d", "System", "", ("Unit",))], {}
    )
    assert shared == ["System: 'Unit' is avoided for both 'Pod' and 'Station'"]
    inside = glossary_problems(
        [GlossaryRow("Pod", "d", "System", "", ("unit",)), GlossaryRow("Power Management Unit", "d", "System", "", ())],
        {},
    )
    assert inside == ["System: 'unit' is avoided for 'Pod' but is a word of the term 'Power Management Unit' in System"]


def test_plugin_glossary_copy_is_in_sync() -> None:
    assert PLUGIN_GLOSSARY.read_bytes() == SYSTEM_GLOSSARY.read_bytes()


def test_glossaries_are_consistent() -> None:
    others = {
        path.parents[3].name: load_glossary(path)
        for path in sorted((REPO / "docs").glob("*/modules/ROOT/partials/glossary.tsv"))
        if path != SYSTEM_GLOSSARY
    }
    assert glossary_problems(load_glossary(SYSTEM_GLOSSARY), others) == []


class _TableHeaderParser(HTMLParser):
    """Collects the header cell texts of every table."""

    def __init__(self) -> None:
        super().__init__()
        self.headers: list[str] = []
        self._in_th = False

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        self._in_th = tag == "th"

    def handle_endtag(self, tag: str) -> None:
        self._in_th = False

    def handle_data(self, data: str) -> None:
        if self._in_th:
            self.headers.append(data.strip())


@site_test
def test_glossary_page_renders_the_table() -> None:
    parser = _TableHeaderParser()
    parser.feed((SITE_DIR / "system" / "glossary.html").read_text(encoding="utf-8"))
    parser.close()
    assert parser.headers == GLOSSARY_COLUMNS


ID_PAGES = {
    "QG": "quality-goals.adoc",
    "QS": "quality-requirements.adoc",
    "SM": "safety.adoc",
    "IF": "contracts.adoc",
}
QUALITY_GOALS = ["Human Safety", "Food Safety", "Reliability", "Serviceability", "Usability", "Maintainability"]


def id_anchors(path: Path, prefix: str) -> list[str]:
    """Every `[[<prefix>-...]]` anchor of a page, well-formed or not, in document order."""
    if not path.exists():
        return []
    anchors = re.findall(rf"\[\[({prefix}-[^\]]*)\]\]", path.read_text(encoding="utf-8"))
    return [anchor for anchor in anchors if not re.fullmatch(rf"{prefix}-\d\d-R\d\d", anchor)]


def defined_ids(prefix: str) -> list[str]:
    """The IDs of one prefix, as anchors on the page that defines them. Empty while that page has none."""
    return id_anchors(SYSTEM_PAGES / ID_PAGES[prefix], prefix)


def test_quality_goal_ids() -> None:
    assert defined_ids("QG") == [f"QG-{n:02d}" for n in range(1, len(QUALITY_GOALS) + 1)]
    rows = (SYSTEM_PAGES / ID_PAGES["QG"]).read_text(encoding="utf-8").split("\n| [[")[1:]
    for goal_id, name, row in zip(defined_ids("QG"), QUALITY_GOALS, rows, strict=True):
        assert row.startswith(f"{goal_id}]]{goal_id}\n")
        assert f"*{name}*" in row


def test_ids_are_defined_only_on_their_page() -> None:
    for prefix, page in ID_PAGES.items():
        for other in sorted((REPO / "docs").glob("*/modules/*/pages/*.adoc")):
            if other == SYSTEM_PAGES / page:
                continue
            assert id_anchors(other, prefix) == [], f"{other} defines a {prefix} anchor"


@pytest.mark.parametrize("prefix", sorted(ID_PAGES))
def test_ids_are_unique_and_well_formed(prefix: str) -> None:
    ids = defined_ids(prefix)
    assert all(re.fullmatch(rf"{prefix}-\d{{2}}", i) for i in ids), ids
    assert len(ids) == len(set(ids)), ids


@site_test
def test_quality_goal_anchor_resolves() -> None:
    html = (SITE_DIR / "system" / "quality-goals.html").read_text(encoding="utf-8")
    for goal_id in defined_ids("QG"):
        assert f'id="{goal_id}"' in html
    assert 'id="QG-01"' in html


def adr_goal_problems(text: str) -> list[str]:
    """What is wrong with the `== Quality Goals` section of one ADR page, as readable messages."""
    lines = text.splitlines()
    headings = [(i, line) for i, line in enumerate(lines) if re.match(r"==+ ", line)]
    titles = [line for _, line in headings]
    if "== Quality Goals" not in titles:
        return ["missing section '== Quality Goals'"]
    problems: list[str] = []
    start = titles.index("== Quality Goals")
    before = titles[:start]
    if "== Context" not in before:
        problems.append("'== Quality Goals' does not come after '== Context'")
    else:
        between = before[before.index("== Context") + 1 :]
        if any(title != "=== Decision Drivers" for title in between):
            problems.append("'== Quality Goals' is not directly after '== Context'")
    end = headings[start + 1][0] if start + 1 < len(headings) else len(lines)
    section = "\n".join(lines[headings[start][0] + 1 : end])
    refs = re.findall(r"\b(?:QG|QS)-\d+\b", section)
    if not any(ref.startswith("QG-") for ref in refs):
        problems.append("'== Quality Goals' has no QG-nn reference")
    defined = set(defined_ids("QG")) | set(defined_ids("QS"))
    problems += [f"{ref} is not a defined ID" for ref in dict.fromkeys(refs) if ref not in defined]
    for anchor, label in re.findall(r"xref:\S*?#(QG-\d+)\[([^\]]*)\]", text):
        if anchor != label:
            problems.append(f"link to {anchor} has the text '{label}'")
    effect = [i for i, title in enumerate(titles) if title == "=== Effect on the Quality Goals"]
    effect_text = ""
    if effect:
        effect_start = headings[effect[0]][0]
        effect_end = headings[effect[0] + 1][0] if effect[0] + 1 < len(headings) else len(lines)
        effect_text = "\n".join(lines[effect_start + 1 : effect_end])
    assessed = {m for line in effect_text.splitlines() for m in re.findall(r"\bQG-\d+\b", line)}
    problems += [
        f"{ref} has no line in '=== Effect on the Quality Goals'"
        for ref in dict.fromkeys(refs)
        if ref.startswith("QG-") and ref in defined and ref not in assessed
    ]
    return problems


_ADR_GOOD = """= SYS-001: X

== Context

Text.

=== Decision Drivers

* **A:** QG-01.

== Quality Goals

* xref:system::quality-goals.adoc#QG-01[QG-01]: how.

== Decision

=== Effect on the Quality Goals

* xref:system::quality-goals.adoc#QG-01[QG-01]: Better.
"""


ALL_ADR_PAGES = sorted(
    [
        *(REPO / "docs/system/modules/adr/pages").glob("*.adoc"),
        *(REPO / "docs/station/modules/adr/pages").glob("*.adoc"),
    ]
)


@pytest.mark.parametrize("adr", ALL_ADR_PAGES, ids=lambda p: f"{p.parts[-5]}-{p.name}")
def test_adrs_name_their_quality_goals(adr: Path) -> None:
    assert adr_goal_problems(adr.read_text(encoding="utf-8")) == []


def test_adr_goal_problems_reader() -> None:
    assert adr_goal_problems(_ADR_GOOD) == []
    no_section = _ADR_GOOD.replace("== Quality Goals", "== Other")
    assert adr_goal_problems(no_section) == ["missing section '== Quality Goals'"]
    moved = _ADR_GOOD.replace("=== Decision Drivers", "== Alternatives Considered")
    assert adr_goal_problems(moved) == ["'== Quality Goals' is not directly after '== Context'"]
    before_context = "== Quality Goals\n\n* QG-01.\n\n== Context\n\nText.\n"
    assert "'== Quality Goals' does not come after '== Context'" in adr_goal_problems(before_context)
    no_qg = _ADR_GOOD.replace("QG-01]: how", "x]: how").replace("#QG-01[", "#x[")
    assert adr_goal_problems(no_qg) == ["'== Quality Goals' has no QG-nn reference"]
    unknown = _ADR_GOOD.replace("QG-01]: how.", "QG-01]: how.\n* QG-09: how.")
    assert adr_goal_problems(unknown) == ["QG-09 is not a defined ID"]
    mismatch = _ADR_GOOD.replace("#QG-01[QG-01]: how", "#QG-02[QG-01]: how")
    assert "link to QG-02 has the text 'QG-01'" in adr_goal_problems(mismatch)
    unassessed = _ADR_GOOD.replace("* xref:system::quality-goals.adoc#QG-01[QG-01]: Better.", "* Better.")
    assert adr_goal_problems(unassessed) == ["QG-01 has no line in '=== Effect on the Quality Goals'"]
    no_effect = _ADR_GOOD.split("=== Effect")[0]
    assert adr_goal_problems(no_effect) == ["QG-01 has no line in '=== Effect on the Quality Goals'"]


CONTRACTS_PAGE = SYSTEM_PAGES / "contracts.adoc"

CONTRACT_SECTIONS = (
    "What crosses",
    "Guarantees and limits",
    "Fault behaviour",
    "Requirements",
    "Open questions",
    "Sources",
)
CONTRACT_KINDS = ("data", "power", "fluid", "mechanical")
CONTRACT_STATUSES = ("Decided", "Partly decided", "Not designed yet")


class Requirement(NamedTuple):
    id: str
    text: str
    goals: tuple[str, ...]
    met_by: str


class Contract(NamedTuple):
    id: str
    title: str
    kind: str
    status: str
    sections: tuple[str, ...]
    requirements: tuple[Requirement, ...]
    anchor: str = ""


CONTRACT_ANCHOR = re.compile(r"^\[\[(IF-\d\d)\]\]$", re.MULTILINE)
CONTRACT_HEADING = re.compile(r"^== (IF-\d\d): (.+)$", re.MULTILINE)
XREF = re.compile(r"xref:([^\[\s]+)\[([^\]]*)\]")


def _definition(chunk: str, term: str) -> str:
    match = re.search(rf"^{term}::[ \t]*(.*)$", chunk, re.MULTILINE)
    return match.group(1).strip() if match else ""


def parse_contracts(text: str) -> list[Contract]:
    """The contracts of the Contracts page, split at the lines that are exactly `[[IF-nn]]`."""
    anchors = list(CONTRACT_ANCHOR.finditer(text))
    contracts = []
    for index, anchor in enumerate(anchors):
        end = anchors[index + 1].start() if index + 1 < len(anchors) else len(text)
        chunk = text[anchor.end() : end]
        heading = CONTRACT_HEADING.search(chunk)
        requirements = []
        for line in chunk.splitlines():
            if not line.startswith("| [[IF-"):
                continue
            cells = [cell.strip() for cell in line[1:].split("|")]
            cells += [""] * (4 - len(cells))
            found = re.match(r"\[\[([^\]]*)\]\]", cells[0])
            requirements.append(
                Requirement(
                    found.group(1) if found else cells[0], cells[1], tuple(re.findall(r"QG-\d\d", cells[2])), cells[3]
                )
            )
        contracts.append(
            Contract(
                heading.group(1) if heading else "",
                heading.group(2) if heading else "",
                _definition(chunk, "Kind"),
                _definition(chunk, "Status"),
                tuple(re.findall(r"^=== (.+)$", chunk, re.MULTILINE)),
                tuple(requirements),
                anchor.group(1),
            )
        )
    return contracts


def defined_requirement_ids() -> list[str]:
    """The `IF-nn-Rnn` anchors of the Contracts page, in order."""
    if not CONTRACTS_PAGE.exists():
        return []
    return re.findall(r"\[\[(IF-\d\d-R\d\d)\]\]", CONTRACTS_PAGE.read_text(encoding="utf-8"))


def xref_page_exists(xref: str, component: str = "system") -> bool | None:
    """Whether the page an xref target names exists, `None` when it is in a component that is not checkable here."""
    parts = xref.split("#", 1)[0].split(":")
    page = parts[-1]
    module = "ROOT"
    match parts[:-1]:
        case []:
            pass
        case [name] if name in ("ROOT", "adr"):
            module = name
        case [name, ""] | [name, "ROOT"]:
            component = name
        case [name, other]:
            component, module = name, other
        case _:
            return False
    if component not in ("system", "station"):
        return None
    return (REPO / "docs" / component / "modules" / module / "pages" / page).is_file()


def _met_by_problems(label: str, cell: str) -> list[str]:
    if cell == "open":
        return []
    xrefs = XREF.findall(cell)
    rest = XREF.sub(" ", cell)
    if not xrefs or any(word not in ("", "and") for word in re.split(r"[\s,;]+", rest)):
        return [f"{label}: Met by is not 'open' or a list of xrefs: '{cell}'"]
    return [
        f"{label}: Met by links the missing page {target}" for target, _ in xrefs if xref_page_exists(target) is False
    ]


def contract_problems(text: str) -> list[str]:
    """What is wrong with the contracts of the Contracts page, as readable messages."""
    problems: list[str] = []
    goals = set(defined_ids("QG"))
    seen: set[str] = set()
    for contract in parse_contracts(text):
        label = contract.anchor
        if contract.id != contract.anchor:
            problems.append(f"{label}: the heading names '{contract.id or 'no ID'}'")
        if contract.kind not in CONTRACT_KINDS:
            problems.append(f"{label}: Kind '{contract.kind}' is not one of {list(CONTRACT_KINDS)}")
        if contract.status not in CONTRACT_STATUSES:
            problems.append(f"{label}: Status '{contract.status}' is not one of {list(CONTRACT_STATUSES)}")
        if contract.sections != CONTRACT_SECTIONS:
            problems.append(f"{label}: sections {list(contract.sections)} are not {list(CONTRACT_SECTIONS)}")
        if not contract.requirements:
            problems.append(f"{label}: no requirement row")
        for requirement in contract.requirements:
            name = requirement.id
            if not re.fullmatch(rf"{re.escape(contract.anchor)}-R\d\d", name):
                problems.append(f"{label}: requirement ID '{name}' is not {contract.anchor}-Rnn")
            if name in seen:
                problems.append(f"{label}: duplicate requirement ID '{name}'")
            seen.add(name)
            if not requirement.goals:
                problems.append(f"{name}: no QG-nn goal")
            problems += [f"{name}: {goal} is not a defined ID" for goal in requirement.goals if goal not in goals]
            problems += _met_by_problems(name, requirement.met_by)
    return problems


def _requirement_row(requirement_id: str, goal: str = "QG-02", met_by: str = "open") -> str:
    return f"| [[{requirement_id}]]{requirement_id} | An outcome. | {goal} | {met_by}\n"


def _contract(
    contract_id: str = "IF-05", rows: str | None = None, sections: tuple[str, ...] = CONTRACT_SECTIONS
) -> str:
    rows = _requirement_row(f"{contract_id}-R01") if rows is None else rows
    parts = [
        f"[[{contract_id}]]\n== {contract_id}: Bottle to Pod\n\nSides:: A and B\nKind:: fluid\nStatus:: Decided\n\n"
    ]
    for section in sections:
        parts.append(f"=== {section}\n\n")
        if section == "Requirements":
            parts.append(
                f'[cols="2,6,2,3", options="header"]\n|===\n| ID | Requirement | Goals | Met by\n{rows}|===\n\n'
            )
        else:
            parts.append("Text.\n\n")
    return "".join(parts)


_CONTRACTS_GOOD = "= Contracts\n\nLead.\n\n" + _contract()


def test_contract_problems_reader() -> None:
    assert contract_problems("= Contracts\n\nLead.\n") == []
    assert contract_problems(_CONTRACTS_GOOD) == []
    no_fault = _contract(sections=tuple(s for s in CONTRACT_SECTIONS if s != "Fault behaviour"))
    assert any("sections" in p for p in contract_problems(no_fault))
    swapped = _contract(sections=(*CONTRACT_SECTIONS[1::-1], *CONTRACT_SECTIONS[2:]))
    assert any("sections" in p for p in contract_problems(swapped))
    assert any("Status 'Planned'" in p for p in contract_problems(_CONTRACTS_GOOD.replace("Decided", "Planned")))
    assert any("Kind 'optical'" in p for p in contract_problems(_CONTRACTS_GOOD.replace("fluid", "optical")))
    copied = _contract("IF-06", _requirement_row("IF-05-R01"))
    assert any("IF-05-R01" in p and "IF-06" in p for p in contract_problems(copied))
    twice = _contract("IF-05", _requirement_row("IF-05-R01") + _requirement_row("IF-05-R01"))
    assert any("duplicate requirement ID 'IF-05-R01'" in p for p in contract_problems(twice))
    assert any("no QG-nn goal" in p for p in contract_problems(_contract(rows=_requirement_row("IF-05-R01", "none"))))
    assert any(
        "QG-07 is not a defined ID" in p
        for p in contract_problems(_contract(rows=_requirement_row("IF-05-R01", "QG-07")))
    )
    assert any("no requirement row" in p for p in contract_problems(_contract(rows="")))
    wrong_heading = _contract("IF-06").replace("[[IF-06]]", "[[IF-05]]")
    assert any("heading names 'IF-06'" in p for p in contract_problems(wrong_heading))
    accepted = [
        "open",
        "xref:mechanics::pump.adoc[Mechanics pump page]",
        "xref:adr:004-power-architecture.adoc[SYS-004]",
        "xref:system::protocol.adoc#framing[System protocol page]",
        "xref:protocol.adoc[], xref:adr:004-power-architecture.adoc[SYS-004] and xref:hardware.adoc[]",
    ]
    for met_by in accepted:
        assert contract_problems(_contract(rows=_requirement_row("IF-05-R01", met_by=met_by))) == [], met_by
    for met_by in (
        "xref:missing-page.adoc[]",
        "xref:station::missing-page.adoc[Station page]",
        "soon",
        "open, xref:protocol.adoc[]",
        "xref:protocol.adoc[] maybe",
    ):
        assert contract_problems(_contract(rows=_requirement_row("IF-05-R01", met_by=met_by))), met_by


def test_xref_page_exists() -> None:
    assert xref_page_exists("protocol.adoc") is True
    assert xref_page_exists("ROOT:protocol.adoc") is True
    assert xref_page_exists("adr:001-system-split.adoc") is True
    assert xref_page_exists("system::protocol.adoc#framing") is True
    assert xref_page_exists("system:adr:001-system-split.adoc") is True
    assert xref_page_exists("station::building-blocks.adoc") is True
    assert xref_page_exists("station:adr:001-system-split.adoc") is False
    assert xref_page_exists("missing.adoc") is False
    assert xref_page_exists("mechanics::pump.adoc") is None
    assert xref_page_exists("embedded::pump.adoc") is None


def test_requirement_anchors_are_not_contract_ids(tmp_path: Path) -> None:
    page = tmp_path / "page.adoc"
    page.write_text("[[IF-01]]\n| [[IF-01-R01]]IF-01-R01\n[[IF-1]]\n", encoding="utf-8")
    assert id_anchors(page, "IF") == ["IF-01", "IF-1"]


def test_contracts_page_is_valid() -> None:
    assert contract_problems(CONTRACTS_PAGE.read_text(encoding="utf-8")) == []
    assert first_line(CONTRACTS_PAGE) == "= Contracts"


def test_contracts_page_is_in_nav_after_hardware() -> None:
    nav = (SYSTEM_PAGES.parent / "nav.adoc").read_text(encoding="utf-8").splitlines()
    start = nav.index("** xref:hardware.adoc[]")
    assert nav[start + 1] == "** xref:contracts.adoc[]"


def test_building_blocks_links_contracts() -> None:
    assert "xref:contracts.adoc[" in (SYSTEM_PAGES / "building-blocks.adoc").read_text(encoding="utf-8")


def test_chapter5_diagram_names_every_contract() -> None:
    page = SYSTEM_PAGES / "building-blocks.adoc"
    lines = page.read_text(encoding="utf-8").splitlines()
    blocks = [source for number, source in d2_blocks(page) if 'target="conceptual-blocks"' in lines[number - 1]]
    assert len(blocks) == 1
    labels = [
        label
        for line in blocks[0]
        if not line.lstrip().startswith("#") and D2_EDGE.search(without_quoted(line))
        for label in re.findall(r"""(?:"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')""", line)
    ]
    missing = [contract for contract in defined_ids("IF") if not any(contract in label for label in labels)]
    assert missing == [], f"contracts missing from the chapter 5 diagram: {missing}"


@site_test
def test_contracts_page_renders() -> None:
    html = (SITE_DIR / "system" / "contracts.html").read_text(encoding="utf-8")
    for contract_id in defined_ids("IF"):
        assert f'id="{contract_id}"' in html
