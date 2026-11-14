"""Checks that the dark theme covers every light part of the default UI.

The default UI (`site.css` in the built site) is light.
`site-extra.css` and `dark.css` override it with the palette colours.
Without `DOCS_SITE_DIR`, the tests that read the built `site.css` are skipped.
"""

import re
from typing import NamedTuple

import pytest
from site_html import parse_page
from test_built_site import SITE_DIR, SUPPLEMENTAL_DIR, needs_site, resolve

CSS_DIR = SUPPLEMENTAL_DIR / "css"
OUR_STYLESHEETS = ("site-extra.css", "dark.css")

# A colour with a relative luminance above this is a light colour.
LIGHT_LUMINANCE = 0.5
# The lowest WCAG contrast ratio for text.
MIN_CONTRAST = 4.5
# The lightest surface of the dark theme. Default text colours are checked against it.
LIGHTEST_SURFACE = "ink-elevated"

# Overrides that set a light colour on purpose, because they are not surfaces.
LIGHT_OVERRIDES = {".navbar-burger span": "the lines of the burger icon, drawn on the dark header"}

# The named colours of CSS Color 4.
NAMED_COLOURS = frozenset(
    """aliceblue antiquewhite aqua aquamarine azure beige bisque black blanchedalmond blue blueviolet brown
    burlywood cadetblue chartreuse chocolate coral cornflowerblue cornsilk crimson cyan darkblue darkcyan
    darkgoldenrod darkgray darkgreen darkgrey darkkhaki darkmagenta darkolivegreen darkorange darkorchid darkred
    darksalmon darkseagreen darkslateblue darkslategray darkslategrey darkturquoise darkviolet deeppink deepskyblue
    dimgray dimgrey dodgerblue firebrick floralwhite forestgreen fuchsia gainsboro ghostwhite gold goldenrod gray
    green greenyellow grey honeydew hotpink indianred indigo ivory khaki lavender lavenderblush lawngreen
    lemonchiffon lightblue lightcoral lightcyan lightgoldenrodyellow lightgray lightgreen lightgrey lightpink
    lightsalmon lightseagreen lightskyblue lightslategray lightslategrey lightsteelblue lightyellow lime limegreen
    linen magenta maroon mediumaquamarine mediumblue mediumorchid mediumpurple mediumseagreen mediumslateblue
    mediumspringgreen mediumturquoise mediumvioletred midnightblue mintcream mistyrose moccasin navajowhite navy
    oldlace olive olivedrab orange orangered orchid palegoldenrod palegreen paleturquoise palevioletred papayawhip
    peru pink plum powderblue purple rebeccapurple red rosybrown royalblue saddlebrown salmon sandybrown seagreen
    seashell sienna silver skyblue slateblue slategray slategrey snow springgreen steelblue tan teal thistle tomato
    turquoise violet wheat white whitesmoke yellow yellowgreen""".split()
)
# The named colours that the default UI uses, with their values.
NAMED_COLOUR_VALUES = {
    "black": "#000000",
    "grey": "#808080",
    "gray": "#808080",
    "navy": "#000080",
    "teal": "#008080",
    "white": "#ffffff",
}
# Keywords that are allowed where a colour goes, because they are not colours of their own.
COLOUR_KEYWORDS = frozenset({"transparent", "currentcolor", "inherit", "initial"})

# The properties whose light colours draw a surface, and the properties that override them.
SURFACE_PROPERTIES = {
    "background": {"background", "background-color", "background-image"},
    "background-color": {"background", "background-color"},
    "scrollbar-color": {"scrollbar-color"},
}
# The properties whose light colours draw a line.
LINE_PROPERTIES = ("border", "outline", "box-shadow")

# --- a small CSS reader ------------------------------------------------------------------------------------------


class Rule(NamedTuple):
    selectors: tuple[str, ...]
    declarations: tuple[tuple[str, str], ...]
    comments: tuple[str, ...]
    context: tuple[str, ...]  # the enclosing at-rules, outermost first


# At-rules that hold style rules. Every other at-rule with a block (font-face, page, keyframes) is skipped.
_GROUPING_AT_RULES = ("@media", "@supports", "@layer", "@container")


