import { staticGlobal, type GlobalDescriptors } from "../runtime/globals";
export function installHaEntityFetchModule(): GlobalDescriptors {
    // ── Home Assistant entity suggestions ─────────────────────────────────
    // Connects directly to Home Assistant's WebSocket API using the address and
    // long-lived token stored on the device, fetches the list of entities, and
    // feeds them into the shared entity-suggestion store so every entity box
    // can autocomplete against the user's real Home Assistant entities.
    var HA_REFRESH_DEBOUNCE_MS: any = 600;
    var HA_CONNECT_TIMEOUT_MS: any = 10000;
    // Re-fetch the entity list at most this often when an entity box is focused,
    // so entities created in Home Assistant show up without reloading the page.
    var HA_FOCUS_REFRESH_MIN_AGE_MS: any = 8000;
    var haRefreshTimer: any = null;
    var haSocket: any = null;
    var haRequestId: any = 0;
    var haGeneration: any = 0;
    var haLastRefreshAt: any = 0;
    function normalizeHaWebsocketUrl(this: any, raw?: any) {
        var text: any = String(raw || "").trim();
        if (!text)
            return "";
        if (text.indexOf("://") === -1)
            text = "http://" + text;
        var sep: any = text.indexOf("://");
        var scheme: any = text.substring(0, sep).toLowerCase();
        var rest: any = text.substring(sep + 3).replace(/\/+$/, "");
        var slash: any = rest.indexOf("/");
        var host: any = slash === -1 ? rest : rest.substring(0, slash);
        if (!host)
            return "";
        var secure: any = (scheme === "https" || scheme === "wss");
        // Default to Home Assistant's port (8123) when the user gives a bare
        // host/IP over plain http. Secure URLs are left on their default (443),
        // since https usually means a reverse proxy. An explicit port (a colon
        // in the host, including IPv6 literals) is always respected.
        if (!secure && host.indexOf(":") === -1)
            host = host + ":8123";
        var wsScheme: any = secure ? "wss" : "ws";
        return wsScheme + "://" + host + "/api/websocket";
    }
    function syncHaEntityStatus(this: any) {
        var elm: any = els.haConnectionStatus;
        if (!elm)
            return;
        var status: any = state.haConnectionStatus;
        var text: any = "";
        var color: any = "var(--text2)";
        if (!state.haUrl || !state.haToken) {
            text = "Add an address and token above to suggest your Home Assistant entities.";
        }
        else if (status === "connecting") {
            text = "Connecting to Home Assistant…";
        }
        else if (status === "connected") {
            text = "Connected — " + state.haEntityCount + " entit" +
                (state.haEntityCount === 1 ? "y" : "ies") + " available for suggestions.";
            color = "var(--accent)";
        }
        else if (status === "auth") {
            text = "Home Assistant rejected the token. Check the access token and try again.";
            color = "#e05a5a";
        }
        else if (status === "error") {
            text = "Couldn't reach Home Assistant at that address.";
            color = "#e05a5a";
        }
        else {
            text = "Entities load automatically when you open this page.";
        }
        elm.textContent = text;
        elm.style.color = color;
    }
    function setHaConnectionStatus(this: any, status?: any, count?: any) {
        state.haConnectionStatus = status;
        if (typeof count === "number")
            state.haEntityCount = count;
        syncHaEntityStatus();
    }
    function closeHaSocket(this: any) {
        if (haSocket) {
            try {
                haSocket.close();
            }
            catch (_) { }
            haSocket = null;
        }
    }
    function refreshActiveEntityDropdown(this: any) {
        var active: any = document.activeElement;
        if (active && active._entitySuggestionsAttached)
            refreshEntityDatalist(active);
    }
    function applyHaStates(this: any, states?: any) {
        var count: any = 0;
        for (var i: any = 0; i < states.length; i++) {
            var entry: any = states[i];
            var id: any = entry && entry.entity_id;
            if (!id || !parseHomeAssistantEntity(id))
                continue;
            var friendly: any = entry.attributes && entry.attributes.friendly_name;
            rememberEntityName(id, friendly ? String(friendly) : titleFromEntityId(id));
            count++;
        }
        setHaConnectionStatus("connected", count);
        refreshActiveEntityDropdown();
    }
    function refreshHaEntities(this: any) {
        var url: any = normalizeHaWebsocketUrl(state.haUrl);
        var token: any = String(state.haToken || "").trim();
        if (!url || !token) {
            closeHaSocket();
            setHaConnectionStatus("idle");
            return;
        }
        if (typeof WebSocket === "undefined")
            return;
        haLastRefreshAt = Date.now();
        haGeneration++;
        var myGen: any = haGeneration;
        closeHaSocket();
        setHaConnectionStatus("connecting");
        var socket: any;
        try {
            socket = new WebSocket(url);
        }
        catch (_) {
            setHaConnectionStatus("error");
            return;
        }
        haSocket = socket;
        var settled: any = false;
        function finish(this: any, status?: any, count?: any) {
            if (settled)
                return;
            settled = true;
            clearTimeout(timeout);
            if (status)
                setHaConnectionStatus(status, count);
            try {
                socket.close();
            }
            catch (_) { }
        }
        var timeout: any = setTimeout(function (this: any) {
            finish("error");
        }, HA_CONNECT_TIMEOUT_MS);
        socket.onmessage = function (this: any, ev?: any) {
            if (myGen !== haGeneration) {
                try {
                    socket.close();
                }
                catch (_) { }
                return;
            }
            var msg: any;
            try {
                msg = JSON.parse(ev.data);
            }
            catch (_) {
                return;
            }
            if (msg.type === "auth_required") {
                socket.send(JSON.stringify({ type: "auth", access_token: token }));
                return;
            }
            if (msg.type === "auth_invalid") {
                finish("auth");
                return;
            }
            if (msg.type === "auth_ok") {
                haRequestId++;
                socket.send(JSON.stringify({ id: haRequestId, type: "get_states" }));
                return;
            }
            if (msg.type === "result") {
                if (msg.success && msg.result && typeof msg.result.length === "number") {
                    settled = true;
                    clearTimeout(timeout);
                    applyHaStates(msg.result);
                    try {
                        socket.close();
                    }
                    catch (_) { }
                }
                else {
                    finish("error");
                }
            }
        };
        socket.onerror = function (this: any) {
            finish("error");
        };
        socket.onclose = function (this: any) {
            if (haSocket === socket)
                haSocket = null;
            finish("error");
        };
    }
    function scheduleHaEntityRefresh(this: any) {
        if (haRefreshTimer)
            clearTimeout(haRefreshTimer);
        haRefreshTimer = setTimeout(function (this: any) {
            haRefreshTimer = null;
            refreshHaEntities();
        }, HA_REFRESH_DEBOUNCE_MS);
    }
    // Called when an entity box is focused: refresh the list if it hasn't been
    // fetched recently, so newly-created Home Assistant entities appear without
    // a page reload. Skips when unconfigured or a fetch is already in flight.
    function refreshHaEntitiesIfStale(this: any) {
        if (!state.haUrl || !state.haToken)
            return;
        if (state.haConnectionStatus === "connecting" || haRefreshTimer)
            return;
        if (Date.now() - haLastRefreshAt < HA_FOCUS_REFRESH_MIN_AGE_MS)
            return;
        scheduleHaEntityRefresh();
    }
    return {
        "normalizeHaWebsocketUrl": staticGlobal(normalizeHaWebsocketUrl),
        "syncHaEntityStatus": staticGlobal(syncHaEntityStatus),
        "setHaConnectionStatus": staticGlobal(setHaConnectionStatus),
        "refreshHaEntities": staticGlobal(refreshHaEntities),
        "scheduleHaEntityRefresh": staticGlobal(scheduleHaEntityRefresh),
        "refreshHaEntitiesIfStale": staticGlobal(refreshHaEntitiesIfStale),
    };
}
