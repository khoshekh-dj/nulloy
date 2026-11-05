/********************************************************************
**  TrackDeck, https://github.com/khoshekh-dj/trackdeck
**
**  "Khoshekh" skin. This skin package including all images, cascading
**  style sheets, UI forms, and JavaScript files is released under
**  Attribution-ShareAlike Unported License 3.0 (CC-BY-SA 3.0):
**  http://creativecommons.org/licenses/by-sa/3.0/
**  Credits for the skins TrackDeck's skins build on are in README.md.
**
*********************************************************************/

function Main()
{
    try {
        Ui.titleWidget.enableDoubleClick();
        Ui.titleWidget.doubleClicked.connect(Ui.mainWindow.toggleMaximize);

        PlaybackEngine["stateChanged(N::PlaybackState)"].connect(this, "on_stateChanged");
        PlaybackEngine["mediaChanged(const QString &, int)"].connect(this, "on_mediaChanged");
        Player["trackInfoChanged()"].connect(this, "updateTrackLabels"); // e.g. BPM detected
        Player["ambientImageChanged(QString)"].connect(this, "on_ambientImageChanged");
        Settings["valueChanged(QString,QVariant)"].connect(this, "on_settingChanged");

        Ui.mainWindow["newTitle(const QString &)"].connect(this, "setTitle");
        Ui.mainWindow["fullScreenEnabled(bool)"].connect(this, "on_fullScreenEnabled");
        Ui.mainWindow["showPlaybackControlsEnabled(bool)"].connect(this, "on_showPlaybackControlsEnabled");
        Ui.mainWindow["maximizeEnabled(bool)"].connect(this, "on_maximizeEnabled");
        Ui.mainWindow.resized.connect(this, "on_resized");

        if (!Settings.value("KhoshekhSkin/Splitter")) {
            Settings.setValue("KhoshekhSkin/Splitter", [300, 260]);
        }
        if (!Settings.value("KhoshekhSkin/SplitterFullScreen")) {
            Settings.setValue("KhoshekhSkin/SplitterFullScreen", [999999, 0]);
        }
        Ui.splitter["splitterMoved(int, int)"].connect(this, "on_splitterMoved");

        Ui.mainWindow.windowFlags = (Ui.mainWindow.windowFlags | Qt.FramelessWindowHint | Qt.WindowCloseButtonHint) ^ (Qt.WindowTitleHint | Qt.Dialog);

        if (Q_WS == "mac") {
            Ui.playlistWidget.setAttribute(Qt.WA_MacShowFocusRect, false);
            Ui.sizeGrip.setParent(Ui.borderWidget);
        } else {
            Ui.sizeGrip.hide();
            Ui.mainWindow.setSizeGripEnabled(true);
        }

        // window buttons go where the platform expects them
        if (Q_WS != "mac" && WS_WM_BUTTON_DIRECTION != "left") {
            Ui.titleWidget.layout().addWidget(Ui.minimizeButton);
            Ui.titleWidget.layout().addWidget(Ui.closeButton);
        }

        this.marginsBkp_ = Ui.borderWidget.layout().contentsMargins();

        if (WS_WM_TILING) {
            Ui.titleWidget.setVisible(false);
        }
    } catch (err) {
        print("QtScript: " + err);
    }
}

Main.prototype.afterShow = function()
{
    Ui.splitter.setSizes(Settings.value("KhoshekhSkin/Splitter"));
    Ui.menuButton["clicked()"].connect(this, "showMenu");
    this.updateTrackLabels();
    this.on_resized();

    // the skin's stylesheet, before accent/ambient changes are applied to it
    this.baseStyleSheet = Ui.mainWindow.styleSheet;
    this.ambientImage = Player.ambientImage();
    this.applyStyle();
}

Main.prototype.showMenu = function()
{
    var menuPos = Ui.menuButton.pos;
    menuPos.y += Ui.menuButton.height;
    Player.showContextMenu(menuPos);
}

Main.prototype.on_mediaChanged = function(file, id)
{
    this.updateTrackLabels();
}

Main.prototype.updateTrackLabels = function()
{
    var title = Player.trackInfo("{%t|%F}");
    if (title == "") {
        Ui.trackTitleLabel.text = "TrackDeck";
        Ui.trackArtistLabel.text = "Drop music here to start";
        Ui.trackMetaLabel.text = "";
        return;
    }

    var artist = Player.trackInfo("%a");
    var album = Player.trackInfo("%A");
    Ui.trackTitleLabel.text = title;
    Ui.trackTitleLabel.toolTip = title;
    Ui.trackArtistLabel.text = (artist != "" && album != "") ? artist + "  —  " + album : artist + album;
    Ui.trackArtistLabel.toolTip = Ui.trackArtistLabel.text;

    var meta = [];
    var formats = ["{%M BPM}", "{%B kbps}", "{%s kHz}"];
    for (var i = 0; i < formats.length; ++i) {
        var value = Player.trackInfo(formats[i]);
        if (value != "") {
            meta.push(value);
        }
    }
    Ui.trackMetaLabel.text = meta.join("   ·   ");
}

Main.prototype.on_stateChanged = function(state)
{
    if (state == N.PlaybackPlaying)
        Ui.playButton.styleSheet = "qproperty-icon: url(pause.svg)";
    else
        Ui.playButton.styleSheet = "qproperty-icon: url(play.svg)";
}

