import pytest
from pathlib import Path
from quite import ProbeManager, Probe, RemoteObject, query


@pytest.fixture(scope="session")
def probe_manager():
    return ProbeManager()


@pytest.fixture(scope="session")
def cocktail_maker_app(probe_manager: ProbeManager) -> Probe:
    current_dir = Path(__file__).parent
    path = current_dir / ".." / ".." / "build" / "runtime" / "cocktail-maker"
    return probe_manager.launch_qt_probe_application(
        name="cocktail-maker",
        path_to_application=str(path),
    )


@pytest.fixture(scope="session")
def cocktail_maker_page(cocktail_maker_app: Probe) -> RemoteObject:
    return cocktail_maker_app.find_object(
        object_query=query().property("objectName", "stackView")
    )
