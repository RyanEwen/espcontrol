#!/usr/bin/env node
"use strict";

const fs = require("fs");
const http = require("http");
const path = require("path");
const { chromium } = require("playwright");
const { loadBundledWebSource } = require("./web_source");

const ROOT = path.resolve(__dirname, "..");
const IMAGES_DIR = path.join(ROOT, "docs", "public", "images");

function installDocsHarness(source) {
  const marker = "\n})();";
  const idx = source.lastIndexOf(marker);
  if (idx < 0) throw new Error("Could not find end of webserver bundle");
  const harness = `

  if (typeof globalThis !== "undefined") {
    function docsBlankButton() {
      return { entity: "", label: "", icon: "Auto", icon_on: "Auto", sensor: "", unit: "", type: "", precision: "", options: "" };
    }

    function docsButtonConfig(button) {
      var out = docsBlankButton();
      button = button || {};
      for (var key in out) {
        if (Object.prototype.hasOwnProperty.call(button, key)) out[key] = button[key];
      }
      return out;
    }

    function docsApplyConfig(config) {
      config = config || {};
      setConfigLocked(false);
      state.onColor = config.onColor || "FF8C00";
      state.offColor = config.offColor || "313131";
      state.sensorColor = config.sensorColor || "212121";
      state.clockBarOn = config.clockBarOn !== false;
      state.networkStatusOn = config.networkStatusOn !== false;
      state.temperatureDegreeSymbolOn = true;
      state.temperatureUnit = normalizeTemperatureUnit(config.temperatureUnit || "°C");
      state.timezone = config.timezone || "Europe/London (GMT+0)";
      state.timezoneOptions = ["Europe/London (GMT+0)", "UTC (GMT+0)", "America/New_York (GMT-5)"];
      state.clockFormat = config.clockFormat || "24h";
      state.firmwareVersion = config.firmwareVersion || "2026.5.0";
      state.firmwareLatestVersion = state.firmwareVersion;
      state.scheduleEnabled = config.scheduleEnabled != null ? !!config.scheduleEnabled : true;
      state.scheduleMode = config.scheduleMode || "clock";
      state.scheduleOnHour = 7;
      state.scheduleOffHour = 22;
      state.brightnessDayVal = 85;
      state.brightnessNightVal = 30;
      state.screensaverMode = "timer";
      state.screensaverAction = "clock";
      state.screensaverTimeout = 300;
      state.homeScreenTimeout = 60;
      state.developerExperimentalFeatures = true;
      state.grid = [];
      state.sizes = {};
      state.buttons = [];
      for (var i = 0; i < NUM_SLOTS; i++) {
        state.grid.push(0);
        state.buttons.push(docsBlankButton());
      }
      (config.buttons || []).forEach(function (item, index) {
        var slot = item.slot || (index + 1);
        var pos = item.pos != null ? item.pos : index;
        if (slot < 1 || slot > NUM_SLOTS || pos < 0 || pos >= NUM_SLOTS) return;
        state.buttons[slot - 1] = docsButtonConfig(item);
        state.grid[pos] = slot;
        if (item.size && item.size !== 1) state.sizes[slot] = item.size;
      });
      applySpans(state.grid, state.sizes, NUM_SLOTS);
      state.selectedSlots = [];
      state.lastClickedSlot = -1;
      state.editingSubpage = null;
      state.subpageSelectedSlots = [];
      state.subpageLastClicked = -1;
      hideSettingsOverlay();
      syncPreviewOrientation();
      syncClockBarUi();
      syncScreenScheduleUi();
      syncTemperatureUi();
      syncIdleUi();
      syncNtpServerUi();
      syncMonthNameUi();
      renderFirmwareVersion();
      syncFirmwareUpdateUi();
      updateSunInfo();
      updateClock();
      renderPreview();
      renderButtonSettings();
    }

    globalThis.__ESPCONTROL_DOCS__ = {
      applyConfig: docsApplyConfig,
      showScreen: function () {
        switchTab("screen");
        hideSettingsOverlay();
        renderPreview();
      },
      showCardSettings: function (slot) {
        switchTab("screen");
        state.selectedSlots = [slot];
        state.lastClickedSlot = slot;
        renderPreview();
        renderButtonSettings(true);
      },
      showSettingsTab: function () {
        hideSettingsOverlay();
        state.selectedSlots = [];
        state.lastClickedSlot = -1;
        switchTab("settings");
      }
    };
  }
`;
  return source.slice(0, idx) + harness + source.slice(idx);
}

