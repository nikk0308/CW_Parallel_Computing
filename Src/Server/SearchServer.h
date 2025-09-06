#ifndef CW_PARALLEL_COMPUTING_SEARCHSERVER_H
#define CW_PARALLEL_COMPUTING_SEARCHSERVER_H

#include "InvertedIndex.h"
#include "ThreadPool.h"
#include "FileWatcher.h"
#include "DocumentReader.h"

#include <winsock2.h>
#include <string>
#include <thread>
#include <atomic>

using namespace std;

class SearchServer {
private:
    string _host;
    int _port;

    InvertedIndex* _index;
    ThreadPool* _pool;
    FileWatcher* _watcher;
    DocumentReader* _reader;

    atomic<bool> _isRunning{false};
    thread _acceptThread;
    SOCKET _listenSock{INVALID_SOCKET};

    void AcceptLoop();
    void HandleClient(SOCKET clientSock);
    void NotifyClientQueuePlace(SOCKET clientSock, int queuePlace);

    string HandleCommand(const string &command);
    string CommandSearch(const string &phrase);

    static SOCKET CreateListenSocket(const string &host, int port);
    static SOCKET AcceptSocket(SOCKET listenSock);
    static bool ReadLine(SOCKET sock, string &out);
    static bool WriteAll(SOCKET sock, const string &data);
    static void CloseSocket(SOCKET socket);

public:
    SearchServer(const string& host, int port, int workersThreadsAmount, int clientsThreadsAmount,
        int refreshRate, int notifyIntervalSeconds);
    ~SearchServer();

    void Start();
    void Stop();
};

#endif //CW_PARALLEL_COMPUTING_SEARCHSERVER_H