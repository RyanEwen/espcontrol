---
name: flash-displays
description: Flash EspControl display firmware from this repository using ESPHome. Use when the user asks to flash, reflash, update, or upload firmware to all known displays in sequence, or to a specific display such as 7inch, 7-inch P4, 10inch, 10-inch P4, 4-inch P4, or 4-inch S3.
---

# Flash Displays

## Overview

Use the local development ESPHome configs to flash the known EspControl displays. Flash one requested display, or flash all displays in the fixed order below.

## Device Map

| Request names | ESPHome config directory | Flash target |
|---|---|---|
| `7inch`, `7-inch`, `7inch P4`, `7-inch P4`, `JC1060P470` | `devices/guition-esp32-p4-jc1060p470` | OTA at `192.168.6.102`; USB fallback `/dev/cu.usbmodem201301` when explicitly requested |
| `10inch`, `10-inch`, `10inch P4`, `10-inch P4`, `JC8012P4A1` | `devices/guition-esp32-p4-jc8012p4a1` | OTA at `192.168.6.103` |
| `4inch P4`, `4-inch P4`, `4.3inch P4`, `4.3-inch P4`, `JC4880P443` | `devices/guition-esp32-p4-jc4880p443` | OTA at `192.168.10.52` |
| `4inch S3`, `4-inch S3`, `4848S040` | `devices/guition-esp32-s3-4848s040` | OTA at `192.168.10.226` |

If the user says only `4inch` or `4-inch`, ask whether they mean the P4 screen or the S3 screen.

For `all`, flash in this sequence:

1. 7-inch P4 over OTA to `192.168.6.102`.
2. 10-inch P4 over OTA to `192.168.6.103`.
3. 4-inch P4 over OTA to `192.168.10.52`.
4. 4-inch S3 over OTA to `192.168.10.226`.

## Workflow

1. Confirm the repository state:
   - Run `git status --short --branch`.
   - Use `main` as the source. If not on `main`, switch only when it is safe and there are no blocking local changes; otherwise explain the issue.
   - If the worktree is dirty, do not revert or commit unrelated changes. Tell the user the flash will use the current local checkout as-is.
   - If the worktree is clean, run `git pull --ff-only` before flashing.
2. Resolve the requested display names from the device map. If the request is ambiguous, ask one short clarification.
3. For OTA targets, check reachability first with `ping -c 2 -W 1000 <ip>`.
4. For USB flashing, only when explicitly requested or when using the documented 7-inch P4 USB fallback:
   - List ports with `ls -1 /dev/cu.*`.
   - Prefer `/dev/cu.usbmodem201301` when present.
   - If that port is missing and exactly one obvious `/dev/cu.usbmodem*` port exists, use it.
   - If no clear USB modem port exists, ask the user to connect the display or choose the port.
5. Flash each selected display with the command below, running displays sequentially. Do not run multiple flashes in parallel.
6. After each OTA flash, ping the IP again. A first ping may fail during reboot; retry once after a short delay before reporting a problem.
7. Do not commit or push for flashing alone. Commit/push only if this skill or other source files were intentionally changed as part of the user request.

## Commands

Use this substitution so ESPHome builds from the local repository checkout:

```bash
esphome -s espcontrol_component_url file:///Users/jtenniswood/Git/espcontrol run dev.yaml --device <target> --no-logs
```

Run from the appropriate config directory:

```bash
# 7-inch P4 over IP
cd /Users/jtenniswood/Git/espcontrol/devices/guition-esp32-p4-jc1060p470
esphome -s espcontrol_component_url file:///Users/jtenniswood/Git/espcontrol run dev.yaml --device 192.168.6.102 --no-logs

# 7-inch P4 over USB, only when explicitly requested or needed as fallback
cd /Users/jtenniswood/Git/espcontrol/devices/guition-esp32-p4-jc1060p470
esphome -s espcontrol_component_url file:///Users/jtenniswood/Git/espcontrol run dev.yaml --device /dev/cu.usbmodem201301 --no-logs

# 10-inch P4 over IP
cd /Users/jtenniswood/Git/espcontrol/devices/guition-esp32-p4-jc8012p4a1
esphome -s espcontrol_component_url file:///Users/jtenniswood/Git/espcontrol run dev.yaml --device 192.168.6.103 --no-logs

# 4-inch P4 over IP
cd /Users/jtenniswood/Git/espcontrol/devices/guition-esp32-p4-jc4880p443
esphome -s espcontrol_component_url file:///Users/jtenniswood/Git/espcontrol run dev.yaml --device 192.168.10.52 --no-logs

# 4-inch S3 over IP
cd /Users/jtenniswood/Git/espcontrol/devices/guition-esp32-s3-4848s040
esphome -s espcontrol_component_url file:///Users/jtenniswood/Git/espcontrol run dev.yaml --device 192.168.10.226 --no-logs
```

## Reporting

Keep user updates concise:

- Say which display is currently compiling/uploading.
- Mention known ESPHome warnings only if they affect the result; framework, platform, GPIO19/GPIO20, and MIPI narrowing warnings are normally non-blocking.
- Final response: list each requested display as flashed successfully, or clearly identify the display that failed and the blocking symptom.