def _skip_string(css: str, i: int) -> int:
    quote = css[i]
    i += 1
    while css[i] != quote:
        i += 2 if css[i] == "\\" else 1
    return i + 1


def _skip_block(css: str, i: int) -> int:
    """The index after the `}` that closes the block opened just before `i`."""
    depth = 1
    while depth:
        if css.startswith("/*", i):
            i = css.index("*/", i) + 2
            continue
        if css[i] in "\"'":
            i = _skip_string(css, i)
            continue
        depth += {"{": 1, "}": -1}.get(css[i], 0)
        i += 1
    return i


def _split_top_level(text: str, separator: str) -> list[str]:
    """`text` split at `separator`, outside strings, brackets and parentheses."""
    parts, depth, start, i = [], 0, 0, 0
    while i < len(text):
        ch = text[i]
        if ch in "\"'":
            i = _skip_string(text, i)
            continue
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        elif ch == separator and depth == 0:
            parts.append(text[start:i])
            start = i + 1
        i += 1
    parts.append(text[start:])
    return parts


def normalise_selector(selector: str) -> str:
    return re.sub(r"\s*([>+~])\s*", r"\1", " ".join(selector.split()))


def _read_body(css: str, i: int) -> tuple[str, list[str], int]:
    """The declarations and the comments of a rule body that starts at `i`, and the index after its `}`."""
    body, comments = [], []
    while css[i] != "}":
        if css.startswith("/*", i):
            end = css.index("*/", i)
            comments.append(" ".join(css[i + 2 : end].split()))
            i = end + 2
        elif css[i] in "\"'":
            end = _skip_string(css, i)
            body.append(css[i:end])
            i = end
        else:
            body.append(css[i])
            i += 1
    return "".join(body), comments, i + 1


def _parse_declarations(body: str) -> tuple[tuple[str, str], ...]:
    declarations = []
    for part in _split_top_level(body, ";"):
        if ":" in part:
            name, value = part.split(":", 1)
            value = re.sub(r"\s*!important\s*$", "", value.strip())
            declarations.append((name.strip().lower(), value))
    return tuple(declarations)


def _parse(css: str, i: int, context: tuple[str, ...], rules: list[Rule]) -> int:
    prelude = []
    while i < len(css):
        if css.startswith("/*", i):
            i = css.index("*/", i) + 2
            continue
        ch = css[i]
        if ch in "\"'":
            end = _skip_string(css, i)
            prelude.append(css[i:end])
            i = end
        elif ch == "{":
            head = " ".join("".join(prelude).split())
            if head.startswith("@"):
                # `( pointer: fine )` is the same condition as `(pointer:fine)`.
                head = re.sub(r"\s*:\s*", ":", re.sub(r"\(\s+", "(", re.sub(r"\s+\)", ")", head)))
            prelude = []
            if head.startswith(_GROUPING_AT_RULES):
                i = _parse(css, i + 1, (*context, head), rules)
            elif head.startswith("@"):
                i = _skip_block(css, i + 1)
            else:
                body, comments, i = _read_body(css, i + 1)
                selectors = tuple(normalise_selector(part) for part in _split_top_level(head, ","))
                rules.append(Rule(selectors, _parse_declarations(body), tuple(comments), context))
        elif ch == "}":
            return i + 1
        elif ch == ";" and "".join(prelude).lstrip().startswith("@"):
            prelude = []  # a statement at-rule such as @import
            i += 1
        else:
            prelude.append(ch)
            i += 1
    return i


def parse_css(css: str) -> list[Rule]:
    """The style rules of `css`, without the rules for print."""
    rules: list[Rule] = []
    _parse(css, 0, (), rules)
    return [rule for rule in rules if not any("print" in at_rule for at_rule in rule.context)]


# --- colours -----------------------------------------------------------------------------------------------------


def palette() -> dict[str, str]:
    """The generated palette: the name after `--cm-` and its hex value."""
    css = (CSS_DIR / "palette.css").read_text(encoding="utf-8")
    return dict(re.findall(r"--cm-([a-z0-9-]+):\s*(#[0-9a-f]{6})\s*;", css))


