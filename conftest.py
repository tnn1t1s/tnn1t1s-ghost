"""Repo-root conftest: make the `panelkit` package importable in tests without
requiring an editable install."""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
