// Local display card: toggles screen lock on the device without Home Assistant.
var SCREEN_LOCK_CARD_METADATA = {
  icon: {
    pickerIdSuffix: "icon-picker",
    idSuffix: "icon",
    field: "icon",
    fallback: "Lock",
  },
  labelField: {
    label: "Label",
    idSuffix: "label",
    field: "label",
    placeholder: "e.g. Screen Lock",
    rerender: true,
  },
  preview: {
    badge: "lock",
  },
};

registerButtonType("screen_lock", {
  label: function () { return cardContractCardLabel("screen_lock"); },
  allowInSubpage: function () { return cardContractAllowInSubpage("screen_lock"); },
  pickerKey: function () { return cardContractPickerKey("screen_lock"); },
  experimental: function () { return cardContractExperimental("screen_lock"); },
  hidden: function () { return cardContractHidden("screen_lock"); },
  labelPlaceholder: "e.g. Screen Lock",
  defaultConfig: function () { return cardContractDefaultConfig("screen_lock"); },
  cardMetadata: SCREEN_LOCK_CARD_METADATA,
  onSelect: function (b) {
    b.entity = "";
    b.sensor = "";
    b.unit = "";
    b.precision = "";
    b.options = "";
    b.icon = b.icon && b.icon !== "Auto" ? b.icon : "Lock";
    b.icon_on = b.icon_on && b.icon_on !== "Auto" ? b.icon_on : "Lock Open";
  },
  renderSettings: function (panel, b, slot, helpers) {
    helpers.renderCardTextField(panel, b, helpers, SCREEN_LOCK_CARD_METADATA.labelField);
    helpers.renderCardIconPicker(panel, b, helpers, Object.assign({}, SCREEN_LOCK_CARD_METADATA.icon, {
      value: b.icon && b.icon !== "Auto" ? b.icon : "Lock",
      fallback: "Lock",
      label: "Locked Icon",
    }));
    helpers.renderCardIconPicker(panel, b, helpers, {
      pickerIdSuffix: "icon-on-picker",
      idSuffix: "icon-on",
      field: "icon_on",
      value: b.icon_on && b.icon_on !== "Auto" ? b.icon_on : "Lock Open",
      fallback: "Lock Open",
      label: "Unlocked Icon",
    });
  },
  renderPreview: function (b, helpers) {
    return cardBadgePreview(b, helpers, {
      label: b.label || "Screen Lock",
      iconFallback: b.icon && b.icon !== "Auto" ? b.icon : "Lock",
      badge: SCREEN_LOCK_CARD_METADATA.preview.badge,
    });
  },
});