def _channel(value: float) -> float:
    value /= 255
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def luminance(rgb: tuple[float, float, float]) -> float:
    red, green, blue = (_channel(channel) for channel in rgb)
    return 0.2126 * red + 0.7152 * green + 0.0722 * blue


def contrast(first: tuple[float, float, float], second: tuple[float, float, float]) -> float:
    lighter, darker = sorted((luminance(first), luminance(second)), reverse=True)
    return (lighter + 0.05) / (darker + 0.05)


def _hex_digits(value: str) -> str:
    """The 6 or 8 hex digits of a colour, with the short forms expanded."""
    digits = value.lstrip("#")
    return "".join(digit * 2 for digit in digits) if len(digits) in (3, 4) else digits


def hex_rgb(value: str) -> tuple[float, float, float]:
    digits = _hex_digits(value)
    return tuple(int(digits[i : i + 2], 16) for i in (0, 2, 4))


def hex_alpha(value: str) -> float:
    digits = _hex_digits(value)
    return int(digits[6:8], 16) / 255 if len(digits) == 8 else 1.0


_COLOUR_TOKEN = re.compile(r"#[0-9a-fA-F]{3,8}\b|\brgba?\([^)]*\)|\b[a-zA-Z]+\b")


def colours(value: str, variables: dict[str, str] | None = None) -> list[tuple[float, float, float]]:
    """The opaque colours in a declaration value. A colour with less than half opacity does not count."""
    if variables is not None:
        value = re.sub(r"var\(--cm-([a-z0-9-]+)\)", lambda match: variables[match.group(1)], value)
    value = re.sub(r"url\([^)]*\)", "", value)
    found = []
    for token in _COLOUR_TOKEN.findall(value):
        lowered = token.lower()
        if token.startswith("#"):
            if hex_alpha(token) >= 0.5:
                found.append(hex_rgb(token))
        elif lowered.startswith("rgb"):
            numbers = [float(number) for number in re.findall(r"[\d.]+", token)]
            if len(numbers) < 4 or numbers[3] >= 0.5:
                found.append(tuple(numbers[:3]))
        elif lowered in NAMED_COLOURS:
            assert lowered in NAMED_COLOUR_VALUES, f"add the named colour {lowered} to NAMED_COLOUR_VALUES"
            found.append(hex_rgb(NAMED_COLOUR_VALUES[lowered]))
    return found


def is_light(value: str, variables: dict[str, str] | None = None) -> bool:
    return any(luminance(colour) > LIGHT_LUMINANCE for colour in colours(value, variables))


# --- the stylesheets ---------------------------------------------------------------------------------------------


def default_rules() -> list[Rule]:
    return parse_css((SITE_DIR / "_" / "css" / "site.css").read_text(encoding="utf-8"))


def our_rules() -> list[Rule]:
    return [
        rule
        for name in OUR_STYLESHEETS
        if (CSS_DIR / name).is_file()
        for rule in parse_css((CSS_DIR / name).read_text(encoding="utf-8"))
    ]


def overrides(selector: str, context: tuple[str, ...], properties, rules: list[Rule]) -> list[str]:
    """The values that our rules set for `selector` with one of `properties`, where the default rule applies.

    An override counts if it applies at least wherever the default rule applies: outside any at-rule,
    or inside the same at-rules.
    """
    return [
        value
        for rule in rules
        if selector in rule.selectors and rule.context in ((), context)
        for name, value in rule.declarations
        if properties(name)
    ]


def find_missing(check) -> list[str]:
    """Every default rule that `check(selector, context, name, value)` reports, one line each."""
    missing = []
    for rule in default_rules():
        for name, value in rule.declarations:
            for selector in rule.selectors:
                problem = check(selector, rule.context, name, value)
                if problem:
                    where = " ".join(rule.context)
                    missing.append(f"{selector} {{ {name}: {value} }}{' in ' + where if where else ''}: {problem}")
    return sorted(set(missing))


# --- tests -------------------------------------------------------------------------------------------------------


