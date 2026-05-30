// ── Context abstraction ────────────────────────────────────────────────

function ctx() {
  if (state.editingSubpage) {
    var sp = getSubpage(state.editingSubpage);
    return {
      grid: sp.grid, sizes: sp.sizes, buttons: sp.buttons,
      maxSlots: NUM_SLOTS, selected: state.subpageSelectedSlots,
      isSub: true,
      setSelected: function (s) { state.subpageSelectedSlots = s; },
      setLastClicked: function (s) { state.subpageLastClicked = s; },
      getLastClicked: function () { return state.subpageLastClicked; },
      save: function () { saveSubpageConfig(state.editingSubpage); },
    };
  }
  if (PAGE_COUNT > 1) {
    var start = state.dashboardPage * NUM_SLOTS;
    var pageGrid = [];
    for (var i = 0; i < NUM_SLOTS; i++) {
      pageGrid.push(start + i + 1);
    }
    return {
      grid: pageGrid, sizes: state.sizes, buttons: state.buttons,
      maxSlots: NUM_SLOTS, selected: state.selectedSlots,
      isSub: false,
      setSelected: function (s) { state.selectedSlots = s; },
      setLastClicked: function (s) { state.lastClickedSlot = s; },
      getLastClicked: function () { return state.lastClickedSlot; },
      save: function () {},
    };
  }
  return {
    grid: state.grid, sizes: state.sizes, buttons: state.buttons,
    maxSlots: NUM_SLOTS, selected: state.selectedSlots,
    isSub: false,
    setSelected: function (s) { state.selectedSlots = s; },
    setLastClicked: function (s) { state.lastClickedSlot = s; },
    getLastClicked: function () { return state.lastClickedSlot; },
    save: function () { postText(entityName("button_order"), serializeGrid(state.grid)); },
  };
}

function setDashboardPage(page) {
  if (PAGE_COUNT <= 1) return;
  page = Math.max(0, Math.min(PAGE_COUNT - 1, parseInt(page, 10) || 0));
  if (state.dashboardPage === page) return;
  state.dashboardPage = page;
  state.selectedSlots = [];
  state.lastClickedSlot = -1;
  state.settingsDraft = null;
  syncDashboardPageControls();
  renderPreview();
  renderButtonSettings();
}

function syncDashboardPageControls() {
  if (els.epaperPage) {
    els.epaperPage.textContent = "Page " + (state.dashboardPage + 1) + "/" + PAGE_COUNT;
  }
  if (!els.dashboardPages) return;
  els.dashboardPages.querySelectorAll(".sp-page-btn").forEach(function (btn) {
    var page = parseInt(btn.getAttribute("data-page"), 10) || 0;
    btn.className = "sp-page-btn" + (page === state.dashboardPage ? " active" : "");
  });
}

// ── Grid helpers ───────────────────────────────────────────────────────

function sizeFromToken(token) {
  return EspControlModel.sizeFromToken(token);
}

function sizeToken(size) {
  return EspControlModel.sizeToken(size);
}

function sizeRowSpan(size) {
  return EspControlModel.sizeRowSpan(size);
}

function sizeColSpan(size) {
  return EspControlModel.sizeColSpan(size);
}

function sizeClass(size) {
  return size === 4 ? " sp-btn-big" : size === 2 ? " sp-btn-double" :
    size === 3 ? " sp-btn-wide" : size === 5 ? " sp-btn-extra-tall" :
    size === 6 ? " sp-btn-extra-wide" : "";
}

function coveredCells(pos, size, maxSlots, includeOrigin) {
  return EspControlModel.coveredCells(pos, size, maxSlots, GRID_COLS, includeOrigin);
}

function sizeFitsAt(pos, size, maxSlots) {
  return EspControlModel.sizeFitsAt(pos, size, maxSlots, GRID_COLS);
}

function markSpannedCells(grid, pos, size, maxSlots) {
  EspControlModel.markSpannedCells(grid, pos, size, maxSlots, GRID_COLS);
}

function parseOrder(str) {
  var parsed = EspControlModel.parseGridOrder(str, NUM_SLOTS, GRID_COLS, state.sizes);
  state.sizes = parsed.sizes;
  return parsed.grid;
}

function applySpans(grid, sizes, maxSlots) {
  EspControlModel.applySpans(grid, sizes, maxSlots, GRID_COLS);
}

function serializeGrid(grid) {
  return EspControlModel.serializeGridOrder(grid, state.sizes);
}

function applyImportedButtonOrder(orderStr, importedSizes) {
  state.sizes = importedSizes || {};
  state.grid = parseOrder(orderStr);
}

function clearSpans(grid, maxSlots) {
  EspControlModel.clearSpans(grid, maxSlots);
}


function resolveIcon(b) {
  var sel = b.icon || "Auto";
  if (sel === "Auto" && b.entity) {
    var domain = b.entity.split(".")[0];
    return DOMAIN_ICONS[domain] || "cog";
  }
  return iconSlug(sel);
}

function btnDisplayName(b) {
  return b.label || b.entity || "Configure";
}
