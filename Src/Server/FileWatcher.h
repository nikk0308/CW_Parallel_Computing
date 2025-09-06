#ifndef CW_PARALLEL_COMPUTING_FILEWATCHER_H
#define CW_PARALLEL_COMPUTING_FILEWATCHER_H

#include <string>
#include <vector>
#include <unordered_set>
#include <functional>
#include <atomic>
#include <thread>
#include <filesystem>

using namespace std;
using namespace filesystem;

class FileWatcher {
private:
    string _trackPath;
    unordered_set<string> _operatedFiles;
    int _intervalSeconds;
    atomic<bool> _isRunning{false};
    thread _worker;

    void RunLoop(function<void(const vector<string>&)> onNew);
    vector<directory_entry> GetAllFilesFromDir(const path& path);

public:
    explicit FileWatcher(const string &path, const unordered_set<string> &initializedDocs, int intervalSec);

    void Start(function<void(const vector<string>&)> onNew);
    void Stop();

    vector<string> CheckForNewFiles();
    const string &WatchedPath() const;
};

#endif //CW_PARALLEL_COMPUTING_FILEWATCHER_H