def test_css_reader():
    rules = parse_css(
        "/* a */ a,b>c ,d + e{color:red;/* contrast: --cm-x on --cm-y */background:url(x.svg#a;b)}"
        "@media screen and (min-width:1px){.x{color:#fff!important}}"
        "@media print{.p{color:red}}@font-face{src:url(f)}"
        "@media ( pointer: fine ){@supports (a:b){html{color:red}}}"
    )
    assert rules == [
        Rule(
            ("a", "b>c", "d+e"),
            (("color", "red"), ("background", "url(x.svg#a;b)")),
            ("contrast: --cm-x on --cm-y",),
            (),
        ),
        Rule((".x",), (("color", "#fff"),), (), ("@media screen and (min-width:1px)",)),
        Rule(("html",), (("color", "red"),), (), ("@media (pointer:fine)", "@supports (a:b)")),
    ]


def test_contrast_comment_reader():
    assert comment_pairs("contrast: --cm-sand on --cm-ink, --cm-cream on --cm-ink-raised") == [
        ("sand", "ink"),
        ("cream", "ink-raised"),
    ]
    assert comment_pairs("The label at the top left.") == []


def test_colour_reader():
    assert colours("linear-gradient(180deg,#f0f0f0 0,#fff)") == [(240, 240, 240), (255, 255, 255)]
    assert colours("rgba(0,0,0,.4) url(#fff) transparent") == []
    assert colours("var(--cm-ink)", {"ink": "#0a0806"}) == [(10, 8, 6)]
    assert is_light("#c1c1c1") and not is_light("#9c9c9c") and not is_light("none")
    assert round(contrast((255, 255, 255), (0, 0, 0)), 1) == 21.0


@needs_site
def test_every_light_background_has_a_dark_override():
    rules = our_rules()
    variables = palette()

    def check(selector, context, name, value):
        if name not in SURFACE_PROPERTIES or not is_light(value):
            return None
        allowed = SURFACE_PROPERTIES[name]
        if "gradient(" in value:
            allowed = allowed - {"background-color"}
        values = overrides(selector, context, allowed.__contains__, rules)
        if not values:
            return "no override in site-extra.css or dark.css"
        if selector not in LIGHT_OVERRIDES and any(is_light(override, variables) for override in values):
            return f"the override is light too: {values}"
        return None

    missing = find_missing(check)
    assert not missing, "light surfaces without a dark override:\n" + "\n".join(missing)


@needs_site
def test_every_light_line_has_a_dark_override():
    rules = our_rules()
    variables = palette()

    def check(selector, context, name, value):
        prefix = next((prefix for prefix in LINE_PROPERTIES if name.startswith(prefix)), None)
        if prefix is None or name.endswith(("-width", "-style", "-radius")) or not is_light(value):
            return None
        values = overrides(selector, context, lambda other: other.startswith(prefix), rules)
        if not values:
            return "no override in site-extra.css or dark.css"
        if any(is_light(override, variables) for override in values):
            return f"the override is light too: {values}"
        return None

    missing = find_missing(check)
    assert not missing, "light lines without a dark override:\n" + "\n".join(missing)


@needs_site
def test_every_dark_text_colour_has_an_override():
    rules = our_rules()
    variables = palette()
    surface = hex_rgb(variables[LIGHTEST_SURFACE])

    def unreadable(value, variables=None):
        return any(contrast(colour, surface) < MIN_CONTRAST for colour in colours(value, variables))

    def check(selector, context, name, value):
        if name != "color" or not unreadable(value):
            return None
        values = overrides(selector, context, "color".__eq__, rules)
        if not values:
            return "no override in site-extra.css or dark.css"
        if any(unreadable(override, variables) for override in values):
            return f"the override is not readable on {LIGHTEST_SURFACE} either: {values}"
        return None

    missing = find_missing(check)
    assert not missing, "text colours too dark for the dark theme:\n" + "\n".join(missing)


# The lowest WCAG contrast ratio for icons and other graphics.
MIN_GRAPHIC_CONTRAST = 3.0
# The properties that replace or recolour a background image.
ICON_OVERRIDES = {
    "background",
    "background-image",
    "filter",
    "mask",
    "mask-image",
    "-webkit-mask",
    "-webkit-mask-image",
}


