#ifndef CW_PARALLEL_COMPUTING_INVERTEDINDEX_H
#define CW_PARALLEL_COMPUTING_INVERTEDINDEX_H

#include "Document.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "ThreadPool.h"

using namespace std;

struct Posting {
    int charOffset;
    int wordOffset;
};

class InvertedIndex
{
private:
    shared_ptr<unordered_map<string, unordered_map<string, vector<Posting>>>> _index;
    mutable mutex _indexMutex;
    mutable mutex _indexPreparingMutex;
    ThreadPool* _pool;
    thread _updateThread;

    vector<Document> _unIndexedDocuments;
    atomic<bool> _isIndexingNow{false};
    atomic<bool> _wasAddedNewFiles{false};
    atomic<bool> _wasIndexedAtStart{false};

    static vector<pair<string, pair<int,int>>> TokenizeWithBothOffsets(const string& text);
    static string ToLowerLatin(const string& input);

public:
    InvertedIndex(ThreadPool* pool);
    ~InvertedIndex();

    void GetNewFiles(const vector<Document>& documents);
    void UpdateIndex();
    void ShowIndex();
    bool IsIndexedAtStart();

    unordered_map<string, vector<Posting>> SearchWord(const string& word) const;
    unordered_map<string, vector<Posting>> SearchPhrase(const string& phrase) const;
};

#endif //CW_PARALLEL_COMPUTING_INVERTEDINDEX_H
