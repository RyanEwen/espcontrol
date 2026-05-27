// Experimental Home Assistant todo card.
var TODO_CARD_METADATA = {
  entity: {
    label: "Todo Entity",
    idSuffix: "todo-entity",
    placeholder: "e.g. todo.shopping",
    domains: function () { return cardContractDomains("todo"); },
    bindName: "entity",
    rerender: true,
    requiredMessage: "Add a todo entity before saving.",
  },
  labelField: {
    label: "Label",
    idSuffix: "todo-label",
    field: "label",
    placeholder: "e.g. Shopping",
    rerender: true,
  },
  icon: {
    pickerIdSuffix: "todo-icon-picker",
    idSuffix: "todo-icon",
    field: "icon",
    fallback: "Check",
  },
  countDisplay: {
    label: "Show Item Count",
    idSuffix: "todo-show-count",
    checked: function (b) { return todoCardShowCount(b); },
    onChange: function (button, cardHelpers, checked) {
      setTodoCardShowCount(button, checked);
      cardHelpers.saveField("options", button.options);
    },
  },
  labelDisplay: {
    label: "Label Shows Item Count",
    idSuffix: "todo-label-count",
    checked: function (b) { return todoCardLabelShowsCount(b); },
    onChange: function (button, cardHelpers, checked) {
      setTodoCardLabelShowsCount(button, checked);
      cardHelpers.saveField("options", button.options);
    },
  },
  preview: {
    badge: "check",
  },
};

function normalizeTodoConfig(b) {
  if (!b) return;
  b.sensor = "";
  b.unit = "";
  b.precision = "";
  b.options = normalizeTodoOptions(b.options);
  b.icon_on = "Auto";
  if (!b.icon || b.icon === "Auto") b.icon = "Check";
}

registerButtonType("todo", {
  label: function () { return cardContractCardLabel("todo"); },
  allowInSubpage: function () { return cardContractAllowInSubpage("todo"); },
  pickerKey: function () { return cardContractPickerKey("todo"); },
  experimental: function () { return cardContractExperimental("todo"); },
  hidden: function () { return cardContractHidden("todo"); },
  hideLabel: true,
  defaultConfig: function () { return cardContractDefaultConfig("todo"); },
  cardMetadata: TODO_CARD_METADATA,
  onSelect: function (b) {
    b.entity = "";
    b.label = "";
    b.icon = "Check";
    normalizeTodoConfig(b);
  },
  renderSettings: function (panel, b, slot, helpers) {
    normalizeTodoConfig(b);
    helpers.renderCardEntityField(panel, b, helpers, TODO_CARD_METADATA);
    var labelControl = helpers.renderCardTextField(panel, b, helpers, TODO_CARD_METADATA.labelField);
    var labelDisplayToggle = helpers.renderCardOptionToggle(panel, b, helpers, Object.assign({}, TODO_CARD_METADATA.labelDisplay, {
      onChange: function (button, cardHelpers, checked) {
        setTodoCardLabelShowsCount(button, checked);
        cardHelpers.saveField("options", button.options);
        syncLabelField();
        scheduleRender();
      },
    }));
    var iconPicker = helpers.renderCardIconPicker(panel, b, helpers, TODO_CARD_METADATA.icon);
    var countToggle = helpers.renderCardOptionToggle(panel, b, helpers, Object.assign({}, TODO_CARD_METADATA.countDisplay, {
      onChange: function (button, cardHelpers, checked) {
        setTodoCardShowCount(button, checked);
        cardHelpers.saveField("options", button.options);
        syncIconPicker();
        scheduleRender();
      },
    }));
    function syncLabelField() {
      labelControl.field.style.display = todoCardLabelShowsCount(b) ? "none" : "";
      labelDisplayToggle.input.checked = todoCardLabelShowsCount(b);
    }
    function syncIconPicker() {
      iconPicker.style.display = todoCardShowCount(b) ? "none" : "";
      countToggle.input.checked = todoCardShowCount(b);
    }
    syncLabelField();
    syncIconPicker();
  },
  renderPreview: function (b, helpers) {
    var label = todoCardLabelShowsCount(b) ? "3 items" : (b.label || b.entity || "Todo");
    return {
      iconHtml: todoCardShowCount(b)
        ? cardSensorPreviewHtml(b, helpers, "3", "")
        : '<span class="sp-btn-icon mdi mdi-' + iconSlug(b.icon || "Check") + '"></span>',
      labelHtml: cardBadgeLabelHtml(helpers, label, TODO_CARD_METADATA.preview.badge),
    };
  },
});
