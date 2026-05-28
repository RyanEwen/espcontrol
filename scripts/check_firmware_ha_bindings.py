#!/usr/bin/env python3
"""Guard firmware Home Assistant access behind button_grid_ha.h helpers."""

from __future__ import annotations

import argparse
import re
from pathlib import Path
from tempfile import TemporaryDirectory


ROOT = Path(__file__).resolve().parents[1]
FIRMWARE_DIR = ROOT / "components" / "espcontrol"
HA_BOUNDARY_ALLOWLIST = {
    "button_grid_ha.h",
}
DIRECT_HA_PATTERNS = (
    (re.compile(r"\bglobal_api_server\b"), "access Home Assistant API through button_grid_ha.h helpers"),
    (re.compile(r"->send_homeassistant_action\s*\("), "send Home Assistant actions through button_grid_ha.h helpers"),
    (re.compile(r"->subscribe_home_assistant_state\s*\("), "subscribe to Home Assistant state through button_grid_ha.h helpers"),
    (re.compile(r"->register_action_response_callback\s*\("), "register action callbacks through button_grid_ha.h helpers"),
)
STATE_HELPER_PATTERN = re.compile(
    r"inline\s+bool\s+ha_subscribe_state\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
    re.DOTALL,
)
ATTRIBUTE_HELPER_PATTERN = re.compile(
    r"inline\s+bool\s+ha_subscribe_attribute\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
    re.DOTALL,
)
TODO_GET_ITEMS_HELPER_PATTERN = re.compile(
    r"inline\s+bool\s+todo_begin_get_items_request\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
    re.DOTALL,
)


def firmware_ha_binding_errors(firmware_dir: Path, root: Path) -> list[str]:
    errors: list[str] = []
    for path in sorted(firmware_dir.glob("button_grid*.h")):
        if path.name in HA_BOUNDARY_ALLOWLIST:
            continue
        for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            for pattern, message in DIRECT_HA_PATTERNS:
                if pattern.search(line):
                    rel = path.relative_to(root)
                    errors.append(f"{rel}:{line_no}: {message}")
                    break
    return errors


def firmware_ha_boundary_errors(firmware_dir: Path, root: Path) -> list[str]:
    path = firmware_dir / "button_grid_ha.h"
    if not path.exists():
        return []
    rel = path.relative_to(root)
    text = path.read_text(encoding="utf-8")
    errors: list[str] = []

    state_helper = STATE_HELPER_PATTERN.search(text)
    if not state_helper:
        errors.append(f"{rel}: missing ha_subscribe_state helper")
    elif "heap_available" in state_helper.group("body"):
        errors.append(f"{rel}: keep core HA state subscriptions off the low-heap guard")

    attribute_helper = ATTRIBUTE_HELPER_PATTERN.search(text)
    if not attribute_helper:
        errors.append(f"{rel}: missing ha_subscribe_attribute helper")
    elif "heap_available" in attribute_helper.group("body"):
        errors.append(f"{rel}: keep HA metadata attribute subscriptions off the low-heap guard")

    return errors


def firmware_todo_request_errors(firmware_dir: Path, root: Path) -> list[str]:
    path = firmware_dir / "button_grid_todo.h"
    if not path.exists():
        return []
    rel = path.relative_to(root)
    text = path.read_text(encoding="utf-8")
    errors: list[str] = []

    helper = TODO_GET_ITEMS_HELPER_PATTERN.search(text)
    if not helper:
        errors.append(f"{rel}: missing todo_begin_get_items_request helper")
        return errors

    body = helper.group("body")
    if '"todo.get_items"' not in body:
        errors.append(f"{rel}: todo_begin_get_items_request must call todo.get_items")
    if "wants_response" not in body or "response_template" not in body:
        errors.append(f"{rel}: todo.get_items requests must capture a compact response template")
    if 'ha_action_add_data(req, "status"' in body:
        errors.append(f"{rel}: filter todo items in the response template, not in action data")
    return errors


