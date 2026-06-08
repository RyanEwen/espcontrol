// Read-only Home Assistant camera image card.
var IMAGE_CARD_METADATA = {
  entity: {
    label: "Camera Entity",
    idSuffix: "entity",
    placeholder: "e.g. camera.front_door",
    domains: function () { return cardContractDomains("image"); },
    bindName: "entity",
    rerender: true,
    requiredMessage: "Add a camera entity before saving.",
  },
};

function imageRefreshIntervalOptions() {
  return [
    ["off", "Off"],
    ["10", "10 seconds"],
    ["30", "30 seconds"],
    ["60", "1 minute"],
    ["300", "5 minutes"],
  ];
}

function imageRefreshModeOptions() {
  return [
    ["changes_timer", "Changes + interval"],
    ["timer", "Interval only"],
  ];
}

function renderImageRefreshSettings(panel, b, helpers) {
  var intervalField = helpers.selectField(
    "Refresh Interval",
    helpers.idPrefix + "image-refresh",
    imageRefreshIntervalOptions(),
    imageRefreshInterval(b)
  );
  panel.appendChild(intervalField.field);

  var modeField = helpers.selectField(
    "Refresh Mode",
    helpers.idPrefix + "image-refresh-mode",
    imageRefreshModeOptions(),
    imageRefreshMode(b)
  );
  panel.appendChild(modeField.field);

  function syncModeVisibility() {
    modeField.field.hidden = imageRefreshInterval(b) === "off";
    modeField.select.value = imageRefreshMode(b);
  }

  intervalField.select.addEventListener("change", function () {
    setImageRefreshInterval(b, this.value);
    helpers.saveField("options", b.options);
    syncModeVisibility();
  });
  modeField.select.addEventListener("change", function () {
    setImageRefreshMode(b, this.value);
    helpers.saveField("options", b.options);
  });
  syncModeVisibility();
}

registerButtonType("image", {
  label: function () { return cardContractCardLabel("image"); },
  allowInSubpage: function () { return cardContractAllowInSubpage("image"); },
  pickerKey: function () { return cardContractPickerKey("image"); },
  experimental: function () { return cardContractExperimental("image"); },
  hidden: function () { return cardContractHidden("image"); },
  hideLabel: true,
  defaultConfig: function () { return cardContractDefaultConfig("image"); },
  cardMetadata: IMAGE_CARD_METADATA,
  onSelect: function (b) {
    b.label = "";
    b.icon = "Auto";
    b.icon_on = "Auto";
    b.sensor = "";
    b.unit = "";
    b.precision = "";
    b.options = normalizeImageOptions(b.options);
  },
  renderSettings: function (panel, b, slot, helpers) {
    b.label = "";
    b.icon = "Auto";
    b.icon_on = "Auto";
    b.sensor = "";
    b.unit = "";
    b.precision = "";
    b.options = normalizeImageOptions(b.options);
    helpers.renderCardEntityField(panel, b, helpers, IMAGE_CARD_METADATA);
    renderImageRefreshSettings(panel, b, helpers);
  },
  renderPreview: function () {
    return {
      buttonClass: "sp-image-card",
      iconHtml:
        '<span class="sp-image-preview">' +
        '<span class="sp-image-preview-sky"></span>' +
        '<span class="sp-image-preview-ground"></span>' +
        '<span class="sp-image-preview-shape"></span>' +
        '</span>',
      labelHtml: "",
    };
  },
});
