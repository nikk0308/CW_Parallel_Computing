#include "SearchClient.h"
#include <iostream>
#include <string>

using namespace std;

int main() {
    try {
        SearchClient client("127.0.0.1", 9090);
        client.Connect();
        client.Disconnect();
    }
    catch (const exception& ex) {
        cerr << "[FATAL] " << ex.what() << endl;
        return 1;
    }
    return 0;
}