def icon_colours(svg: str) -> list[tuple[float, float, float]]:
    """The colours an SVG icon draws with. An SVG without a fill draws in black."""
    values = re.findall(r'(?:fill|stroke)="([^"]+)"', svg)
    found = [colour for value in values if value != "none" for colour in colours(value)]
    return found or [(0, 0, 0)]


@needs_site
def test_every_dark_icon_has_an_override():
    """The default icons are dark images, so each rule that shows one needs an override."""
    rules = our_rules()
    surface = hex_rgb(palette()[LIGHTEST_SURFACE])

    def check(selector, context, name, value):
        if name not in ("background", "background-image"):
            return None
        dark = []
        for path in re.findall(r"url\(\.\./img/([^)#]+\.svg)", value):
            svg = (SITE_DIR / "_" / "img" / path).read_text(encoding="utf-8")
            if any(contrast(colour, surface) < MIN_GRAPHIC_CONTRAST for colour in icon_colours(svg)):
                dark.append(path)
        if not dark:
            return None
        if not overrides(selector, context, ICON_OVERRIDES.__contains__, rules):
            return f"{', '.join(dark)} is too dark for {LIGHTEST_SURFACE} and has no override"
        return None

    missing = find_missing(check)
    assert not missing, "dark icons without an override:\n" + "\n".join(missing)


_CONTRAST_PAIR = re.compile(r"--cm-([a-z0-9-]+)\s+on\s+--cm-([a-z0-9-]+)")


def comment_pairs(comment: str) -> list[tuple[str, str]]:
    """The pairs of a `contrast: --cm-a on --cm-b, --cm-c on --cm-d` comment."""
    if not comment.startswith("contrast:"):
        return []
    return [_CONTRAST_PAIR.fullmatch(part.strip()).groups() for part in comment.removeprefix("contrast:").split(",")]


_PALETTE_VARIABLE = re.compile(r"var\(--cm-([a-z0-9-]+)\)")


def contrast_pairs() -> list[tuple[str, str, Rule]]:
    """The `/* contrast: --cm-text on --cm-background */` comments of our stylesheets, with their rules."""
    return [
        (text, background, rule)
        for rule in our_rules()
        for comment in rule.comments
        for text, background in comment_pairs(comment)
    ]


def test_text_colour_pairs_are_readable():
    variables = palette()
    pairs = contrast_pairs()
    assert any(rule in parse_css((CSS_DIR / "dark.css").read_text(encoding="utf-8")) for _, _, rule in pairs), (
        "dark.css names no contrast pairs"
    )
    problems = []
    for text, background, rule in pairs:
        unknown = [name for name in (text, background) if name not in variables]
        if unknown:
            problems.append(f"{rule.selectors[0]}: --cm-{', --cm-'.join(unknown)} is not in palette.css")
            continue
        ratio = contrast(hex_rgb(variables[text]), hex_rgb(variables[background]))
        if ratio < MIN_CONTRAST:
            problems.append(f"{rule.selectors[0]}: --cm-{text} on --cm-{background} has a contrast of {ratio:.2f}")
    assert not problems, "\n".join(problems)


def test_every_text_colour_names_its_contrast_pair():
    """Each of our rules that sets a text colour names that colour in a contrast comment."""
    assert (CSS_DIR / "dark.css").is_file(), "dark.css does not exist"
    problems = []
    for rule in our_rules():
        texts = {
            name for prop, value in rule.declarations if prop == "color" for name in _PALETTE_VARIABLE.findall(value)
        }
        named = {text for comment in rule.comments for text, _ in comment_pairs(comment)}
        if texts - named:
            unnamed = ", ".join(f"--cm-{name}" for name in sorted(texts - named))
            problems.append(f"{', '.join(rule.selectors)}: no contrast comment for {unnamed}")
    assert not problems, "\n".join(problems)


