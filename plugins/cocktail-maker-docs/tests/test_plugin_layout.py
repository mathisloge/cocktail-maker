import json
import re
import shutil
import subprocess
import sys
import tomllib
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


def test_adr_template_has_quality_goals():
    # A small stand-in for `adr_goal_problems` in docs/tests, so the plugin tests stay self-contained.
    text = (SKILLS / "writing-adrs/adr-template.adoc").read_text(encoding="utf-8")
    titles = [line for line in text.splitlines() if line.startswith("==") and not line.startswith("===")]
    assert titles[:2] == ["== Context", "== Quality Goals"]
    section = text.split("\n== Quality Goals\n", 1)[1].split("\n== ", 1)[0]
    assert re.search(r"\bQG-\d\d\b", section)


SCRIPT = REPO / "plugins/cocktail-maker-docs/scripts/check_adoc_style.py"
WHEEL_FILES = {
    "plugins/cocktail-maker-docs/scripts/check_adoc_style.py": "cm_adoc_style/check_adoc_style.py",
    "plugins/cocktail-maker-docs/skills/writing-style/avoid-words.tsv": "cm_adoc_style/avoid-words.tsv",
    "plugins/cocktail-maker-docs/skills/writing-style/glossary-system.tsv": "cm_adoc_style/glossary-system.tsv",
}


def test_hook_installs_as_python_package():
    # pre-commit installs the hook repo with pip, so the hook runs wherever pre-commit runs.
    manifest = (REPO / ".pre-commit-hooks.yaml").read_text(encoding="utf-8")
    assert "  language: python\n" in manifest
    assert "  entry: adoc-style\n" in manifest
    pyproject = tomllib.loads((REPO / "pyproject.toml").read_text(encoding="utf-8"))
    assert pyproject["project"]["scripts"]["adoc-style"] == "cm_adoc_style.check_adoc_style:main"
    wheel = pyproject["tool"]["hatch"]["build"]["targets"]["wheel"]
    assert wheel["force-include"] == WHEEL_FILES
    for source in WHEEL_FILES:
        assert (REPO / source).is_file(), source


def test_installed_checker_finds_its_data_next_to_it(tmp_path):
    package = tmp_path / "cm_adoc_style"
    package.mkdir()
    for source, target in WHEEL_FILES.items():
        shutil.copy(REPO / source, tmp_path / target)
    page = tmp_path / "page.adoc"
    page.write_text("= Page\n\nThe staff use it in order to pour.\n", encoding="utf-8")
    result = subprocess.run(
        [sys.executable, "-I", str(package / "check_adoc_style.py"), str(page)],
        capture_output=True,
        text=True,
        cwd=tmp_path,
    )
    assert "avoid-word" in result.stdout
    assert "glossary-term" in result.stdout


CONTRACT_SECTION_TITLES = (
    "What crosses",
    "Guarantees and limits",
    "Fault behaviour",
    "Requirements",
    "Open questions",
    "Sources",
)
CONTRACT_STATUS_VALUES = ("Decided", "Partly decided", "Not designed yet")


def test_architecture_skill_has_the_contract_template():
    # A small stand-in for `contract_problems` in docs/tests, so the plugin tests stay self-contained.
    text = (SKILLS / "architecture-docs/SKILL.md").read_text(encoding="utf-8")
    positions = [text.find(f"=== {title}\n") for title in CONTRACT_SECTION_TITLES]
    assert all(position >= 0 for position in positions)
    assert positions == sorted(positions)
    for status in CONTRACT_STATUS_VALUES:
        assert status in text
    for manifest in (".claude-plugin/marketplace.json", "plugins/cocktail-maker-docs/.claude-plugin/plugin.json"):
        assert '"version": "0.6.0"' in (REPO / manifest).read_text(encoding="utf-8"), manifest
