"""Checks the navigation of a built docs site.

Build the site first and point `DOCS_SITE_DIR` at its output folder.
Without `DOCS_SITE_DIR`, the tests that need the site are skipped.
"""

import os
import re
from pathlib import Path
from urllib.parse import unquote, urlsplit

import pytest
from site_html import Page, parse_page

SITE_DIR = Path(os.environ["DOCS_SITE_DIR"]).resolve() if os.environ.get("DOCS_SITE_DIR") else None

needs_site = pytest.mark.skipif(SITE_DIR is None, reason="DOCS_SITE_DIR is not set")

SUPPLEMENTAL_DIR = Path(__file__).resolve().parent.parent / "supplemental"

SOURCE_URL = "https://github.com/mathisloge/cocktail-maker"

# The stylesheets every page loads last, in this order: the default UI, the palette, our styles, then the dark theme.
STYLESHEETS = ["css/site.css", "css/palette.css", "css/site-extra.css", "css/dark.css"]

# The subprojects the header knows, in header order, with their titles.
KNOWN_SUBPROJECTS = {"station": "Station", "embedded": "Embedded", "mechanics": "Mechanics"}


def component_names() -> list[str]:
    """The components of the built site: every top-level folder except the UI folder `_`."""
    return sorted(path.name for path in SITE_DIR.iterdir() if path.is_dir() and path.name != "_")


def component_title(name: str) -> str:
    """The title of a component, as the sidebar of its start page shows it."""
    if name in KNOWN_SUBPROJECTS:
        return KNOWN_SUBPROJECTS[name]
    page = parse_page(SITE_DIR / name / "index.html")
    titles = [element.text for element in page.elements("title", within="nav-menu")]
    assert titles, f"no component title in the sidebar of {name}/index.html"
    return titles[0]


def expected_subprojects() -> list[str]:
    names = component_names()
    known = [title for name, title in KNOWN_SUBPROJECTS.items() if name in names]
    unknown = sorted(component_title(name) for name in names if name not in KNOWN_SUBPROJECTS and name != "system")
    return known + unknown


def header_pages() -> list[str]:
    """The pages the header tests run on, relative to the site folder."""
    pages = ["system/index.html", "station/index.html"]
    if SITE_DIR is None:
        return pages
    # The start page of every other component, so that a scratch component gets checked too.
    pages += [
        f"{name}/index.html"
        for name in component_names()
        if name not in ("system", "station") and (SITE_DIR / name / "index.html").is_file()
    ]
    adr_pages = sorted((SITE_DIR / "system" / "adr").glob("*.html"))
    if adr_pages:
        pages.append(adr_pages[0].relative_to(SITE_DIR).as_posix())
    return pages


@pytest.fixture(params=header_pages())
def page_path(request) -> Path:
    return SITE_DIR / request.param


def resolve(page_path: Path, href: str) -> Path:
    """The file that a link on `page_path` points to."""
    path = unquote(urlsplit(href).path)
    if path.startswith("/"):
        return (SITE_DIR / path.lstrip("/")).resolve()
    return (page_path.parent / path).resolve()


@needs_site
def test_brand_leads_to_system_start(page_path):
    brands = parse_page(page_path).links("brand")
    assert len(brands) == 1
    assert resolve(page_path, brands[0].href) == SITE_DIR / "system" / "index.html"


@needs_site
def test_parent_then_divider_then_subprojects(page_path):
    page = parse_page(page_path)

    parents = page.links("section-parent")
    assert [link.text for link in parents] == ["System"]
    assert "section-link" in parents[0].classes

    dividers = page.elements("section-divider")
    assert len(dividers) == 1
    assert dividers[0].tag == "span"
    assert dividers[0].attrs.get("aria-hidden") == "true"

    groups = page.elements("section-group")
    assert len(groups) == 1
    assert groups[0].tag == "div"
    assert groups[0].attrs.get("aria-label") == "Subprojects"

    order = [
        kind
        for element in page.elements()
        for kind in ("section-parent", "section-divider", "section-group")
        if kind in element.classes
    ]
    assert order == ["section-parent", "section-divider", "section-group"]

    subprojects = [link.text for link in page.links("section-link", within="section-group")]
    assert subprojects == expected_subprojects()
    assert len(page.links("section-link")) == 1 + len(subprojects)


@needs_site
def test_current_component_marked(page_path):
    links = parse_page(page_path).links("section-link")
    current = [link for link in links if "is-current" in link.classes]
    assert len(current) == 1
    assert current[0].attrs.get("aria-current") == "page"
    assert [link for link in links if "aria-current" in link.attrs] == current

    component = page_path.relative_to(SITE_DIR).parts[0]
    assert resolve(page_path, current[0].href) == SITE_DIR / component / "index.html"


@needs_site
def test_all_header_links_resolve(page_path):
    page = parse_page(page_path)
    links = page.links("brand") + page.links("section-link")
    assert links
    for link in links:
        assert resolve(page_path, link.href).is_file(), f"{link.text}: {link.href} does not exist"


