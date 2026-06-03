#!/usr/bin/env python3
"""Generate an English source-string inventory for hard-coded UI text."""

from __future__ import annotations

import json
import re
import unicodedata
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "common" / "config" / "strings.en.json"

SOURCE_GLOBS = (
    "src/webserver/**/*.js",
    "components/espcontrol/**/*.h",
)

JSON_SOURCES = (
    ROOT / "common" / "config" / "card_contract.json",
)

SKIP_FILES = {
    "entry.js",
    "styles.js",
    "sun_calc.h",
}

STRING_RE = re.compile(r"(?P<q>[\"'])(?P<body>(?:\\.|(?!\1).)*?)(?P=q)")
LOWER_DISPLAY_WORDS = {
    "on",
    "off",
    "auto",
    "low",
    "high",
    "medium",
    "home",
    "away",
    "sleep",
    "activity",
    "eco",
    "boost",
    "comfort",
}
IGNORED_EXACT = {
    "block",
    "button",
    "change",
    "click",
    "false",
    "input",
    "json",
    "label",
    "none",
    "null",
    "options",
    "precision",
    "sensor",
    "source",
    "state",
    "status",
    "target",
    "true",
    "type",
    "undefined",
    "unit",
    "value",
    "yaml",
}
IGNORED_SUBSTRINGS = (
    "&amp;",
    "&lt;",
    "&times;",
    "${",
    "%",
    "/api/",
    "://",
    "<",
    "=",
    ">",
    "__",
    "::",
    "LV_",
    "alarm_control_panel.",
    "automation.",
    "binary_sensor.",
    "button.",
    "climate.",
    "cover.",
    "device_tracker.",
    "entity_id",
    "fan.",
    "firmware__",
    "homeassistant",
    "input_",
    "light.",
    "media_player.",
    "mdi-",
    "number-",
    "script.",
    "screen__",
    "select-",
    "select.",
    "sensor.",
    "sp-",
    "std::",
    "subpage_",
    "switch-",
    "switch.",
    "text-",
    "update-",
    "{",
    "}",
)
IGNORED_PATTERNS = (
    re.compile(r"^[a-z0-9_./:-]+$"),
    re.compile(r"^#[0-9A-Fa-f]{3,8}$"),
    re.compile(r"^[A-Z0-9_]+$"),
    re.compile(r"^\d+(\.\d+)?$"),
    re.compile(r"^\.[A-Za-z0-9_-]+"),
)


def decode_js_string(value: str) -> str:
    if "\\" not in value:
        return value
    try:
        return bytes(value, "utf-8").decode("unicode_escape")
    except UnicodeDecodeError:
        return value


def candidate_line(line: str, suffix: str) -> bool:
    if "find_icon(" in line:
        return False
    if suffix == ".h":
        markers = (
            "lv_label_set_text",
            "return ",
            "== ",
            "alarm_",
            "climate_",
            "fan_",
            "media_",
            "todo_",
        )
    else:
        markers = (
            "label",
            "placeholder",
            "requiredMessage",
            "textContent",
            "innerHTML",
            "selectField",
            "textField",
            "toggleSection",
            "return ",
            "options",
        )
    return any(marker in line for marker in markers)


def is_display_string(value: str) -> bool:
    value = value.strip()
    if len(value) < 2 or len(value) > 120:
        return False
    if not any(ch.isalpha() for ch in value):
        return False
    if value.lower() in IGNORED_EXACT:
        return False
    if any(part in value for part in IGNORED_SUBSTRINGS):
        return False
    if "/" in value and value not in {"Heat/Cool", "Play/Pause"} and " / " not in value:
        return False
    if any(pattern.match(value) for pattern in IGNORED_PATTERNS):
        return False
    if len(value) <= 2 and value not in {"No", "On"}:
        return False
    if re.match(r"^[a-z][a-z0-9-]*$", value) and value not in LOWER_DISPLAY_WORDS:
        return False
    if re.match(r"^[A-Za-z]+[A-Za-z0-9_]*$", value) and not value[:1].isupper():
        return False
    if value.startswith(("label_", "pin_", "number_", "icon_")):
        return False
    return True


