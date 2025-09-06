#include "FileWatcher.h"

FileWatcher::FileWatcher(const string& path, const unordered_set<string> &initializedDocs, int intervalSec)
    : _trackPath(path), _intervalSeconds(intervalSec), _operatedFiles(initializedDocs) {}

vector<string> FileWatcher::CheckForNewFiles() {
    vector<string> newFiles;
    const path root = path(_trackPath);
    if (!exists(root) || !is_directory(root))
        return newFiles;

    vector<directory_entry> entries = GetAllFilesFromDir(root);
    sort(entries.begin(), entries.end(),
         [](const auto& a, const auto& b) {
             return a.path().filename().string() < b.path().filename().string();
         });

    for (const auto& entry : entries) {
        const string path = entry.path().string();
        if (!_operatedFiles.contains(path)) {
            _operatedFiles.insert(path);
            newFiles.push_back(path);
        }
    }
    return newFiles;
}

void FileWatcher::RunLoop(function<void(const vector<string>&)> onNew) {
    while (_isRunning.load()) {
        auto files = CheckForNewFiles();
        if (!files.empty() && onNew)
            onNew(files);
        this_thread::sleep_for(chrono::seconds(_intervalSeconds));
    }
}

vector<directory_entry> FileWatcher::GetAllFilesFromDir(const path& path) {
    vector<directory_entry> entries;
    for (const auto& entry : directory_iterator(path))
        if (entry.is_regular_file())
            entries.push_back(entry);
        else if (entry.is_directory())
            for (const auto& subEntry : GetAllFilesFromDir(entry.path()))
                entries.emplace_back(subEntry);

    return entries;
}

void FileWatcher::Start(function<void(const vector<string>&)> onNew) {
    if (_isRunning.exchange(true))
        return;
    _worker = thread([this, onNew] { RunLoop(onNew); });
}

void FileWatcher::Stop() {
    if (!_isRunning.exchange(false))
        return;
    if (_worker.joinable())
        _worker.join();
}

const string& FileWatcher::WatchedPath() const {
    return _trackPath;
}