"""A small reader for the pages of a built Antora site, for the site tests."""

from html.parser import HTMLParser
from pathlib import Path
from typing import NamedTuple

# Elements that never have an end tag, so they never enclose other elements.
_VOID_TAGS = frozenset(
    {"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "source", "track", "wbr"}
)
# Elements whose content is code, not page text.
_RAW_TEXT_TAGS = frozenset({"script", "style"})


class Link(NamedTuple):
    href: str
    text: str
    classes: frozenset[str]
    attrs: dict[str, str]


class Element(NamedTuple):
    tag: str
    text: str
    classes: frozenset[str]
    attrs: dict[str, str]


class _Node:
    def __init__(self, tag: str, attrs: dict[str, str], ancestor_classes: frozenset[str]) -> None:
        self.tag = tag
        self.attrs = attrs
        self.classes = frozenset(attrs.get("class", "").split())
        self.ancestor_classes = ancestor_classes
        self.text_parts: list[str] = []

    def element(self) -> Element:
        return Element(self.tag, " ".join("".join(self.text_parts).split()), self.classes, self.attrs)


class _PageParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.nodes: list[_Node] = []
        self.text_parts: list[str] = []
        self._open: list[_Node] = []

    def _add(self, tag: str, attrs: list[tuple[str, str | None]]) -> _Node:
        ancestor_classes = frozenset().union(*(node.classes for node in self._open))
        node = _Node(tag, {name: value or "" for name, value in attrs}, ancestor_classes)
        self.nodes.append(node)
        return node

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        node = self._add(tag, attrs)
        if tag not in _VOID_TAGS:
            self._open.append(node)

    def handle_startendtag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        self._add(tag, attrs)

    def handle_endtag(self, tag: str) -> None:
        # An end tag closes its element and every element left open inside it.
        if any(node.tag == tag for node in self._open):
            while self._open.pop().tag != tag:
                pass

    def handle_data(self, data: str) -> None:
        if any(node.tag in _RAW_TEXT_TAGS for node in self._open):
            return
        self.text_parts.append(data)
        for node in self._open:
            node.text_parts.append(data)


class Page:
    """One parsed HTML page: its elements in document order, its text and its source."""

    def __init__(self, source: str) -> None:
        parser = _PageParser()
        parser.feed(source)
        parser.close()
        self.source = source
        self.text = "".join(parser.text_parts)
        self._nodes = parser.nodes

    def elements(self, css_class: str | None = None, within: str | None = None) -> list[Element]:
        """The elements that have `css_class` and sit inside an element with the class `within`."""
        return [
            node.element()
            for node in self._nodes
            if (css_class is None or css_class in node.classes) and (within is None or within in node.ancestor_classes)
        ]

    def links(self, css_class: str | None = None, within: str | None = None) -> list[Link]:
        """The `a` elements among `elements(css_class, within)`."""
        return [
            Link(element.attrs.get("href", ""), element.text, element.classes, element.attrs)
            for element in self.elements(css_class, within)
            if element.tag == "a"
        ]

    def has_class(self, css_class: str) -> bool:
        return any(css_class in node.classes for node in self._nodes)


def parse_page(path: Path) -> Page:
    return Page(path.read_text(encoding="utf-8"))
