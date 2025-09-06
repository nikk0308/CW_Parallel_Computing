#include "DocumentReader.h"
#include "Document.h"

#include <algorithm>
#include <filesystem>

using namespace std;
using namespace filesystem;

DocumentReader::DocumentReader(const string& path) : _basePath(path) {}

vector<Document> DocumentReader::LoadDocuments(const vector<string>& files)
{
    vector<Document> result;
    int id = _curIndex;
    for (auto& file : files)
        result.emplace_back(id++, file);

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