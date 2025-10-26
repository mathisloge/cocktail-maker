import pytest

# Import all fixture modules to make them available
pytest_plugins = [
    "tests_e2e.fixtures.cocktail_maker",
    "tests_e2e.steps.common_steps",
]


@pytest.fixture(scope="session")
def probe_url():
    """Base URL for the application."""
    return "http://localhost:8000"
