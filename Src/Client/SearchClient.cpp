#include "SearchClient.h"
#include "SearchResultPrinter.h"

#include <iostream>
#include <sstream>
#include <ws2tcpip.h>
#include <cctype>

#pragma comment(lib, "Ws2_32.lib")

SearchClient::SearchClient(const string& host, int port)
    : _host(host), _port(port) {}

SearchClient::~SearchClient() {
    Disconnect();
}

bool SearchClient::SendLine(const string& line) {
    if (_sock == INVALID_SOCKET)
        return false;

    string payload = line;
    if (payload.empty() || payload.back() != '\n')
        payload.push_back('\n');

    int sentBytes = send(_sock, payload.c_str(), (int)payload.size(), 0);
    return sentBytes > 0;
}

string SearchClient::ReadLine(SOCKET sock) {
    string lineOut;
    char receivedChar;
    while (true) {
        int bytesReceived = recv(sock, &receivedChar, 1, 0);
        if (bytesReceived <= 0)
            return "";
        if (receivedChar == '\n')
            break;
        if (receivedChar != '\r')
            lineOut.push_back(receivedChar);
    }
    return lineOut;
}

bool SearchClient::ReceiveSearchResults(string& header, unordered_map<string, vector<int>>& outResults) {
    outResults.clear();

    if (header.rfind("OK ", 0) != 0) {
        cout << header << endl;
        return false;
    }

    size_t spacePos = header.find(' ');
    if (spacePos == string::npos)
        return false;

    int documentsCount = 0;
    try {
        documentsCount = stoi(header.substr(spacePos + 1));
    } catch (...) {
        return false;
    }

    for (int i = 0; i < documentsCount; ++i) {
        string dataLine = ReadLine(_sock);
        if (dataLine.empty() && i < documentsCount)
            return false;

        size_t tabPos = dataLine.find('\t');
        string documentPath;
        vector<int> positions;

        if (tabPos == string::npos) {
            documentPath = dataLine;
        } else {
            documentPath = dataLine.substr(0, tabPos);
            string positionsCsv = dataLine.substr(tabPos + 1);

            string numberToken;
            stringstream tokenStream(positionsCsv);
            while (getline(tokenStream, numberToken, ',')) {
                if (numberToken.empty())
                    continue;
                try {
                    positions.push_back(stoi(numberToken));
                } catch (...) {
                }
            }
        }
        outResults.emplace(documentPath, positions);
    }
    return true;
}

void SearchClient::InfinityCommandLoop() {
    string input;

    while (true)
    {
        string singleLineResponse = ReadLine(_sock);
        if (singleLineResponse.empty()) {
            cout << "[ERROR] Disconnected" << endl;
            return;
        }
        if (singleLineResponse == "start")
            break;
        cout << singleLineResponse << endl;
    }

    cout << "[INFO] It's your turn!" << endl;
    while (true) {
        cout << endl << "Type commands:";
        cout << endl
                 << "[ping] check connection" << endl
                 << "[search <your phrase>] search your phrase in all documents" << endl
                 << "[quit] quit" << endl;
        cout << "> ";
        if (!getline(cin, input))
            break;

        if (input == "quit")
            break;

        if (!SendLine(input)) {
            cout << "[ERROR] Send failed" << endl;
            continue;
        }

        if (input.rfind("search ", 0) == 0) {
            string responseHeader = ReadLine(_sock);
            if (responseHeader == "in process")
                cout << "[INFO] Server is preparing for work" << endl;
            else if (ReceiveSearchResults(responseHeader, _lastSearchResults))
                SearchResultPrinter::PrintFiles(_lastSearchResults, input.substr(7));
            else
                cout << "[ERROR] Failed to receive search results" << endl;
        }
        else {
            string singleLineResponse = ReadLine(_sock);
            if (singleLineResponse.empty()) {
                cout << "[ERROR] Disconnected" << endl;
                break;
            }
            cout << singleLineResponse << endl;
        }
    }
}

bool SearchClient::Connect() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cerr << "[ERROR] WSAStartup failed" << endl;
        return false;
    }

    _sock = socket(AF_INET, SOCK_STREAM, 0);
    if (_sock == INVALID_SOCKET) {
        cerr << "[ERROR] Cannot create socket" << endl;
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(_port);
    inet_pton(AF_INET, _host.c_str(), &serverAddr.sin_addr);

    if (connect(_sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "[ERROR] Cannot connect to server " << _host << ":" << _port << endl;
        closesocket(_sock);
        _sock = INVALID_SOCKET;
        return false;
    }

    cout << "[INFO] Connected to " << _host << ":" << _port << endl;
    cout << "[INFO] Please wait until it's your turn" << endl;
    InfinityCommandLoop();
    return true;
}

void SearchClient::Disconnect() {
    if (_sock != INVALID_SOCKET) {
        closesocket(_sock);
        _sock = INVALID_SOCKET;
        WSACleanup();
        cout << "[INFO] Disconnected" << endl;
    }
}