def slugify(value: str) -> str:
    normalized = unicodedata.normalize("NFKD", value)
    ascii_value = normalized.encode("ascii", "ignore").decode("ascii")
    slug = re.sub(r"[^a-zA-Z0-9]+", "_", ascii_value.lower()).strip("_")
    if not slug:
        slug = "string"
    if slug[0].isdigit():
        slug = f"n_{slug}"
    return slug


def add_string(strings: dict[str, set[str]], value: str, source: str) -> None:
    value = decode_js_string(value).strip()
    if not is_display_string(value):
        return
    strings.setdefault(value, set()).add(source)


def extract_code_strings(strings: dict[str, set[str]]) -> None:
    files: list[Path] = []
    for pattern in SOURCE_GLOBS:
        files.extend(ROOT.glob(pattern))
    for path in sorted(set(files)):
        if path.name in SKIP_FILES or "generated" in path.name:
            continue
        rel = path.relative_to(ROOT).as_posix()
        for line_no, line in enumerate(path.read_text(errors="ignore").splitlines(), 1):
            if not candidate_line(line, path.suffix):
                continue
            for match in STRING_RE.finditer(line):
                add_string(strings, match.group("body"), f"{rel}:{line_no}")


def json_line_index(path: Path) -> dict[str, int]:
    index: dict[str, int] = {}
    for line_no, line in enumerate(path.read_text().splitlines(), 1):
        for match in STRING_RE.finditer(line):
            index.setdefault(match.group("body"), line_no)
    return index


def extract_json_strings(strings: dict[str, set[str]]) -> None:
    ui_keys = {
        "defaultValue",
        "description",
        "experimental",
        "label",
        "placeholder",
        "requiredMessage",
    }

    for path in JSON_SOURCES:
        line_index = json_line_index(path)
        rel = path.relative_to(ROOT).as_posix()
        data = json.loads(path.read_text())

        def walk(value: object, key: str = "") -> None:
            if isinstance(value, dict):
                for child_key, child_value in value.items():
                    walk(child_value, child_key)
                return
            if isinstance(value, list):
                for item in value:
                    walk(item, key)
                return
            if not isinstance(value, str):
                return
            if key not in ui_keys and not key.endswith("Label"):
                return
            line_no = line_index.get(value, 1)
            add_string(strings, value, f"{rel}:{line_no}")

        walk(data)


def build_inventory(strings: dict[str, set[str]]) -> dict[str, object]:
    entries: dict[str, dict[str, object]] = {}
    used_keys: set[str] = set()
    for value in sorted(strings, key=lambda item: item.lower()):
        key = slugify(value)
        base = key
        suffix = 2
        while key in used_keys:
            key = f"{base}_{suffix}"
            suffix += 1
        used_keys.add(key)
        entries[key] = {
            "value": value,
            "sources": sorted(strings[value]),
        }
    return {
        "$schema": "https://espcontrol.local/schemas/source-strings-v1.json",
        "language": "en",
        "description": (
            "English source-string inventory for hard-coded user-facing text. "
            "This file is generated for translation review and is not yet used at runtime."
        ),
        "generatedBy": "scripts/generate_strings_inventory.py",
        "strings": entries,
    }


def main() -> None:
    strings: dict[str, set[str]] = {}
    extract_code_strings(strings)
    extract_json_strings(strings)
    OUTPUT.write_text(json.dumps(build_inventory(strings), indent=2, ensure_ascii=False) + "\n")
    print(f"Wrote {OUTPUT.relative_to(ROOT)}")
    print(f"Strings: {len(strings)}")


if __name__ == "__main__":
    main()
