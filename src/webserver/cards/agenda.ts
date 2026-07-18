import { liveGlobal, type GlobalDescriptors } from "../runtime/globals";
export function registerAgendaCardTypes(): GlobalDescriptors {
    // Read-only agenda card: shows the next few upcoming events from one or
    // more Home Assistant calendars, grouped by day.
    var AGENDA_CARD_METADATA: any = {
        entity: {
            label: "Calendar Entities",
            idSuffix: "entity",
            placeholder: "e.g. calendar.family, calendar.work",
            domains: function (this: any) { return cardContractDomains("agenda"); },
            bindName: "entity",
            rerender: true,
            requiredMessage: "Add at least one calendar entity before saving.",
        },
        labelField: {
            label: "Label",
            idSuffix: "label",
            placeholder: "e.g. Agenda",
        },
        preview: {
            badge: "calendar-clock",
        },
    };
    registerButtonType("agenda", {
        label: function (this: any) { return cardContractCardLabel("agenda"); },
        allowInSubpage: function (this: any) { return cardContractAllowInSubpage("agenda"); },
        pickerKey: function (this: any) { return cardContractPickerKey("agenda"); },
        hidden: function (this: any) { return cardContractHidden("agenda"); },
        hideLabel: true,
        defaultConfig: function (this: any) { return cardContractDefaultConfig("agenda"); },
        cardMetadata: AGENDA_CARD_METADATA,
        onSelect: function (this: any, b?: any) {
            b.icon = "Auto";
            b.icon_on = "Auto";
            b.sensor = "";
            b.unit = "";
            b.precision = "";
            b.options = "";
        },
        renderSettings: function (this: any, panel?: any, b?: any, slot?: any, helpers?: any) {
            helpers.renderCardEntityField(panel, b, helpers, AGENDA_CARD_METADATA);
            helpers.renderCardTextField(panel, b, helpers, AGENDA_CARD_METADATA.labelField);
        },
        renderPreview: function (this: any, b?: any, helpers?: any) {
            var label: any = b.label || "Agenda";
            return {
                iconHtml: '<span class="sp-btn-icon mdi mdi-calendar-clock"></span>',
                labelHtml: cardBadgeLabelHtml(helpers, label, AGENDA_CARD_METADATA.preview.badge),
            };
        },
    });
    return {
        "AGENDA_CARD_METADATA": liveGlobal(() => AGENDA_CARD_METADATA, (value?: any) => { AGENDA_CARD_METADATA = value; }),
    };
}
