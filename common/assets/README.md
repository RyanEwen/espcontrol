# Assets

This directory contains internal firmware assets for fonts, icons, and glyph
sets. It is repository documentation for maintainers and is not part of the
public docs site.

## Font naming and ramp

Font component IDs are generic style names. New firmware features should reuse
these names directly instead of adding size- or font-family-based IDs:

| Font ID | Use |
| --- | --- |
| `font_icon_main` | Main icons, setup icons, modal icons, and climate card icons |
| `font_icon_status` | Small status and subpage icons |
| `font_text_body` | Labels, setup body text, modal labels, climate option text, and general UI text |
| `font_text_title` | Media titles, setup headings, and larger text headings |
| `font_number_value` | Normal sensor values |
| `font_number_value_large` | Large sensor values |
| `font_number_modal` | Large modal numbers |
| `font_number_clock` | Screensaver clock |

Each device implements those IDs at the physical size that fits its panel in
`devices/*/device/fonts.yaml`. Shared setup screens and generated device files
then refer to the generic names, so adding a feature does not require knowing
each device's pixel sizes.

### Shared ratios

There are repeated ratios that should be preserved when adding or adjusting a
device:

| Relationship | Current rule |
| --- | --- |
| Large sensor numbers | `2.5x` the normal sensor value font |
| Status icons | Small enough for top-bar and subpage indicators |
| 720x720 square panel | Mostly `1.5x` the 480x480 square panel |

Examples of the current ratios:

| Device class | Main icon | Status icon | Sensor | Large sensor |
| --- | ---: | ---: | ---: | ---: |
| 480x480 square | 44 | 19 | 44 | 110 |
| 720x720 square | 66 | 26 | 66 | 165 |
| 1024x600 landscape | 55 | 19 | 55 | 138 |
| 480x800 portrait | 62 | 23 | 62 | 155 |
| 1280x800 landscape | 56 | 21 | 52 | 130 |

### Current role assignments

The firmware role names in `devices/manifest.json` are still usage-based. They
map UI roles to the generic font IDs:

| Role | Purpose |
| --- | --- |
| `icon` | Main card icon |
| `climateCardIcon` | Climate card icon, using the main icon style |
| `subpageChevron` | Subpage marker, using the status icon style |
| `sensor` | Normal sensor/statistic value |
| `largeSensor` | Large-number sensor card value |
| `mediaTitle` | Media card title text |
| `volumeNumber` | Large modal numeric value |
| `volumeLabel` | Modal label text |
| `climateOptionTitle` | Climate option chip title |
| `climateOptionValue` | Climate option chip value |

When changing the ramp, update the device font sizes and the manifest together,
then run:

```sh
python3 scripts/generate_device_slots.py
npm run check:all
```

## How to add an icon

All button icons are defined once in [`icons.json`](icons.json) and synced to the device font list, firmware lookup table, and web UI by a script. Never edit generated icon lists directly.

## 1. Find the icon on MDI

Browse [Material Design Icons](https://materialdesignicons.com/) and note three things:

| Field | Example | Where to find it |
|-------|---------|-------------------|
| **name** | `Ceiling Fan` | Choose a user-friendly display name |
| **codepoint** | `F1797` | Shown on the icon detail page (hex, no `0x` prefix) |
| **mdi** | `ceiling-fan` | The MDI class name (used as `mdi-ceiling-fan` in CSS) |

## 2. Add the entry to `icons.json`

Open `common/assets/icons.json` and add an object to the `"icons"` array:

```json
{ "name": "Ceiling Fan", "codepoint": "F1797", "mdi": "ceiling-fan" }
```

The array order determines display order in the LVGL font glyph list and the C++ lookup table. The YAML select options and JS picker sort alphabetically, so position doesn't matter for those.

## 3. Run the sync script

```sh
python3 scripts/build.py icons
```

This patches the generated icon sections in:

- `common/assets/icon_glyphs.yaml` — LVGL font glyph codepoints
- `components/espcontrol/icons.h` — C++ icon lookup table and domain defaults
- `src/webserver/entry.js` — web UI icon picker names and domain defaults

Run `python3 scripts/build.py` to also rebuild the generated per-device web UI bundles under `docs/public/webserver/.../www.js`.

## 4. Verify

```sh
python3 scripts/build.py icons --check
```

Exits 0 if everything is in sync. The check also compares each `icons.json` codepoint with the pinned Material Design Icons release, so the browser preview and device font cannot silently drift apart.

## Domain defaults

To change which icon is used when a button's icon is set to "Auto", edit the `"domain_defaults"` object in `icons.json`. Values must reference an icon `name` from the `"icons"` array.
