import json
from pathlib import Path

from check_adoc_style import check_text

REPO = Path(__file__).resolve().parents[3]
SKILLS = REPO / "plugins/cocktail-maker-docs/skills"


def skill_dirs() -> list[Path]:
    return sorted(path.parent for path in SKILLS.glob("*/SKILL.md"))


def parse_frontmatter(path: Path) -> dict[str, str]:
    lines = path.read_text(encoding="utf-8").splitlines()
    assert lines[0] == "---", f"{path} does not start with frontmatter"
    end = lines.index("---", 1)
    meta: dict[str, str] = {}
    for line in lines[1:end]:
        key, sep, value = line.partition(":")
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "'\"":
            value = value[1:-1]
        if sep:
            meta[key.strip()] = value
    return meta


def test_skill_dirs_found():
    assert "writing-style" in [d.name for d in skill_dirs()]


def test_marketplace_points_at_plugin():
    market = json.loads((REPO / ".claude-plugin/marketplace.json").read_text())
    assert market["name"] == "cocktail-maker"
    entry = market["plugins"][0]
    assert entry["name"] == "cocktail-maker-docs"
    plugin = json.loads((REPO / entry["source"] / ".claude-plugin/plugin.json").read_text())
    assert plugin["name"] == entry["name"]
    assert plugin["version"] == entry["version"]
    assert plugin["description"] == entry["description"]


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
    source = settings["extraKnownMarketplaces"]["cocktail-maker"]["source"]
    assert source == {"source": "github", "repo": "mathisloge/cocktail-maker"}


def test_writing_style_points_at_avoid_list():
    text = (SKILLS / "writing-style/SKILL.md").read_text()
    assert "avoid-words.tsv" in text
    assert (SKILLS / "writing-style/avoid-words.tsv").is_file()


def test_expected_skills_present():
    assert {d.name for d in skill_dirs()} == {"writing-style", "architecture-docs", "writing-adrs", "writing-diagrams"}


def test_adr_template_passes_style():
    text = (SKILLS / "writing-adrs/adr-template.adoc").read_text()
    assert [f for f in check_text(text) if f.severity == "error"] == []
    assert "include::partial$adr-header.adoc[]" in text