function entityResponse(reqPath) {
  const cleanPath = decodeURIComponent(reqPath.split("?")[0]);
  const parts = cleanPath.split("/").filter(Boolean);
  const domain = parts[0] || "text";
  const name = parts.slice(1).join("/") || "";
  const id = domain + "-" + name.toLowerCase().replace(/[^a-z0-9]+/g, "_").replace(/^_+|_+$/g, "");
  if (domain === "switch") return { id, state: "OFF", value: false };
  if (domain === "number") return { id, state: "0", value: 0 };
  if (domain === "select") return { id, state: "", value: "", options: [] };
  if (domain === "button") return { id, state: "", value: "" };
  if (domain === "update") return { id, state: "idle", value: "idle" };
  return { id, state: "", value: "" };
}

function startServer(html) {
  return new Promise((resolve) => {
    const server = http.createServer((req, res) => {
      if (req.url === "/" || req.url.startsWith("/?")) {
        res.writeHead(200, { "content-type": "text/html; charset=utf-8" });
        res.end(html);
        return;
      }
      if (req.method === "POST") {
        res.writeHead(200, { "content-type": "text/plain" });
        res.end("ok");
        return;
      }
      if (req.url.startsWith("/events")) {
        res.writeHead(204);
        res.end();
        return;
      }
      res.writeHead(200, { "content-type": "application/json" });
      res.end(JSON.stringify(entityResponse(req.url)));
    });
    server.listen(0, "127.0.0.1", () => {
      const address = server.address();
      resolve({ server, url: `http://127.0.0.1:${address.port}/` });
    });
  });
}

function button(overrides) {
  return Object.assign({
    entity: "",
    label: "",
    icon: "Auto",
    icon_on: "Auto",
    sensor: "",
    unit: "",
    type: "",
    precision: "",
    options: "",
  }, overrides || {});
}

const galleryButtons = [
  button({ label: "Living Room", entity: "light.living_room", type: "light_brightness", icon: "Lightbulb", icon_on: "Lightbulb On" }),
  button({ label: "Kitchen", entity: "light.kitchen", type: "light_temperature", icon: "Lightbulb", unit: "2200-6500" }),
  button({ label: "Heating", entity: "climate.downstairs", type: "climate", icon: "Thermostat", options: "number_display=target,label_display=status" }),
  button({ label: "Music", entity: "media_player.lounge", type: "media", sensor: "now_playing", precision: "progress", size: 3 }),
  button({ label: "Today", entity: "weather.forecast_home", type: "weather", precision: "today", options: "large_numbers=1" }),
  button({ label: "Blinds", entity: "cover.living_room", type: "cover", icon: "Blinds", icon_on: "Blinds Open" }),
  button({ label: "Garage", entity: "cover.garage", type: "garage", icon: "Garage", icon_on: "Garage Open" }),
  button({ label: "Front Door", entity: "lock.front_door", type: "lock", icon: "Lock", icon_on: "Lock Open" }),
  button({ label: "Scene", entity: "scene.movie_mode", type: "action", icon: "Movie Open Play" }),
  button({ label: "Wind", entity: "sensor.wind_speed", type: "sensor", icon: "Weather Windy", unit: "kph" }),
  button({ label: "Door", entity: "binary_sensor.patio_door", type: "door_window", icon: "Door Closed", icon_on: "Door Open" }),
  button({ label: "Doorbell", type: "push", icon: "Gesture Tap Button" }),
  button({ label: "Alarm", entity: "alarm_control_panel.house", type: "alarm", icon: "Security" }),
  button({ label: "Clock", type: "timezone", icon: "Clock Outline", sensor: "Europe/London (GMT+0)" }),
  button({ label: "Calendar", type: "calendar", icon: "Calendar Month" }),
];
[
  0, 1, 2, 3, 5,
  6, 7, 8, 9, 10,
  11, 12, 13, 14, 15,
].forEach((pos, index) => {
  galleryButtons[index].pos = pos;
});

