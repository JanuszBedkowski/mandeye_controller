#pragma once
std::string_view indexWebPageData = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Camera Control</title>
    <style>
        body {
            font-family: sans-serif;
            margin: 0;
            display: flex;
            height: 100vh;
        }
        #camera {
            flex: 1;
            background: #000;
            display: flex;
            flex-direction: column;
            justify-content: flex-start;
            align-items: center;
        }
        #photo {
            max-width: 100%;
            max-height: 100%;
            object-fit: contain;
        }
        #photoMeta {
            width: 100%;
            max-width: 640px;
            box-sizing: border-box;
            margin-top: 10px;
        }
        #controls {
            flex: 1;
            padding: 10px;
            display: flex;
            flex-direction: column;
            box-sizing: border-box;
        }
        textarea {
            width: 100%;
            height: 800px;
            font-family: monospace;
        }
        pre {
            background: #eee;
            padding: 10px;
            white-space: pre-wrap;
            flex: 1;
            overflow: auto;
        }
        button {
            margin: 5px 0;
        }
    </style>
</head>
<body>
<div id="camera">
    <img id="photo" alt="Camera stream">
    <pre id="photoMeta"></pre>
</div>

<div id="controls">
    <h2>Camera Control</h2>
    <button id="btnGet">Get Config</button>
    <button id="btnSet">Set Config</button>
    <button id="btnStop">Stop Stream</button>
    <button id="btnDwnl">Download Full Image</button>
    <button id="btnMeta">Show Photo Metadata</button>

    <h3>Config (editable)</h3>
    <textarea id="configInput"></textarea>
</div>

<script>
    const port = window.location.port ? ':' + window.location.port : '';
    const API = 'http://' + window.location.hostname + port;
    const photo = document.getElementById('photo');
    const configInput = document.getElementById('configInput');
    let streamTimer = null;

    async function getConfig() {
        try {
            const res = await fetch(`${API}/getConfig`);
            const json = await res.json();
            configInput.value = JSON.stringify(json, null, 2);
        } catch (err) {
            alert('Error fetching config: ' + err);
        }
    }

    async function setConfig() {
        try {
            const json = JSON.parse(configInput.value);
            console.log(JSON.stringify(json));
            const res = await fetch(`${API}/setConfig`, {
                method: 'POST',
                headers: { 'Content-Type': 'text/plain' },
                body: JSON.stringify(json)
            });
            const text = await res.text();
            alert('Response: ' + text);
        } catch (err) {
            alert('Error sending config: ' + err);
        }
    }

    function startStream() {
        if (streamTimer) return;
        const update = async () => {
            photo.src = `${API}/photo?cacheBust=${Date.now()}`;
            // Fetch and display metadata with each photo update
            try {
                const res = await fetch('/photoMeta');
                const json = await res.json();
                document.getElementById('photoMeta').textContent = JSON.stringify(json, null, 2);
            } catch (err) {
                document.getElementById('photoMeta').textContent = 'Error fetching photo metadata: ' + err;
            }
        };
        update();
        streamTimer = setInterval(update, 250);
    }

    function stopStream() {
        clearInterval(streamTimer);
        streamTimer = null;
    }

    function downloadImage(){
     fetch('/photoFull')
            .then(response => response.blob())
            .then(blob => {
                const url = window.URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.style.display = 'none';
                a.href = url;
                a.download = 'photo_full.jpg';
                document.body.appendChild(a);
                a.click();
                window.URL.revokeObjectURL(url);
            });
    }

    async function showPhotoMeta() {
        try {
            const res = await fetch('/photoMeta');
            const json = await res.json();
            document.getElementById('photoMeta').textContent = JSON.stringify(json, null, 2);
        } catch (err) {
            alert('Error fetching photo metadata: ' + err);
        }
    }

    document.getElementById('btnGet').onclick = getConfig;
    document.getElementById('btnSet').onclick = setConfig;
    document.getElementById('btnStop').onclick = stopStream;
    document.getElementById('btnDwnl').onclick = downloadImage;
    document.getElementById('btnMeta').onclick = showPhotoMeta;


    // Automatically start stream and get config on load
    window.onload = () => {
        startStream();
        getConfig();
    };
</script>
</body>
</html>
)rawliteral";