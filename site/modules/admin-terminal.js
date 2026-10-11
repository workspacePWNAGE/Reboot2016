const outputDiv = document.getElementById('output');
const cliInput = document.getElementById('cli');
const promptSymbol = document.getElementById('prompt-symbol');
const terminalContainer = document.getElementById('terminal-container');
const fileInput = document.getElementById('hidden-file-input');

let REQUEST_TIMEOUT_MS = 15000; 
let UPLOAD_TIMEOUT_MS = 120000;

let state = 'BUSY';
let promptText = 'admin@server:~$';
let pendingUploadType = '';

const history = [];
let historyIndex = 0;

function printLine(text, className = '') {
    const span = document.createElement('span');
    span.textContent = text + '\n';
    if (className) span.className = className;
    outputDiv.appendChild(span);
    terminalContainer.scrollTop = terminalContainer.scrollHeight;
}

async function fetchWithTimeout(url, options, ms) {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), ms);
    try {
        return await fetch(url, { ...options, signal: controller.signal });
    } finally {
        clearTimeout(timer);
    }
}

async function readJson(res) {
    const raw = await res.text();
    try {
        return { data: JSON.parse(raw), problem: null };
    } catch (_) {
        const snippet = raw.replace(/\s+/g, ' ').trim().slice(0, 160);
        const type = res.headers.get('content-type') || 'unknown type';
        return { data: {}, problem: `The server did not answer with JSON (HTTP ${res.status}, ${type})${snippet ? ': ' + snippet : '.'}` };
    }
}

async function sendCommand(command) {
    try {
        const res = await fetchWithTimeout('/api/admin/terminal', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ command })
        }, REQUEST_TIMEOUT_MS);

        const { data, problem } = await readJson(res);
        if (problem) return { ok: false, text: problem };

        if (data.error) {
            if (res.status === 401) return { ok: false, text: data.error + ' Your session may have expired, log in again.' };
            return { ok: false, text: data.error };
        }
        if (data.output !== undefined) return { ok: true, text: data.output };
        return { ok: res.ok, text: res.ok ? JSON.stringify(data) : `Request failed (${res.status}).` };
    } catch (err) {
        if (err && err.name === 'AbortError') {
            return { ok: false, text: `No answer from the server after ${REQUEST_TIMEOUT_MS / 1000} seconds. The request was cancelled (type 'reconnect' to try again).` };
        }
        return { ok: false, text: `Couldn't reach the server: ${err && err.message ? err.message : 'network error'}.` };
    }
}

function resetPrompt() {
    state = 'READY';
    promptSymbol.innerText = promptText;
    cliInput.style.display = 'block';
    cliInput.focus();
}

async function connect() {
    state = 'BUSY';
    printLine('Connecting...', 'system');

    const who = await sendCommand('whoami');

    if (who.ok) {
        const match = who.text.match(/^Logged in as (\S+)/);
        if (match) promptText = `${match[1]}@server:~$`;
        printLine(who.text, 'system');
        printLine("Type 'help' for the commands you can use.\n", 'system');
    } else {
        printLine(who.text, 'error');
        printLine("Not connected. Type 'reconnect' to try again.\n", 'system');
    }

    resetPrompt();
}

document.addEventListener('click', () => cliInput.focus());

function cancelUploadPick() {
    if (state !== 'UPLOAD_PICK') return;
    printLine('Upload cancelled.', 'system');
    pendingUploadType = '';
    resetPrompt();
}

function startFilePick(type) {
    pendingUploadType = type;
    state = 'UPLOAD_PICK';
    promptSymbol.innerText = '';
    cliInput.style.display = 'none';

    window.addEventListener('focus', () => {
        setTimeout(() => { if (state === 'UPLOAD_PICK' && !fileInput.files.length) cancelUploadPick(); }, 600);
    }, { once: true });

    fileInput.click();
}

fileInput.addEventListener('cancel', cancelUploadPick);

fileInput.addEventListener('change', async (e) => {
    const file = e.target.files[0];
    if (!file) {
        cancelUploadPick();
        return;
    }

    state = 'BUSY';
    printLine(`Uploading ${file.name}...`, 'system');

    const formData = new FormData();
    formData.append('type', pendingUploadType);
    formData.append('file', file);

    try {
        const res = await fetchWithTimeout('/api/admin/upload', { method: 'POST', body: formData }, UPLOAD_TIMEOUT_MS);
        const { data, problem } = await readJson(res);

        if (problem) printLine(problem, 'error');
        else if (data.error) printLine(data.error, 'error');
        else if (data.message) printLine(data.message, 'success');
        else printLine(`Upload failed (${res.status}).`, 'error');
    } catch (err) {
        printLine(err && err.name === 'AbortError' ? 'Upload timed out.' : 'Upload failed: Network error.', 'error');
    }

    fileInput.value = '';
    pendingUploadType = '';
    resetPrompt();
});

cliInput.addEventListener('keydown', async (e) => {
    if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
        if (state !== 'READY' || history.length === 0) return;
        e.preventDefault();
        historyIndex += e.key === 'ArrowUp' ? -1 : 1;
        historyIndex = Math.max(0, Math.min(history.length, historyIndex));
        cliInput.value = history[historyIndex] || '';
        return;
    }

    if (e.key !== 'Enter' || state === 'BUSY' || state === 'UPLOAD_PICK') return;

    const inputVal = cliInput.value.trim();
    cliInput.value = '';

    if (state === 'UPLOAD_TYPE') {
        printLine(`Upload Type: ${inputVal}`);
        const type = inputVal.toLowerCase();

        if (['texture', 'sound', 'place'].includes(type)) {
            startFilePick(type);
        } else {
            printLine('Invalid type. Allowed: texture, sound, place.', 'error');
            resetPrompt();
        }
        return;
    }

    if (state === 'READY') {
        if (!inputVal) {
            printLine(promptText + ' ');
            return;
        }

        printLine(`${promptText} ${inputVal}`);
        history.push(inputVal);
        historyIndex = history.length;

        const cmd = inputVal.split(/\s+/)[0].toLowerCase();

        if (cmd === 'clear') {
            outputDiv.innerHTML = '';
            return;
        }
        if (cmd === 'reconnect') {
            await connect();
            return;
        }

        state = 'BUSY';
        const result = await sendCommand(inputVal);

        if (cmd === 'upload' && result.ok && !/permission/i.test(result.text)) {
            state = 'UPLOAD_TYPE';
            promptSymbol.innerText = 'Asset type (texture/sound/place): ';
            cliInput.style.display = 'block';
            cliInput.focus();
            return;
        }

        printLine(result.text, result.ok ? '' : 'error');
        resetPrompt();
    }
});

connect();
