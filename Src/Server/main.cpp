#include "SearchServer.h"
#define REFRESH_RATE 10
#define CLIENTS_NOTIFY_RATE 10

#include <iostream>
#include <filesystem>

using namespace std;
using namespace filesystem;

int main() {
    try {
        int workersThreadsAmount;
        int clientsThreadsAmount;
        cout << "Input index builders threads amount: ";
        cin >> workersThreadsAmount;
        cout << "Input clients threads amount: ";
        cin >> clientsThreadsAmount;

        SearchServer server("0.0.0.0", 9090, workersThreadsAmount, clientsThreadsAmount,
            REFRESH_RATE, CLIENTS_NOTIFY_RATE);
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cin.get();
    }
    catch (const exception& ex) {
        cerr << "[FATAL] " << ex.what() << endl;
        return 1;
    }
    return 0;
}