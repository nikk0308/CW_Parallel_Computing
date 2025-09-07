const express = require("express");
const net = require("net");

const app = express();
app.use(express.json());

const SERVER_HOST = "127.0.0.1";
const SERVER_PORT = 9090;

let cppClient = null;
let buffer = "";
let ready = false;

let pendingQueue = [];
let lineBuffer = [];

let cmdChain = Promise.resolve();

let lastQueueLine = "";

const fileStream = require("fs");

app.use(express.static("public"));

app.listen(3000, () => {
    console.log("Web client running at http://localhost:3000");
    connectToCppServer();
});

app.get("/api/file", (req, res) => {
    const filePath = req.query.path;
    if (!filePath)
        return res.status(400).send("No path");
    fileStream.readFile(filePath, "utf8", (err, data) => {
        if (err)
            return res.status(500).send("Cannot read file");
        res.type("text/plain").send(data);
    });
});

app.get("/api/queue", (req, res) => {
    res.type("text/plain").send(ready ? ("start") : (lastQueueLine || "waiting..."));
});

app.post("/api/command", (req, res) => {
    const { command } = req.body || {};
    if (!command)
        return res.status(400).json({ error: "No command provided" });
    if (!cppClient || !ready)
        return res.status(503).json({ error: "Not connected / not ready" });

    enqueueCommand(async () => {
        console.log("[CLIENT] Sending:", command);
        cppClient.write(command + "\n");

        if (command === "ping") {
            const line = await waitForLine();
            return res.json({ type: "pong", message: line });
        }

        if (command.startsWith("search ")) {
            const header = await waitForLine();

            if (header === "in process")
                return res.json({ type: "info", message: "Server is preparing for work" });
            if (!header.startsWith("OK "))
                return res.json({ type: "error", message: header });

            const count = parseInt(header.split(" ")[1] || "0", 10);
            const lines = await waitForNLines(count);

            const files = [];
            for (const line of lines) {
                const tab = line.indexOf("\t");
                if (tab === -1)
                    continue;

                const path = line.slice(0, tab);
                const positionsCsv = line.slice(tab + 1);
                const positions = positionsCsv
                    ? positionsCsv.split(",").map((x) => parseInt(x, 10)).filter((n) => !isNaN(n))
                    : [];
                files.push({ path, positions });
            }

            files.sort((a, b) => b.positions.length - a.positions.length);
            return res.json({ type: "search", files });
        }

        const line = await waitForLine();
        return res.json({ type: "raw", message: line });
    }).catch((err) => {
        if (!res.headersSent)
            res.status(500).json({ error: err.message });
    });
});

function enqueueCommand(fn) {
    const job = cmdChain.then(() => fn(), () => fn());
    cmdChain = job.catch(() => {});
    return job;
}

function connectToCppServer() {
    cppClient = new net.Socket();

    cppClient.connect(SERVER_PORT, SERVER_HOST, () => {
        console.log(`[INFO] Connected to C++ server at ${SERVER_HOST}:${SERVER_PORT}`);
    });

    cppClient.on("data", (data) => {
        buffer += data.toString();

        while (true) {
            const index = buffer.indexOf("\n");
            if (index === -1)
                break;

            let line = buffer.slice(0, index);
            buffer = buffer.slice(index + 1);

            line = line.trim();
            if (!line)
                continue;

            if (!ready) {
                if (line === "start") {
                    ready = true;
                    console.log("[INFO] Handshake complete, client ready");
                } else {
                    lastQueueLine = line;
                    console.log("[SERVER]", line);
                }
                continue;
            }

            if (pendingQueue.length > 0) {
                const resolve = pendingQueue.shift();
                resolve(line);
            }
            else
                lineBuffer.push(line);
        }
    });

    cppClient.on("error", (err) => {
        console.error("[ERROR] TCP error:", err);
    });

    cppClient.on("close", () => {
        console.log("[WARN] Disconnected. Reconnecting in 3s...");
        ready = false;

        pendingQueue.forEach((r) => r("[DISCONNECTED]"));
        pendingQueue = [];
        lineBuffer = [];
        buffer = "";
        setTimeout(connectToCppServer, 3000);
    });
}

function waitForLine(timeoutMs = 120000) {
    return new Promise((resolve, reject) => {
        if (lineBuffer.length > 0) {
            const line = lineBuffer.shift();
            return resolve(line);
        }

        const resolver = (line) => {
            clearTimeout(timer);
            if (line === "[DISCONNECTED]")
                return reject(new Error("Disconnected from server"));
            resolve(line);
        };

        pendingQueue.push(resolver);

        const timer = setTimeout(() => {
            const i = pendingQueue.indexOf(resolver);
            if (i >= 0)
                pendingQueue.splice(i, 1);
            reject(new Error("Timeout waiting for line from server"));
        }, timeoutMs);
    });
}

async function waitForNLines(n, timeoutPerLineMs = 120000) {
    const arr = [];
    while (arr.length < n && lineBuffer.length > 0)
        arr.push(lineBuffer.shift());
    while (arr.length < n) {
        const line = await waitForLine(timeoutPerLineMs);
        arr.push(line);
    }
    return arr;
}