@pytest.mark.parametrize("name", OUR_STYLESHEETS)
def test_dark_and_extra_css_use_only_palette_colours(name):
    css = (CSS_DIR / name).read_text(encoding="utf-8")
    css = re.sub(r"/\*.*?\*/", "", css, flags=re.DOTALL)
    hex_colours = re.findall(r"#(?:[0-9a-fA-F]{8}|[0-9a-fA-F]{6}|[0-9a-fA-F]{3,4})\b", css)
    functions = re.findall(r"\b(?:rgba?|hsla?|hwb|lab|lch|oklab|oklch|color|color-mix)\(", css)
    named = [
        f"{prop}: {value}"
        for rule in parse_css(css)
        for prop, value in rule.declarations
        for word in re.findall(r"[a-zA-Z-]+", re.sub(r"var\([^)]*\)|url\([^)]*\)|\"[^\"]*\"|'[^']*'", "", value))
        if word.lower() in NAMED_COLOURS and word.lower() not in COLOUR_KEYWORDS
    ]
    assert not hex_colours, f"raw hex colours in {name}: {hex_colours}"
    assert not functions, f"colour literals in {name}: {functions}"
    assert not named, f"named colours in {name}: {named}"
    variables = set(palette())
    unknown = sorted(set(_PALETTE_VARIABLE.findall(css)) - variables)
    assert not unknown, f"{name} uses variables that palette.css does not define: {unknown}"


@needs_site
def test_404_page_loads_the_dark_theme():
    path = SITE_DIR / "404.html"
    if not path.is_file():
        pytest.skip("the build wrote no 404.html (the playbook sets no site.url)")
    stylesheets = [
        element.attrs.get("href", "")
        for element in parse_page(path).elements()
        if element.tag == "link" and element.attrs.get("rel") == "stylesheet"
    ]
    assert stylesheets and stylesheets[-1].endswith("css/dark.css")
    assert resolve(path, stylesheets[-1]).is_file()
    assert "status-404" in parse_page(path).source


REFERENCE_PAGE = "system/style-reference.html"


def page_title(path) -> str:
    titles = [element.text for element in parse_page(path).elements() if element.tag == "title"]
    assert len(titles) == 1, f"{path} has {len(titles)} title elements"
    return titles[0]


@needs_site
def test_reference_page_has_every_element():
    page = parse_page(SITE_DIR / REFERENCE_PAGE)
    tags = {element.tag for element in page.elements()}
    assert {"h2", "h3", "h4", "h5", "ol", "ul", "dl", "pre", "code", "table"} <= tags
    assert page.elements("tableblock")
    for kind in ("note", "tip", "important", "caution", "warning"):
        assert any(kind in element.classes for element in page.elements("admonitionblock")), f"no {kind} admonition"
    for block in ("exampleblock", "sidebarblock", "quoteblock"):
        assert page.elements(block), f"no {block}"
    diagrams = [
        element
        for element in page.elements(within="kroki")
        if element.tag == "img" and element.attrs.get("src", "").endswith(".svg")
    ]
    assert diagrams, "no D2 diagram image"
    cells = [(element.tag, element.text) for element in page.elements("tableblock") if element.tag in ("th", "td")]
    rows = set(zip(cells, cells[1:], strict=False))
    assert (("th", "Status"), ("td", "Superseded")) in rows, "no ADR header with the status Superseded"
    warnings = [element for element in page.elements("admonitionblock") if "warning" in element.classes]
    assert any("superseded by SYS-003" in element.text for element in warnings), "no Superseded warning"
    xrefs = {link.href for link in page.links("xref")}
    assert {"protocol.html", "adr/003-protocol-communication.html"} <= xrefs, f"missing xrefs: {xrefs}"


@needs_site
def test_reference_page_not_in_nav():
    page = parse_page(SITE_DIR / "system" / "index.html")
    assert not [link for link in page.links(within="nav-menu") if "style-reference" in link.href]
    assert "style-reference" not in page.source


@needs_site
def test_page_title_names_the_component():
    assert (
        page_title(SITE_DIR / "station/building-blocks.html") == "5 Building Block View · Station · Sir-Mix-A-Lot Docs"
    )


@needs_site
def test_404_title_has_no_component():
    path = SITE_DIR / "404.html"
    if not path.is_file():
        pytest.skip("the build wrote no 404.html (the playbook sets no site.url)")
    title = page_title(path)
    assert title.endswith(" · Sir-Mix-A-Lot Docs") and title.count(" · ") == 1