def region_mentions(page, words: list[str]) -> list[str]:
    """The words that the header or the footer shows or links to, in their text or attribute values."""
    found = []
    for region in ("header", "footer"):
        elements = page.elements(region) + page.elements(within=region)
        haystack = " ".join(
            [element.text for element in page.elements(region)]
            + [value for element in elements for value in element.attrs.values()]
        )
        found += [word for word in words if word in haystack]
    return found


def test_region_mentions_ignores_page_content():
    page = Page(
        '<header class="header"><a href="/x">System</a></header>'
        "<article>Download cocktail-maker-embedded</article>"
        '<footer class="footer"><a href="/cocktail-maker-embedded">Services</a></footer>'
    )
    assert region_mentions(page, ["Download", "Services", "cocktail-maker-embedded"]) == [
        "Services",
        "cocktail-maker-embedded",
    ]
    assert region_mentions(Page("<article>Download cocktail-maker-embedded</article>"), ["Download"]) == []


@needs_site
def test_no_dropdown_no_placeholders_no_external_embedded(page_path):
    page = parse_page(page_path)
    assert not page.has_class("nav-panel-explore")
    assert region_mentions(page, ["Products", "Services", "Download", "cocktail-maker-embedded"]) == []


@needs_site
def test_burger_menu_markup(page_path):
    page = parse_page(page_path)
    burgers = page.elements("navbar-burger")
    assert len(burgers) == 1
    assert burgers[0].tag == "button"
    assert burgers[0].attrs.get("aria-controls") == "topbar-nav"
    assert [element.attrs.get("id") for element in page.elements("navbar-menu")] == ["topbar-nav"]


@needs_site
def test_404_has_no_current():
    path = SITE_DIR / "404.html"
    if not path.is_file():
        pytest.skip("the build wrote no 404.html (the playbook sets no site.url)")
    page = parse_page(path)
    links = page.links("section-link")
    assert [link.text for link in page.links("section-parent")] == ["System"]
    assert not [link for link in links if "is-current" in link.classes or "aria-current" in link.attrs]


@needs_site
def test_footer_matches_header(page_path):
    page = parse_page(page_path)
    header_subprojects = [link.text for link in page.links("section-link", within="section-group")]

    sections = page.links("footer-section", within="footer-links")
    assert [link.text for link in sections] == ["System", *header_subprojects]
    assert len(page.links("footer-section")) == len(sections)
    header_targets = [resolve(page_path, link.href) for link in page.links("section-link")]
    assert [resolve(page_path, link.href) for link in sections] == header_targets

    site_titles = [element.text for element in page.elements("brand-name")]
    assert [element.text for element in page.elements("footer-brand", within="footer")] == site_titles
    assert site_titles and site_titles[0]

    sources = page.links("footer-source", within="footer-links")
    assert [link.href for link in sources] == [SOURCE_URL]


@needs_site
def test_all_footer_links_resolve(page_path):
    links = parse_page(page_path).links("footer-section")
    assert links
    for link in links:
        assert not urlsplit(link.href).scheme, f"{link.text}: {link.href} is not relative"
        assert resolve(page_path, link.href).is_file(), f"{link.text}: {link.href} does not exist"


@needs_site
def test_styles_loaded_in_order(page_path):
    stylesheets = [
        element.attrs.get("href", "")
        for element in parse_page(page_path).elements()
        if element.tag == "link" and element.attrs.get("rel") == "stylesheet"
    ]
    assert len(stylesheets) >= len(STYLESHEETS)
    last = stylesheets[-len(STYLESHEETS) :]
    for href, name in zip(last, STYLESHEETS, strict=True):
        assert href.endswith(name), f"expected {name}, got {href}"
        assert resolve(page_path, href).is_file(), f"{href} does not exist"

    assert (SITE_DIR / "_" / "css" / "palette.css").is_file()
    assert (SITE_DIR / "_" / "font" / "SpaceGrotesk.ttf").is_file()


def test_site_extra_uses_only_palette_colours():
    css = (SUPPLEMENTAL_DIR / "css" / "site-extra.css").read_text(encoding="utf-8")
    hex_colours = re.findall(r"#(?:[0-9a-fA-F]{8}|[0-9a-fA-F]{6}|[0-9a-fA-F]{3,4})\b", css)
    functions = re.findall(r"\b(?:rgba?|hsla?|hwb|lab|lch|oklab|oklch|color)\(", css)
    assert not hex_colours, f"raw hex colours in site-extra.css: {hex_colours}"
    assert not functions, f"colour literals in site-extra.css: {functions}"


def test_site_extra_overrides_default_hover_rules():
    css = (SUPPLEMENTAL_DIR / "css" / "site-extra.css").read_text(encoding="utf-8")
    css = re.sub(r"/\*.*?\*/", "", css, flags=re.DOTALL)
    selectors = {part.strip() for rule in re.findall(r"([^{}]+)\{", css) for part in rule.split(",")}
    for needed in (
        ".navbar-menu a.navbar-item:hover",
        ".navbar-end > a.navbar-item:hover",
        ".nav-panel-menu",
        ".navbar-burger:focus-visible",
        ".navbar-menu .navbar-item:focus-visible",
    ):
        assert needed in selectors, f"site-extra.css has no rule for {needed}"