Main.prototype.on_resized = function()
{
    if (Q_WS == "mac") {
        Ui.sizeGrip.move(Ui.sizeGrip.parentWidget().width - Ui.sizeGrip.width - 3,
                         Ui.sizeGrip.parentWidget().height - Ui.sizeGrip.height - 3);
        Ui.sizeGrip.raise();
    }
}

Main.prototype.on_splitterMoved = function(pos, index)
{
    if (Ui.mainWindow.isFullSceen()) {
        Settings.setValue("KhoshekhSkin/SplitterFullScreen", Ui.splitter.sizes());
    } else {
        Settings.setValue("KhoshekhSkin/Splitter", Ui.splitter.sizes());
    }
}

Main.prototype.setTitle = function(title)
{
    Ui.titleLabel.text = title;
    Ui.titleLabel.toolTip = title;
}

Main.prototype.on_fullScreenEnabled = function(enabled)
{
    if (enabled) {
        Ui.splitter.setSizes(Settings.value("KhoshekhSkin/SplitterFullScreen"));
    } else {
        Ui.splitter.setSizes(Settings.value("KhoshekhSkin/Splitter"));
    }

    if (Settings.value("ShowPlaybackControls")) {
        Ui.controlsContainer.setVisible(!enabled);
    }
    Ui.titleWidget.setVisible(!enabled);
    this.setBorderVisible(!enabled);
}

Main.prototype.on_showPlaybackControlsEnabled = function(enabled)
{
    if (!Ui.mainWindow.isFullSceen()) {
        Ui.controlsContainer.setVisible(enabled);
    }
}

Main.prototype.on_maximizeEnabled = function(enabled)
{
    this.setBorderVisible(!enabled);
}

Main.prototype.setBorderVisible = function(enabled)
{
    if (!enabled) {
        Ui.borderWidget.layout().setContentsMargins(0, 0, 0, 0);
    } else {
        Ui.borderWidget.layout().setContentsMargins(this.marginsBkp_);
    }
}

// --- accent colour & album-art background ----------------------------------

function hexToRgb(hex)
{
    hex = String(hex).replace("#", "");
    return [parseInt(hex.substr(0, 2), 16), parseInt(hex.substr(2, 2), 16), parseInt(hex.substr(4, 2), 16)];
}

function rgbToHex(rgb)
{
    var s = "#";
    for (var i = 0; i < 3; ++i) {
        var v = Math.max(0, Math.min(255, Math.round(rgb[i]))).toString(16).toUpperCase();
        s += (v.length < 2 ? "0" : "") + v;
    }
    return s;
}

// t > 0 mixes towards white, t < 0 towards black
function shade(rgb, t)
{
    var target = t > 0 ? 255 : 0;
    var k = Math.abs(t);
    return [rgb[0] + (target - rgb[0]) * k, rgb[1] + (target - rgb[1]) * k, rgb[2] + (target - rgb[2]) * k];
}

function replaceAll(text, from, to)
{
    return text.split(from).join(to);
}

Main.prototype.applyStyle = function()
{
    if (this.baseStyleSheet === undefined) {
        return;
    }
    var sheet = this.baseStyleSheet;

    // The skin's accents are teal; swap in the chosen accent colour. The navy
    // backgrounds (#05070F, #0A1420, #0E2630, #12303A) keep the night-sky look.
    var accent = String(Settings.value("AccentColor") || "#2F8F9A");
    if (/^#[0-9A-Fa-f]{6}$/.test(accent) && accent.toUpperCase() != "#2F8F9A") {
        var rgb = hexToRgb(accent);
        sheet = replaceAll(sheet, "#2F8F9A", rgbToHex(rgb));               // accent
        sheet = replaceAll(sheet, "#3FA9B5", rgbToHex(shade(rgb, 0.18)));  // hover, wave outline
        sheet = replaceAll(sheet, "#24707A", rgbToHex(shade(rgb, -0.18))); // pressed
        sheet = replaceAll(sheet, "#6CC3CC", rgbToHex(shade(rgb, 0.35)));  // light text
        sheet = replaceAll(sheet, "#1E5F6E", rgbToHex(shade(rgb, -0.35))); // deep accent
        sheet = replaceAll(sheet, "rgba(47, 143, 154,",
                           "rgba(" + rgb[0] + ", " + rgb[1] + ", " + rgb[2] + ",");
    }

    // blurred album art behind a semi-transparent player, tinted navy
    if (this.ambientImage) {
        // NB: written as "url" + "(" so the skin loader, which rewrites every
        // literal url( in skin files to url(skin:...), leaves this path alone
        sheet += "\n#borderWidget { background: #05070F url" + "(\"" + this.ambientImage + "\") no-repeat center center; }"
               + "\n#splitter, #splitTop, #splitter::handle { background: transparent; }"
               + "\n#playlistWidget { background: rgba(2, 4, 10, 110); }"
               + "\n#coverWidget { background: rgba(10, 20, 32, 120); }"
               + "\n#waveformSlider { qproperty-background: rgba(10, 20, 32, 125); }";
    }

    Ui.mainWindow.styleSheet = sheet;
}

Main.prototype.on_ambientImageChanged = function(path)
{
    this.ambientImage = path;
    this.applyStyle();
}

Main.prototype.on_settingChanged = function(key, value)
{
    if (key == "AccentColor") {
        this.applyStyle();
    }
}
