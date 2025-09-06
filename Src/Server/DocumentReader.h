#ifndef CW_PARALLEL_COMPUTING_DOCUMENTREADER_H
#define CW_PARALLEL_COMPUTING_DOCUMENTREADER_H

#include "Document.h"

#include <string>
#include <vector>

using namespace std;

class DocumentReader {
private:
    string _basePath;
    int _curIndex{0};

public:
    DocumentReader(const string& path);

    vector<Document> LoadDocuments(const vector<string>& files);
    vector<string> GetAllDirectoryFiles(const string& subDir);
};

#endif //CW_PARALLEL_COMPUTING_DOCUMENTREADER_H