#ifndef CW_PARALLEL_COMPUTING_DOCUMENT_H
#define CW_PARALLEL_COMPUTING_DOCUMENT_H

#include <string>

using namespace std;

class Document
{
private:
    int _docId;
    string _path;
    string _content;

public:
    Document(int id, const string& filePath);
    bool LoadContent();
    int GetId() const;
    const string& GetPath() const;
    const string& GetContent() const;
};

#endif //CW_PARALLEL_COMPUTING_DOCUMENT_H
