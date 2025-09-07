let inQueue = true;
let queueInterval = null;

queueInterval = setInterval(pollQueue, 500);

const out = document.getElementById("output");
const searchBtn = document.getElementById("searchBtn");
const pingBtn = document.getElementById("pingBtn");

document.getElementById("searchBtn").addEventListener("click", doSearch);
document.getElementById("pingBtn").addEventListener("click", async () => {
    setLoading(true);
    const res = await sendCommand("ping");
    setLoading(false);

    if (res.type === "pong")
        renderOutput("pong", "success");
    else
        renderOutput(JSON.stringify(res, null, 2), "error");
});
document.getElementById("searchInput").addEventListener("keydown", (e) => {
    if (e.key === "Enter")
        doSearch();
});

async function sendCommand(command) {
    try {
        const response = await fetch("/api/command", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ command }),
        });
        return await response.json();
    } catch (err) {
        return { error: err.message };
    }
}

async function loadFileContent(path) {
    const resp = await fetch(`/api/file?path=${encodeURIComponent(path)}`);
    if (!resp.ok)
        throw new Error("Failed to load file");

    let text = await resp.text();
    return text.replace(/\r\n/g, "\n");
}

function setLoading(on) {
    searchBtn.disabled = on;
    pingBtn.disabled = on;
    if (on)
        out.innerHTML = `<div class="info">Loading…</div>`;
}

function renderOutput(msg, type = "info") {
    out.innerHTML = `<div class="${type}">${msg}</div>`;
}

async function doSearch() {
    const phrase = document.getElementById("searchInput").value.trim();
    if (!phrase)
        return;

    setLoading(true);
    const res = await sendCommand("search " + phrase);
    setLoading(false);

    if (res.type === "search")
        new FileListView(phrase, res.files);
    else
        renderOutput(JSON.stringify(res, null, 2));
}

async function pollQueue() {
    try {
        const resp = await fetch("/api/queue");
        if (!resp.ok)
            return;

        const data = await resp.text();

        if (data.includes("start")) {
            inQueue = false;
            searchBtn.disabled = false;
            pingBtn.disabled = false;
            renderOutput("It's your turn!", "success");

            if (queueInterval) {
                clearInterval(queueInterval);
                queueInterval = null;
            }
        }
        else {
            inQueue = true;
            searchBtn.disabled = true;
            pingBtn.disabled = true;
            renderOutput(data, "info");
        }
    } catch (err) {
        renderOutput("[ERROR] Queue check failed: " + err.message, "error");
    }
}

class Highlighter {
    static SNIPPETS_PER_PAGE = 15;
    static CONTEXT_CHARS = 50;

    static isTokenChar(char) {
        return /[a-zA-Z0-9_]/.test(char);
    }

    static extractLastWord(text) {
        let lastWord = "";
        let currentWord = "";

        for (let char of text) {
            if (Highlighter.isTokenChar(char)) {
                currentWord += char.toLowerCase();
            }
            else {
                if (currentWord) {
                    lastWord = currentWord;
                    currentWord = "";
                }
            }
        }
        if (currentWord)
            lastWord = currentWord;
        return lastWord;
    }

    static buildSnippet(content, position, phrase) {
        let highlightStart = position;
        let highlightEnd = position + phrase.length;

        const lastWord = Highlighter.extractLastWord(phrase);
        if (lastWord) {
            let i = highlightStart;
            while (i < content.length) {
                if (Highlighter.isTokenChar(content[i])) {
                    let j = i;
                    let candidate = "";
                    while (j < content.length && Highlighter.isTokenChar(content[j])) {
                        candidate += content[j].toLowerCase();
                        j++;
                    }
                    if (candidate === lastWord) {
                        highlightEnd = j;
                        break;
                    }
                    i = j;
                }
                else
                    i++;
            }
        }

        const beforeStart = Math.max(0, highlightStart - Highlighter.CONTEXT_CHARS);
        const afterEnd = Math.min(content.length, highlightEnd + Highlighter.CONTEXT_CHARS);

        const before = "..." + content.slice(beforeStart, highlightStart);
        const match = content.slice(highlightStart, highlightEnd);
        const after = content.slice(highlightEnd, afterEnd) + "...";

        return { before, match, after };
    }
}

