#include "Document.h"
#include <fstream>
#include <sstream>
#include <iostream>

using namespace std;

Document::Document(int id, const string& filePath) : _docId(id), _path(filePath)
{
    LoadContent();
}

bool Document::LoadContent()
{
    ifstream file(_path);
    if (!file.is_open())
    {
        cerr << "[ERROR] Cannot open file: " << _path << endl;
        return false;
    }
    ostringstream buffer;
    buffer << file.rdbuf();
    _content = buffer.str();
    return true;
}

int Document::GetId() const
{
    return _docId;
}

const string& Document::GetPath() const
{
    return _path;
}

const string& Document::GetContent() const
{
    return _content;
}