const singleCardConfigs = {
  "card-slider.png": button({ label: "Lamp", entity: "light.lamp", type: "slider", icon: "Lightbulb", icon_on: "Lightbulb On" }),
  "card-cover.png": button({ label: "Blinds", entity: "cover.lounge_blinds", type: "cover", icon: "Blinds", icon_on: "Blinds Open" }),
  "card-light-brightness.png": button({ label: "Living Room", entity: "light.living_room", type: "light_brightness", icon: "Lightbulb", icon_on: "Lightbulb On" }),
  "card-climate.png": button({ label: "Heating", entity: "climate.downstairs", type: "climate", icon: "Thermostat", options: "number_display=target,label_display=status" }),
  "card-weather.png": button({ label: "Today", entity: "weather.forecast_home", type: "weather", precision: "today", options: "large_numbers=1" }),
  "card-media.png": button({ label: "Music", entity: "media_player.lounge", type: "media", sensor: "now_playing", precision: "progress", size: 3 }),
};

async function screenshot(page, selector, fileName) {
  const target = page.locator(selector).first();
  await target.waitFor({ state: "visible" });
  await target.screenshot({ path: path.join(IMAGES_DIR, fileName) });
}

async function main() {
  fs.mkdirSync(IMAGES_DIR, { recursive: true });
  const source = installDocsHarness(loadBundledWebSource());
  const html = `<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>EspControl docs screenshots</title>
</head>
<body><esp-app></esp-app><script>${source}</script></body>
</html>`;
  const { server, url } = await startServer(html);
  const browser = await chromium.launch();
  try {
    const page = await browser.newPage({ viewport: { width: 1280, height: 900 }, deviceScaleFactor: 2 });
    await page.goto(url, { waitUntil: "networkidle" });
    await page.waitForFunction(() => window.__ESPCONTROL_DOCS__);
    await page.evaluate((buttons) => window.__ESPCONTROL_DOCS__.applyConfig({ buttons }), galleryButtons);
    await page.waitForTimeout(400);
    await screenshot(page, ".sp-screen", "webserver-card-gallery.png");

    for (const [fileName, config] of Object.entries(singleCardConfigs)) {
      await page.setViewportSize({ width: 420, height: 340 });
      await page.evaluate((buttonConfig) => {
        window.__ESPCONTROL_DOCS__.applyConfig({ buttons: [buttonConfig], clockBarOn: false });
      }, config);
      await page.waitForTimeout(200);
      await screenshot(page, '.sp-btn[data-slot="1"]', fileName);
    }

    await page.setViewportSize({ width: 980, height: 900 });
    await page.evaluate(() => {
      window.__ESPCONTROL_DOCS__.applyConfig({
        buttons: [{
          label: "Living Room",
          entity: "light.living_room",
          type: "light_brightness",
          icon: "Lightbulb",
          icon_on: "Lightbulb On"
        }]
      });
      window.__ESPCONTROL_DOCS__.showCardSettings(1);
    });
    await page.waitForTimeout(300);
    await screenshot(page, ".sp-settings-modal", "settings-panel-light-card.png");

    await page.setViewportSize({ width: 1180, height: 900 });
    await page.evaluate(() => {
      window.__ESPCONTROL_DOCS__.applyConfig({ buttons: [] });
      window.__ESPCONTROL_DOCS__.showSettingsTab();
    });
    await page.waitForTimeout(300);
    await screenshot(page, "#sp-settings .sp-config", "settings-tab-display.png");
  } finally {
    await browser.close();
    server.close();
  }
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
