import { staticGlobal, type GlobalDescriptors } from "../runtime/globals";
export function installSettingsPhotosSectionModule(): GlobalDescriptors {
    // ── Settings Photo Screensaver Section ─────────────────────────────
    // Shown when the screensaver "Then" action is "Photos". Configures the
    // Home Assistant media-source folder to rotate through, the per-photo
    // duration, and the optional overlay widgets. The connection URL and token
    // live in the Home Assistant connection fields shared with entity
    // suggestions; this card only owns the photo-specific settings.
    function buildPhotoSettingsCard(this: any) {
        var body: any = document.createElement("div");
        body.className = "sp-photos-settings";

        var folderField: any = document.createElement("div");
        folderField.className = "sp-field";
        folderField.appendChild(fieldLabel("Photos Folder", "sp-set-ss-photos-folder"));
        var folderInp: any = document.createElement("input");
        folderInp.type = "text";
        folderInp.className = "sp-input";
        folderInp.id = "sp-set-ss-photos-folder";
        folderInp.placeholder = "media-source://media_source/local/photos";
        folderInp.value = state.photosFolder || "";
        folderField.appendChild(folderInp);
        var folderHelp: any = document.createElement("div");
        folderHelp.className = "sp-help";
        folderHelp.textContent =
            "The Home Assistant media-source folder to show, for example " +
            "media-source://media_source/local/photos for a folder in config/www or media.";
        folderField.appendChild(folderHelp);
        body.appendChild(folderField);
        bindTextPost(folderInp, entityName("screen_saver_photos_folder"), {
            onBlur: function (this: any, value?: any) { state.photosFolder = value; },
            post: function (this: any, value?: any) { postScreensaverPhotosFolder(value); },
        });
        els.setPhotosFolder = folderInp;

        var durationField: any = document.createElement("div");
        durationField.className = "sp-field";
        durationField.appendChild(fieldLabel("Seconds Per Photo", "sp-set-ss-photos-interval"));
        var durationInp: any = document.createElement("input");
        durationInp.type = "number";
        durationInp.min = "5";
        durationInp.max = "3600";
        durationInp.step = "5";
        durationInp.className = "sp-input";
        durationInp.id = "sp-set-ss-photos-interval";
        durationInp.value = String(state.photosInterval || 30);
        durationField.appendChild(durationInp);
        body.appendChild(durationField);
        durationInp.addEventListener("change", function (this: any) {
            var n: any = parseInt(this.value, 10);
            if (!Number.isFinite(n) || n < 5) n = 30;
            if (n > 3600) n = 3600;
            state.photosInterval = n;
            this.value = String(n);
            postScreensaverPhotosInterval(n);
        });
        els.setPhotosInterval = durationInp;

        var shuffleToggle: any = toggleRow("Shuffle", "sp-set-ss-photos-shuffle", state.photosShuffle);
        body.appendChild(shuffleToggle.row);
        shuffleToggle.input.addEventListener("change", function (this: any) {
            state.photosShuffle = this.checked;
            postScreensaverPhotosShuffle(state.photosShuffle);
        });
        els.setPhotosShuffle = shuffleToggle.input;

        var datetimeToggle: any = toggleRow("Show Date and Time", "sp-set-ss-photos-datetime", state.photosShowDatetime);
        body.appendChild(datetimeToggle.row);
        datetimeToggle.input.addEventListener("change", function (this: any) {
            state.photosShowDatetime = this.checked;
            postScreensaverPhotosShowDatetime(state.photosShowDatetime);
        });
        els.setPhotosShowDatetime = datetimeToggle.input;

        var weatherToggle: any = toggleRow("Show Weather", "sp-set-ss-photos-weather", state.photosShowWeather);
        body.appendChild(weatherToggle.row);
        weatherToggle.input.addEventListener("change", function (this: any) {
            state.photosShowWeather = this.checked;
            syncPhotoWeatherField();
            postScreensaverPhotosShowWeather(state.photosShowWeather);
        });
        els.setPhotosShowWeather = weatherToggle.input;

        var weatherField: any = document.createElement("div");
        weatherField.className = "sp-field sp-cond-field";
        weatherField.appendChild(fieldLabel("Weather Entity", "sp-set-ss-photos-weather-entity"));
        var weatherInp: any = entityInput("sp-set-ss-photos-weather-entity", state.photosWeatherEntity, "e.g. weather.home", ["weather"]);
        weatherField.appendChild(weatherInp);
        body.appendChild(weatherField);
        bindTextPost(weatherInp, entityName("screen_saver_photos_weather_entity"), {
            onBlur: function (this: any, value?: any) { state.photosWeatherEntity = value; },
            post: function (this: any, value?: any) { postScreensaverPhotosWeatherEntity(value); },
        });
        els.setPhotosWeatherEntity = weatherInp;

        function syncPhotoWeatherField(this: any) {
            weatherField.style.display = state.photosShowWeather ? "" : "none";
        }
        syncPhotoWeatherField();
        els.syncPhotoWeatherField = syncPhotoWeatherField;

        return body;
    }
    return {
        "buildPhotoSettingsCard": staticGlobal(buildPhotoSettingsCard),
    };
}
