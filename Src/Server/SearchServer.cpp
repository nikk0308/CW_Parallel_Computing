#include "SearchServer.h"
#include <stdexcept>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

static string Trim(const string& str) {
    size_t start = str.find_first_not_of(" \r\n\t");
    if (start == string::npos)
        return "";
    size_t end = str.find_last_not_of(" \r\n\t");
    return str.substr(start, end - start + 1);
}

SearchServer::SearchServer(const string& host, int port, int workersThreadsAmount, int clientsThreadsAmount,
    int refreshRate, int notifyIntervalSeconds)
    : _host(host), _port(port)
{
    path dataRoot(DATA_DIR_PATH);
    if (!exists(dataRoot) || !is_directory(dataRoot))
        throw runtime_error("[ERROR] Data folder not found at: " + dataRoot.string());

    _pool = new ThreadPool([this](SOCKET clientSocket) { HandleClient(clientSocket); },
    [this](SOCKET clientSock, int queuePlace) { NotifyClientQueuePlace(clientSock, queuePlace); },
        workersThreadsAmount, clientsThreadsAmount, notifyIntervalSeconds);
    _index = new InvertedIndex(_pool);
    _reader = new DocumentReader(dataRoot.string());

    Start();

    auto files = _reader->GetAllDirectoryFiles(dataRoot.string());
    if (files.empty())
        cerr << "[WARN] No files found in: " << dataRoot << endl;

    auto docs = _reader->LoadDocuments(files);
    _index->GetNewFiles(docs);

    unordered_set<string> indexedDocsPaths;
    indexedDocsPaths.reserve(docs.size());
    for (const auto& d : docs)
        indexedDocsPaths.insert(d.GetPath());

    _watcher = new FileWatcher(dataRoot.string(), indexedDocsPaths, refreshRate);
    _watcher->Start([this](const vector<string>& newFiles) {
        auto batch = _reader->LoadDocuments(newFiles);
        if (!batch.empty()) {
            cout << "[WATCHER] Added: " << batch.size() << " docs" << endl;
            _index->GetNewFiles(batch);
        }
    });
}

SearchServer::~SearchServer() {
    Stop();
    if (_watcher)
        _watcher->Stop();
    if (_pool)
        _pool->Shutdown();
}

void SearchServer::Start() {
    if (_isRunning.exchange(true))
        return;

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        throw runtime_error("[ERROR] WSAStartup failed");

    _listenSock = CreateListenSocket(_host, _port);
    if (_listenSock == INVALID_SOCKET) {
        WSACleanup();
        throw runtime_error("[ERROR] Failed to create listen socket");
    }
    _acceptThread = thread([this]{ AcceptLoop(); });

    cout << "Server started on " << _host << ":" << _port << " with " << _pool->GetThreadCount() << " threads" << endl;
}

void SearchServer::Stop() {
    if (!_isRunning.exchange(false))
        return;

    if (_listenSock != INVALID_SOCKET) {
        CloseSocket(_listenSock);
        _listenSock = INVALID_SOCKET;
    }

    if (_acceptThread.joinable())
        _acceptThread.join();

    WSACleanup();
    cout << "Server stopped" << endl;
}

void SearchServer::AcceptLoop() {
    while (_isRunning.load()) {
        SOCKET client = AcceptSocket(_listenSock);
        if (client == INVALID_SOCKET) {
            this_thread::sleep_for(chrono::milliseconds(50));
            continue;
        }

        _pool->AddNewClient(client);
    }
}

void SearchServer::HandleClient(SOCKET clientSock) {
    string incomingLine;
    if (!WriteAll(clientSock, "start\n"))
    {
        CloseSocket(clientSock);
        return;
    }

    while (_isRunning.load() && ReadLine(clientSock, incomingLine)) {
        string response = HandleCommand(Trim(incomingLine));

        if (!response.empty() && response.back() != '\n')
            response.push_back('\n');

        if (!WriteAll(clientSock, response))
            break;
    }
    CloseSocket(clientSock);
}

void SearchServer::NotifyClientQueuePlace(SOCKET clientSock, int queuePlace)
{
    WriteAll(clientSock, "[INFO] You are #" + to_string(queuePlace) + " in queue, wait a little bit!\n");
}

string SearchServer::HandleCommand(const string& command) {
    if (command.rfind("search ", 0) == 0)
        return CommandSearch(command.substr(7));
    if (command == "ping")
        return "pong";
    return "[!] Unknown command";
}

string SearchServer::CommandSearch(const string& phrase) {
    if (!_index->IsIndexedAtStart())
        return "in process";

    auto searchResult = _index->SearchPhrase(phrase);

    ostringstream outStream;
    outStream << "OK " << searchResult.size() << '\n';

    for (const auto& documentEntry : searchResult) {
        const string& documentPath = documentEntry.first;
        const auto& positionsVector = documentEntry.second;

        outStream << documentPath << '\t';
        for (size_t posIndex = 0; posIndex < positionsVector.size(); ++posIndex) {
            if (posIndex)
                outStream << ',';
            outStream << positionsVector[posIndex].charOffset;
        }
        outStream << '\n';
    }
    return outStream.str();
}

SOCKET SearchServer::CreateListenSocket(const string& socketHost, int socketPort) {
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET)
        return INVALID_SOCKET;

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)socketPort);
    address.sin_addr.s_addr = socketHost.empty() ? htonl(INADDR_ANY) : inet_addr(socketHost.c_str());

    int opt = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    if (bind(listenSocket, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        closesocket(listenSocket);
        return INVALID_SOCKET;
    }
    if (listen(listenSocket, 64) == SOCKET_ERROR) {
        closesocket(listenSocket);
        return INVALID_SOCKET;
    }
    return listenSocket;
}

SOCKET SearchServer::AcceptSocket(SOCKET listenSock) {
    if (listenSock == INVALID_SOCKET)
        return INVALID_SOCKET;
    SOCKET acceptSocket = accept(listenSock, nullptr, nullptr);
    if (acceptSocket == INVALID_SOCKET)
        return INVALID_SOCKET;

    return acceptSocket;
}

bool SearchServer::ReadLine(SOCKET sock, string& out) {
    out.clear();
    char receivedChar;
    while (true) {
        int bytesReceived = recv(sock, &receivedChar, 1, 0);
        if (bytesReceived <= 0)
            return false;
        if (receivedChar == '\n')
            break;
        if (receivedChar != '\r')
            out.push_back(receivedChar);
    }
    return true;
}

bool SearchServer::WriteAll(SOCKET sock, const string& data) {
    const char* pointer = data.data();
    size_t left = data.size();
    while (left > 0) {
        int sent = send(sock, pointer, (int)left, 0);
        if (sent <= 0)
            return false;
        pointer += sent;
        left -= (size_t)sent;
    }
    return true;
}

void SearchServer::CloseSocket(SOCKET socket) {
    if (socket != INVALID_SOCKET)
        closesocket(socket);
}
