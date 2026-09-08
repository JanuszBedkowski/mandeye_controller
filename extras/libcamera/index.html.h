#pragma once
std::string_view indexWebPageData = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Camera Control</title>
    <style>
        body { font-family: sans-serif; margin: 0; display: flex; flex-wrap: wrap; align-items: flex-start; }
        #camera { flex: 1 1 360px; background: #000; padding: 10px; text-align: center; }
        #photo { width: 100%; max-width: 640px; background: #111; }
        #camera button { margin: 6px 4px 0; }
        #photoMeta { text-align: left; max-width: 640px; margin: 8px auto 0; padding: 8px;
            background: #111; color: #ccc; font: 12px/1.4 monospace; white-space: pre-wrap;
            max-height: 30vh; overflow: auto; }
        #controls { flex: 1 1 420px; padding: 16px; max-width: 720px; }
        h2 { margin: 0 0 12px; }
        .row { display: flex; align-items: center; gap: 8px; padding: 4px 0; }
        .row label { flex: 0 0 190px; font-size: 13px; }
        .row .hint { color: #888; font-size: 11px; }
        .row input[type=number], .row select { padding: 4px; font: inherit; }
        .row input[type=number] { width: 130px; }
        .actions { margin: 14px 0; }
        .actions button { padding: 6px 14px; font: inherit; }
        #msg { font-size: 13px; margin: 8px 0; min-height: 1.2em; }
        .group { border: 1px solid #ccc; margin: 8px 0; padding: 0 10px; }
        .group > summary { cursor: pointer; padding: 8px 0; font-weight: bold; font-size: 14px; }
        details { margin-top: 16px; }
        textarea { width: 100%; min-height: 260px; font: 12px monospace; margin-top: 8px; }
    </style>
</head>
<body>
<div id="camera">
    <img id="photo" alt="Camera stream"><br>
    <button id="btnStream">Pause stream</button>
    <button id="btnDwnl">Download full image</button>
    <pre id="photoMeta"></pre>
</div>

<div id="controls">
    <h2>Camera Control</h2>

    <div class="row">
        <label for="rateMs">rateMs</label>
        <input type="number" id="rateMs" min="0" step="10">
        <span class="hint">ms between frames</span>
    </div>

    <div id="form"></div>

    <div class="actions">
        <button id="btnReload">Reload</button>
        <button id="btnApply">Apply</button>
        <button id="btnSave">Save to USB</button>
    </div>
    <div id="msg"></div>

    <details>
        <summary>Raw JSON</summary>
        <textarea id="raw" spellcheck="false"></textarea>
        <div class="actions">
            <button id="btnSyncRaw">Sync from fields</button>
            <button id="btnApplyRaw">Apply raw JSON</button>
        </div>
    </details>
</div>

<script>
    "use strict";
    const photo = document.getElementById("photo");
    const photoMeta = document.getElementById("photoMeta");
    const formEl = document.getElementById("form");
    const msgEl = document.getElementById("msg");
    const rawEl = document.getElementById("raw");

    let fullConfig = null;
    let widgets = [];
    let resSelEl = null;
    let streamTimer = null;

    // known libcamera enum controls -> option labels (index = value)
    const ENUMS = {
        AeMeteringMode:    ["CentreWeighted", "Spot", "Matrix", "Custom"],
        AeConstraintMode:  ["Normal", "Highlight", "Shadows", "Custom"],
        AeExposureMode:    ["Normal", "Short", "Long", "Custom"],
        AeFlickerMode:     ["Off", "Manual", "Auto"],
        ExposureTimeMode:  ["Auto", "Manual"],
        AnalogueGainMode:  ["Auto", "Manual"],
        AwbMode:           ["Auto", "Incandescent", "Tungsten", "Fluorescent", "Indoor", "Daylight", "Cloudy", "Custom"],
        AfMode:            ["Manual", "Auto", "Continuous"],
        AfRange:           ["Normal", "Macro", "Full"],
        AfSpeed:           ["Normal", "Fast"],
        AfMetering:        ["Auto", "Windows"],
        AfPause:           ["Immediate", "Deferred", "Resume"],
        HdrMode:           ["Off", "MultiExposureUnmerged", "MultiExposure", "SingleExposure", "Night"],
        NoiseReductionMode:["Off", "Fast", "HighQuality", "Minimal", "ZSL"],
    };

    // controls grouped into collapsible sections; first matching group wins
    const GROUPS = [
        ["Auto exposure", n => /^Ae/.test(n) || /^Exposure/.test(n) || /^AnalogueGain|^DigitalGain/.test(n) || n === "FrameDurationLimits"],
        ["White balance", n => /^Awb/.test(n) || /^Colour/.test(n)],
        ["Autofocus",     n => /^Af/.test(n) || n === "LensPosition"],
        ["HDR",           n => /^Hdr/.test(n)],
        ["Image tuning",  n => ["Brightness", "Contrast", "Saturation", "Sharpness", "Gamma", "NoiseReductionMode"].includes(n)],
        ["Other",         () => true],
    ];
    const groupOf = n => (GROUPS.find(g => g[1](n)) || GROUPS[GROUPS.length - 1])[0];

    // array / rectangle controls: element labels + count (count falls back to the current value's length)
    const ARRAY_CTL = {
        ColourGains:         { labels: ["red", "blue"], float: true },
        FrameDurationLimits: { labels: ["min us", "max us"], float: false },
        ScalerCrop:          { labels: ["x", "y", "w", "h"], float: false },
    };
    // only the controls we can actually set (ARRAY_CTL) plus ScalerCrop's rectangle
    const isArrayCtl = (name, info) =>
        !!ARRAY_CTL[name] || (name === "ScalerCrop" && info.type_str === "ControlTypeRectangle");

    /* ---------- helpers ---------- */
    function showMsg(t) { msgEl.textContent = t; }
    function num(v) {
        if (typeof v === "boolean") return v ? 1 : 0;
        const f = parseFloat(v);
        return Number.isFinite(f) ? f : null;
    }
    function firstNum() {
        for (const x of arguments) { const n = num(x); if (n !== null) return n; }
        return null;
    }
    const isBool  = i => i.type_str === "ControlTypeBool";
    const isFloat = i => i.type_str === "ControlTypeFloat";
    const isInt   = i => /Integer|Byte|Unsigned/.test(i.type_str || "") ||
                         (i.type_str === "Unknown" && num(i.min) !== null && num(i.max) !== null);
    const isRenderable = i => isBool(i) || isFloat(i) || isInt(i);
    const defNum  = i => num(i.def);
    const defBool = i => { const s = String(i.def).toLowerCase();
                           return s === "true" ? true : s === "false" ? false : null; };

    /* ---------- build fields from fullConfig ---------- */
    function build() {
        formEl.innerHTML = "";
        widgets = [];
        const info = (fullConfig && fullConfig.controls_info) || {};
        const values = (fullConfig && fullConfig.picamera) || {};
        document.getElementById("rateMs").value =
            Number.isFinite(fullConfig && fullConfig.rateMs) ? fullConfig.rateMs : 500;

        buildResolutionRow();

        const names = Object.keys(info).sort().filter(name => {
            const i = info[name];
            if (!i || typeof i !== "object" || !i.type_str) return false;
            if (name.startsWith("_") || i.isInput === false) return false;
            if (i.isArray === true && !ARRAY_CTL[name]) return false; // unknown array control - not settable here
            return isRenderable(i) || isArrayCtl(name, i);
        });

        for (const [title] of GROUPS) {
            const inGroup = names.filter(n => groupOf(n) === title);
            if (!inGroup.length) continue;
            const det = document.createElement("details");
            det.className = "group";
            det.open = true;
            const sum = document.createElement("summary");
            sum.textContent = title;
            det.appendChild(sum);
            for (const name of inGroup) det.appendChild(makeRow(name, info[name], values[name]));
            formEl.appendChild(det);
        }
    }

    // resolution picker, built from the discrete list the camera reports
    function buildResolutionRow() {
        resSelEl = null;
        const list = (fullConfig && fullConfig.resolutions) || [];
        if (!list.length) return;
        const cw = fullConfig.width, ch = fullConfig.height;
        const row = document.createElement("div");
        row.className = "row";
        const label = document.createElement("label");
        label.textContent = "Resolution";
        const sel = document.createElement("select");
        let matched = false;
        list.forEach(([w, h]) => {
            const o = document.createElement("option");
            o.value = w + "x" + h;
            o.textContent = w + " × " + h;
            if (w === cw && h === ch) matched = true;
            sel.appendChild(o);
        });
        if (!matched && cw && ch) {
            const o = document.createElement("option");
            o.value = cw + "x" + ch;
            o.textContent = cw + " × " + ch + " (current)";
            sel.appendChild(o);
        }
        if (cw && ch) sel.value = cw + "x" + ch;
        row.appendChild(label);
        row.appendChild(sel);
        formEl.appendChild(row);
        resSelEl = sel;
    }

    function makeRow(name, info, current) {
        if (isArrayCtl(name, info)) return makeArrayRow(name, info, current);

        const row = document.createElement("div");
        row.className = "row";
        const label = document.createElement("label");
        label.textContent = name;
        row.appendChild(label);

        const mn = num(info.min), mx = num(info.max);
        const wasUnset = current === null || current === undefined;
        const w = { name, wasUnset, touched: false, read: () => null };
        const touch = () => { w.touched = true; };

        let hint = "";
        if (ENUMS[name]) {
            const sel = document.createElement("select");
            const lo = mn === null ? 0 : mn;
            const hi = mx === null ? ENUMS[name].length - 1 : mx;
            ENUMS[name].forEach((lbl, idx) => {
                if (idx < lo || idx > hi) return;
                const o = document.createElement("option");
                o.value = String(idx);
                o.textContent = idx + " " + lbl;
                sel.appendChild(o);
            });
            sel.value = String(firstNum(current, defNum(info), lo));
            sel.addEventListener("change", touch);
            row.appendChild(sel);
            w.read = () => parseInt(sel.value, 10);
            if (defNum(info) !== null) hint = "default " + info.def;
        } else if (isBool(info)) {
            const cb = document.createElement("input");
            cb.type = "checkbox";
            cb.checked = current === true || current === 1 || current === "true" ||
                         (wasUnset && defBool(info) === true);
            cb.addEventListener("change", touch);
            row.appendChild(cb);
            w.read = () => cb.checked;
            if (defBool(info) !== null) hint = "default " + defBool(info);
        } else {
            const inp = document.createElement("input");
            inp.type = "number";
            if (mn !== null) inp.min = mn;
            if (mx !== null) inp.max = mx;
            inp.step = isFloat(info) ? "any" : 1;
            inp.value = firstNum(current, defNum(info), mn, 0);
            inp.addEventListener("input", touch);
            row.appendChild(inp);
            w.read = () => {
                let v = parseFloat(inp.value);
                if (!Number.isFinite(v)) return null;
                if (mn !== null) v = Math.max(v, mn);
                if (mx !== null) v = Math.min(v, mx);
                return isInt(info) ? Math.round(v) : v;
            };
            const bits = [];
            if (mn !== null && mx !== null) bits.push(info.min + " … " + info.max);
            if (defNum(info) !== null) bits.push("default " + info.def);
            hint = bits.join(", ");
        }

        if (wasUnset) hint = hint ? "not set, " + hint : "not set";
        if (hint) {
            const h = document.createElement("span");
            h.className = "hint";
            h.textContent = hint;
            row.appendChild(h);
        }
        widgets.push(w);
        return row;
    }

    // array / rectangle control: one number input per element
    function makeArrayRow(name, info, current) {
        const spec = ARRAY_CTL[name] || { labels: ["x", "y", "w", "h"], float: false };
        const cur = Array.isArray(current) ? current : [];
        const n = spec.labels.length || cur.length || 2;
        const wasUnset = !Array.isArray(current) || current.length === 0;

        const row = document.createElement("div");
        row.className = "row";
        const label = document.createElement("label");
        label.textContent = name;
        row.appendChild(label);

        const w = { name, wasUnset, touched: false, read: () => null };
        const touch = () => { w.touched = true; };
        const inputs = [];
        for (let i = 0; i < n; i++) {
            const inp = document.createElement("input");
            inp.type = "number";
            inp.step = spec.float ? "any" : 1;
            inp.style.width = "88px";
            if (spec.labels[i]) inp.title = spec.labels[i];
            if (cur[i] !== undefined) inp.value = cur[i];
            inp.addEventListener("input", touch);
            row.appendChild(inp);
            inputs.push(inp);
        }
        w.read = () => {
            const vals = inputs.map(x => spec.float ? parseFloat(x.value) : parseInt(x.value, 10));
            if (vals.some(v => !Number.isFinite(v))) return null;
            return vals;
        };

        const bits = [];
        if (spec.labels.length) bits.push("[" + spec.labels.join(", ") + "]");
        if (wasUnset) bits.unshift("not set");
        if (bits.length) {
            const h = document.createElement("span");
            h.className = "hint";
            h.textContent = bits.join(" ");
            row.appendChild(h);
        }
        widgets.push(w);
        return row;
    }

    /* ---------- read fields back into a config object ---------- */
    function readForm() {
        const cfg = JSON.parse(JSON.stringify(fullConfig || {}));
        cfg.picamera = cfg.picamera || {};
        const r = parseInt(document.getElementById("rateMs").value, 10);
        if (Number.isFinite(r)) cfg.rateMs = r;
        if (resSelEl && resSelEl.value) {
            const wh = resSelEl.value.split("x");
            cfg.width = parseInt(wh[0], 10);
            cfg.height = parseInt(wh[1], 10);
        }
        for (const w of widgets) {
            if (w.wasUnset && !w.touched) { delete cfg.picamera[w.name]; continue; }
            const v = w.read();
            if (v !== null && v !== undefined && !Number.isNaN(v)) cfg.picamera[w.name] = v;
        }
        for (const k of Object.keys(cfg.picamera)) {
            const v = cfg.picamera[k];
            if (v === null || (Array.isArray(v) && v.length === 0)) delete cfg.picamera[k];
        }
        return cfg;
    }

    /* ---------- server ---------- */
    async function loadConfig() {
        try {
            const res = await fetch("/getConfig", { cache: "no-store" });
            fullConfig = await res.json();
            build();
            rawEl.value = JSON.stringify(fullConfig, null, 2);
            showMsg("Config loaded");
        } catch (err) {
            showMsg("Error fetching config: " + err);
        }
    }

    async function post(url, obj) {
        const res = await fetch(url, {
            method: "POST",
            headers: { "Content-Type": "text/plain" },
            body: JSON.stringify(obj),
        });
        const text = await res.text();
        if (!res.ok) throw new Error(text || res.status);
        return text;
    }

    async function apply() {
        try {
            showMsg("Applying…");
            const resp = await post("/setConfig", readForm());
            showMsg("Applied: " + resp);
            setTimeout(loadConfig, 400);
        } catch (err) {
            showMsg("Apply failed: " + err);
        }
    }

    async function save() {
        if (!confirm("Overwrite the camera config file on the USB stick with the current settings?")) return;
        try {
            showMsg("Saving to USB…");
            const resp = await post("/saveConfig", readForm());
            showMsg(resp);
        } catch (err) {
            showMsg("Save failed: " + err);
        }
    }

    async function applyRaw() {
        try {
            const resp = await post("/setConfig", JSON.parse(rawEl.value));
            showMsg("Applied raw JSON: " + resp);
            setTimeout(loadConfig, 400);
        } catch (err) {
            showMsg("Raw apply failed: " + err);
        }
    }

    /* ---------- stream ---------- */
    function tick() {
        photo.src = "/photo?t=" + Date.now();
        fetch("/photoMeta", { cache: "no-store" })
            .then(r => r.json())
            .then(j => { photoMeta.textContent = JSON.stringify(j, null, 2); })
            .catch(err => { photoMeta.textContent = "metadata error: " + err; });
    }
    function startStream() {
        if (streamTimer) return;
        tick();
        streamTimer = setInterval(tick, 250);
        document.getElementById("btnStream").textContent = "Pause stream";
    }
    function stopStream() {
        clearInterval(streamTimer);
        streamTimer = null;
        document.getElementById("btnStream").textContent = "Resume stream";
    }

    function download() {
        fetch("/photoFull")
            .then(r => r.blob())
            .then(blob => {
                const a = document.createElement("a");
                a.href = URL.createObjectURL(blob);
                a.download = "photo_full.jpg";
                document.body.appendChild(a);
                a.click();
                a.remove();
                URL.revokeObjectURL(a.href);
            });
    }

    document.getElementById("btnReload").onclick = loadConfig;
    document.getElementById("btnApply").onclick = apply;
    document.getElementById("btnSave").onclick = save;
    document.getElementById("btnApplyRaw").onclick = applyRaw;
    document.getElementById("btnSyncRaw").onclick = () => {
        rawEl.value = JSON.stringify(readForm(), null, 2);
        showMsg("Raw JSON synced from fields");
    };
    document.getElementById("btnStream").onclick = () => streamTimer ? stopStream() : startStream();
    document.getElementById("btnDwnl").onclick = download;

    window.addEventListener("load", () => { startStream(); loadConfig(); });
</script>
</body>
</html>
)rawliteral";
