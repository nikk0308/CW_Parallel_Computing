#include "DocumentReader.h"
#include "Document.h"

#include <algorithm>
#include <filesystem>

using namespace std;
using namespace filesystem;

DocumentReader::DocumentReader(const string& path, ThreadPool* pool) : _basePath(path), _pool(pool) {}

vector<Document> DocumentReader::LoadDocuments(const vector<string>& files)
{
    vector<future<Document>> futures;
    vector<Document> result;
    int id = _curIndex;
    for (const auto& file : files)
    {
        //result.emplace_back(id++, file);
        futures.push_back(_pool->Submit([id, &file]() { return Document(id, file); }));
        id++;
    }

    for (auto& fut : futures)
        result.emplace_back(fut.get());

    _curIndex = id;
    return result;
}

vector<string> DocumentReader::GetAllDirectoryFiles(const string& subDir)
{
    vector<string> result;
    string dirPath = /*_basePath + "/" + */subDir;
    for (auto& filePath : directory_iterator(dirPath))
    {
        if (filePath.is_regular_file())
            result.push_back(filePath.path().string());
        else if (filePath.is_directory())
            for (auto& path : GetAllDirectoryFiles(filePath.path().string()))
                result.push_back(path);

    }
    sort(result.begin(), result.end());
    return result;
}