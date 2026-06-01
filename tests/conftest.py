import pathlib
import pytest

from panelkit.config import load_footprints, load_layout, load_theme
from panelkit.text import FontBook

ROOT = pathlib.Path(__file__).resolve().parents[1]


@pytest.fixture(scope="session")
def footprints():
    return load_footprints()


@pytest.fixture(scope="session")
def layout():
    return load_layout(override_path=ROOT / "specs/layout/ghost.yaml")


@pytest.fixture(scope="session")
def theme():
    return load_theme(ROOT / "specs/themes/ghost.yaml")


@pytest.fixture(scope="session")
def fontbook(theme):
    return FontBook(theme)
