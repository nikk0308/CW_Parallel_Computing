#ifndef CW_PARALLEL_COMPUTING_SEARCHCLIENT_H
#define CW_PARALLEL_COMPUTING_SEARCHCLIENT_H

#include <string>
#include <vector>
#include <unordered_map>
#include <winsock2.h>

using namespace std;

class SearchClient
{
private:
    string _host;
    int _port;
    SOCKET _sock{INVALID_SOCKET};

    unordered_map<string, vector<int>> _lastSearchResults;

    bool SendLine(const string& line);
    string ReadLine(SOCKET sock);

    bool ReceiveSearchResults(string& header, unordered_map<string, vector<int>>& outResults);

public:
    SearchClient(const string& host, int port);
    ~SearchClient();

    bool Connect();
    void Disconnect();

    void InfinityCommandLoop();
};

#endif // CW_PARALLEL_COMPUTING_SEARCHCLIENT_H
