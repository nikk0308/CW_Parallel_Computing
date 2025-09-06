#include "SearchServer.h"
#define WORKERS_THREADS_AMOUNT 5
#define CLIENTS_THREADS_AMOUNT 2
#define REFRESH_RATE 10
#define CLIENTS_NOTIFY_RATE 10

#include <iostream>
#include <filesystem>

using namespace std;
using namespace filesystem;

int main() {
    try {
        SearchServer server("0.0.0.0", 9090, WORKERS_THREADS_AMOUNT,
            CLIENTS_THREADS_AMOUNT, REFRESH_RATE, CLIENTS_NOTIFY_RATE);
        cin.get();
    }
    catch (const exception& ex) {
        cerr << "[FATAL] " << ex.what() << endl;
        return 1;
    }
    return 0;
}