class FileListView {
    constructor(phrase, files) {
        this.phrase = phrase;
        this.files = files;
        this.currentPage = 0;
        this.perPage = 15;
        this.render();
    }

    render() {
        out.innerHTML = "";
        const totalPages = Math.ceil(this.files.length / this.perPage);

        if (!this.files || this.files.length === 0) {
            const msg = document.createElement("div");
            msg.className = "error";
            msg.textContent = `No documents found for "${this.phrase}"`;
            out.appendChild(msg);
            return;
        }

        const start = this.currentPage * this.perPage;
        const pageFiles = this.files.slice(start, start + this.perPage);

        const header = document.createElement("h3");
        header.textContent = `--- Files page ${this.currentPage + 1}/${totalPages} ---`;
        out.appendChild(header);

        const ul = document.createElement("ul");
        ul.className = "file-list";

        pageFiles.forEach((file, idx) => {
            const li = document.createElement("li");
            li.innerHTML = `<b>[${start + idx}]</b> ${file.path} <span class="count">(${file.positions.length} matches)</span>`;
            li.onclick = () => this.openFile(file);
            ul.appendChild(li);
        });
        out.appendChild(ul);

        if (totalPages > 1) {
            const menu = document.createElement("div");
            menu.className = "menu";
            menu.innerHTML = `<button>[p] prev</button> <button>[n] next</button>`;
            menu.children[0].onclick = () => this.prevPage();
            menu.children[1].onclick = () => this.nextPage();
            out.appendChild(menu);
        }
    }

    prevPage() {
        const total = Math.ceil(this.files.length / this.perPage);
        this.currentPage = (this.currentPage - 1 + total) % total;
        this.render();
    }

    nextPage() {
        const total = Math.ceil(this.files.length / this.perPage);
        this.currentPage = (this.currentPage + 1) % total;
        this.render();
    }

    async openFile(file) {
        try {
            const content = await loadFileContent(file.path);
            new SnippetView(this.phrase, file, content, this.files);
        } catch (err) {
            renderOutput(err.message, "error");
        }
    }
}

class SnippetView {
    constructor(phrase, file, content, allFiles) {
        this.phrase = phrase;
        this.file = file;
        this.content = content;
        this.positions = file.positions;
        this.allFiles = allFiles;
        this.currentPage = 0;
        this.perPage = 15;
        this.render();
    }

    render() {
        const totalPages = Math.ceil(this.positions.length / this.perPage);
        out.innerHTML = `<h3>--- Snippets from ${this.file.path} (page ${this.currentPage + 1}/${totalPages}) ---</h3>`;
        const start = this.currentPage * Highlighter.SNIPPETS_PER_PAGE;
        const pagePos = this.positions.slice(start, start + Highlighter.SNIPPETS_PER_PAGE);

        const ul = document.createElement("ul");
        ul.className = "snippet-list";

        pagePos.forEach((pos, i) => {
            const { before, match, after } = Highlighter.buildSnippet(this.content, pos, this.phrase);
            const li = document.createElement("li");
            li.className = "snippet";
            li.innerHTML = `[${start + i}] ${before}<span class="match">${match}</span>${after}`;
            ul.appendChild(li);
        });

        out.appendChild(ul);

        const menu = document.createElement("div");
        menu.className = "menu";

        let buttons = "";
        if (totalPages > 1) {
            buttons += `<button>[p] prev</button> <button>[n] next</button> `;
        }
        buttons += `<button>← Back</button>`;
        menu.innerHTML = buttons;

        if (totalPages > 1) {
            menu.children[0].onclick = () => this.prevPage();
            menu.children[1].onclick = () => this.nextPage();
            menu.children[2].onclick = () => new FileListView(this.phrase, this.allFiles);
        }
        else
            menu.children[0].onclick = () => new FileListView(this.phrase, this.allFiles);
        out.appendChild(menu);
    }

    prevPage() {
        const total = Math.ceil(this.positions.length / this.perPage);
        this.currentPage = (this.currentPage - 1 + total) % total;
        this.render();
    }

    nextPage() {
        const total = Math.ceil(this.positions.length / this.perPage);
        this.currentPage = (this.currentPage + 1) % total;
        this.render();
    }
}