def run_scan() -> int:
    errors = firmware_ha_binding_errors(FIRMWARE_DIR, ROOT)
    errors.extend(firmware_ha_boundary_errors(FIRMWARE_DIR, ROOT))
    errors.extend(firmware_todo_request_errors(FIRMWARE_DIR, ROOT))
    if errors:
        print("Firmware Home Assistant binding check failed:")
        for error in errors:
            print(f"  {error}")
        return 1
    print("Firmware Home Assistant binding checks passed.")
    return 0


def expect_errors(name: str, files: dict[str, str], expected: tuple[str, ...]) -> None:
    with TemporaryDirectory() as tmp:
        root = Path(tmp)
        firmware_dir = root / "components" / "espcontrol"
        firmware_dir.mkdir(parents=True)
        for filename, text in files.items():
            (firmware_dir / filename).write_text(text, encoding="utf-8")

        errors = firmware_ha_binding_errors(firmware_dir, root)
        for item in expected:
            assert any(item in error for error in errors), f"{name}: missing {item!r} in {errors!r}"
        if not expected:
            assert not errors, f"{name}: expected no errors, got {errors!r}"


def run_self_test() -> int:
    expect_errors(
        "direct action send",
        {"button_grid_actions.h": "esphome::api::global_api_server->send_homeassistant_action(req);\n"},
        ("access Home Assistant API through button_grid_ha.h helpers",),
    )
    expect_errors(
        "direct state subscription",
        {"button_grid_media.h": "api->subscribe_home_assistant_state(entity, {}, cb);\n"},
        ("subscribe to Home Assistant state through button_grid_ha.h helpers",),
    )
    expect_errors(
        "direct callback registration",
        {"button_grid_alarm.h": "api->register_action_response_callback(id, cb);\n"},
        ("register action callbacks through button_grid_ha.h helpers",),
    )
    expect_errors(
        "helper boundary",
        {"button_grid_ha.h": "esphome::api::global_api_server->send_homeassistant_action(req);\n"},
        (),
    )
    expect_errors(
        "helper use",
        {"button_grid_media.h": "ha_subscribe_state(entity, cb);\n"},
        (),
    )
    with TemporaryDirectory() as tmp:
        root = Path(tmp)
        firmware_dir = root / "components" / "espcontrol"
        firmware_dir.mkdir(parents=True)
        (firmware_dir / "button_grid_todo.h").write_text(
            'inline bool todo_begin_get_items_request() {\n'
            '  ha_action_begin(req, "todo.get_items", false, 1, call_id);\n'
            '  ha_action_add_entity(req, ctx->entity_id);\n'
            '  return true;\n'
            '}\n',
            encoding="utf-8",
        )
        errors = firmware_todo_request_errors(firmware_dir, root)
        assert any("must capture a compact response template" in error for error in errors), errors
    with TemporaryDirectory() as tmp:
        root = Path(tmp)
        firmware_dir = root / "components" / "espcontrol"
        firmware_dir.mkdir(parents=True)
        (firmware_dir / "button_grid_todo.h").write_text(
            'inline bool todo_begin_get_items_request() {\n'
            '  ha_action_begin(req, "todo.get_items", false, 2, call_id);\n'
            '  req.wants_response = true;\n'
            '  req.response_template = response_template;\n'
            '  ha_action_add_entity(req, ctx->entity_id);\n'
            '  ha_action_add_data(req, "status", "needs_action");\n'
            '  return true;\n'
            '}\n',
            encoding="utf-8",
        )
        errors = firmware_todo_request_errors(firmware_dir, root)
        assert any("filter todo items in the response template" in error for error in errors), errors
    print("Firmware Home Assistant binding self-tests passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true", help="run guardrail self-tests")
    args = parser.parse_args()
    return run_self_test() if args.self_test else run_scan()


if __name__ == "__main__":
    raise SystemExit(main())
