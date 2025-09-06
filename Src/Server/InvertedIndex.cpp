#include "InvertedIndex.h"
#include <cctype>
#include <algorithm>
#include <future>
#include <iostream>
#include <mutex>

InvertedIndex::InvertedIndex(ThreadPool* pool) : _pool(pool)
{
    _index = make_shared<unordered_map<string, unordered_map<string, vector<Posting>>>>();
}

InvertedIndex::~InvertedIndex()
{
    if (_updateThread.joinable())
        _updateThread.join();
}

static bool IsTokenChar(unsigned const char currentChar)
{
    return isalnum(currentChar) || currentChar == '_';
}

string InvertedIndex::ToLowerLatin(const string& input)
{
    string lowered;
    lowered.reserve(input.size());
    for (unsigned char character : input)
        lowered.push_back((char)tolower(character));
    return lowered;
}

vector<pair<string, pair<int, int>>> InvertedIndex::TokenizeWithBothOffsets(const string& text)
{
    vector<pair<string, pair<int, int>>> tokens;
    string currentToken;
    currentToken.reserve(32);

    int startCharOffset = -1;
    int currentWordIndex = 0;

    for (int charIndex = 0; charIndex < text.size(); ++charIndex)
    {
        unsigned char character = text[charIndex];
        if (IsTokenChar(character))
        {
            if (currentToken.empty())
                startCharOffset = charIndex;
            currentToken.push_back(tolower(character));
        }
        else if (!currentToken.empty())
        {
            tokens.emplace_back(currentToken, make_pair(startCharOffset, currentWordIndex++));
            currentToken.clear();
        }
    }
    if (!currentToken.empty())
        tokens.emplace_back(currentToken, make_pair(startCharOffset, currentWordIndex));

    return tokens;
}

void InvertedIndex::GetNewFiles(const vector<Document>& documents)
{
    unique_lock lock(_indexPreparingMutex);
    _unIndexedDocuments.reserve(_unIndexedDocuments.size() + documents.size());
    _unIndexedDocuments.insert(_unIndexedDocuments.end(), documents.begin(), documents.end());
    lock.unlock();

    if (_isIndexingNow.load())
    {
        _wasAddedNewFiles.store(true);
        cout << "[INDEX] Indexing delayed for " << documents.size() << " documents" << endl;
        return;
    }

    if (_updateThread.joinable())
        _updateThread.join();

    _updateThread = thread([this] { UpdateIndex(); });
}

void InvertedIndex::UpdateIndex()
{
    _isIndexingNow.store(true);
    _wasAddedNewFiles.store(false);
    unique_lock indexPreparingLock(_indexPreparingMutex);
    vector<Document> indexingDocuments(_unIndexedDocuments);
    _unIndexedDocuments.clear();
    indexPreparingLock.unlock();

    if (indexingDocuments.empty())
    {
        _isIndexingNow.store(false);
        return;
    }

    vector<future<unordered_map<string, unordered_map<string, vector<Posting>>>>> futures;
    futures.reserve(indexingDocuments.size());

    cout << "[INDEX] Started indexing " << indexingDocuments.size() << " documents" << endl;

    auto startTime = chrono::high_resolution_clock::now();
    for (const auto& document : indexingDocuments)
    {
        futures.push_back(_pool->Submit([document]()
        {
            unordered_map<string, unordered_map<string, vector<Posting>>> localIndex;
            auto tokens = TokenizeWithBothOffsets(document.GetContent());

            for (const auto& token : tokens)
            {
                const string& word = token.first;
                const string& docPath = document.GetPath();
                auto charOff = token.second.first;
                auto wordOff = token.second.second;
                localIndex[word][docPath].push_back({charOff, wordOff});
            }
            return localIndex;
        }));
    }

    size_t docsMerged = 0;
    for (auto& fut : futures)
    {
        auto localIndex = fut.get();
        unique_lock indexLock(_indexMutex);
        {
            for (auto& [word, docMap] : localIndex)
            {
                for (auto& [docId, postings] : docMap)
                {
                    auto& dst = (*_index)[word][docId];
                    dst.insert(dst.end(), make_move_iterator(postings.begin()),
                        make_move_iterator(postings.end()));
                }
            }
        }
        ++docsMerged;
    }
    //indexLock.unlock();

    auto endTime = chrono::high_resolution_clock::now();
    auto durationMs = duration_cast<chrono::milliseconds>(endTime - startTime).count();

    _wasIndexedAtStart.store(true);

    cout << "[INDEX] Indexed: " << docsMerged << " documents in " << durationMs << " ms" << endl;

    _isIndexingNow.store(false);
    if (_wasAddedNewFiles.load())
        UpdateIndex();
}


void InvertedIndex::ShowIndex()
{
    unique_lock indexLock(_indexMutex);
    for (const auto& wordEntry : (*_index))
    {
        cout << wordEntry.first << endl;
        for (const auto& documentEntry : wordEntry.second)
        {
            cout << "  Doc " << documentEntry.first << ": ";
            for (const auto& posting : documentEntry.second)
            {
                cout << "(char=" << posting.charOffset
                    << ", word=" << posting.wordOffset << ") ";
            }
            cout << endl;
        }
        cout << endl;
    }
    indexLock.unlock();
}

bool InvertedIndex::IsIndexedAtStart()
{
    return _wasIndexedAtStart.load();
}

unordered_map<string, vector<Posting>> InvertedIndex::SearchWord(const string& word) const
{
    unique_lock indexLock(_indexMutex);
    auto snapshot = _index;
    indexLock.unlock();

    string loweredWord = ToLowerLatin(word);
    auto indexIterator = snapshot->find(loweredWord);
    if (indexIterator == snapshot->end())
        return {};
    return indexIterator->second;
}

unordered_map<string, vector<Posting>> InvertedIndex::SearchPhrase(const string& phrase) const
{
    unique_lock indexLock(_indexMutex);
    auto snapshot = _index;
    indexLock.unlock();

    auto tokens = TokenizeWithBothOffsets(phrase);
    if (tokens.empty())
        return {};

    vector<string> words;
    words.reserve(tokens.size());
    for (auto& tokenPair : tokens)
        words.push_back(tokenPair.first);

    auto firstWordIterator = snapshot->find(words[0]);
    if (firstWordIterator == snapshot->end())
        return {};

    unordered_map<string, vector<Posting>> result;

    for (const auto& firstWordDocEntry : firstWordIterator->second)
    {
        const string& documentId = firstWordDocEntry.first;
        const auto& firstWordPositions = firstWordDocEntry.second;

        vector<const vector<Posting>*> positionsLists;
        positionsLists.reserve(words.size());
        positionsLists.push_back(&firstWordPositions);

        bool documentContainsAllWords = true;
        for (size_t wordIndex = 1; wordIndex < words.size(); ++wordIndex)
        {
            auto wordIterator = snapshot->find(words[wordIndex]);
            if (wordIterator == snapshot->end())
            {
                documentContainsAllWords = false;
                break;
            }
            auto docIterator = wordIterator->second.find(documentId);
            if (docIterator == wordIterator->second.end())
            {
                documentContainsAllWords = false;
                break;
            }
            positionsLists.push_back(&docIterator->second);
        }
        if (!documentContainsAllWords)
            continue;

        vector<Posting> matches;
        for (const auto& firstPosting : *positionsLists[0])
        {
            bool isSequenceValid = true;
            int baseWordOffset = firstPosting.wordOffset;
            for (size_t wordIndex = 1; wordIndex < positionsLists.size(); ++wordIndex)
            {
                const auto& candidateList = *positionsLists[wordIndex];
                auto candidateIt = find_if(candidateList.begin(), candidateList.end(),
                                           [=](const Posting& posting)
                                           {
                                               return posting.wordOffset == baseWordOffset + (int)wordIndex;
                                           });
                if (candidateIt == candidateList.end())
                {
                    isSequenceValid = false;
                    break;
                }
            }
            if (isSequenceValid)
                matches.push_back(firstPosting);
        }

        if (!matches.empty())
            result.emplace(documentId, matches);
    }

    return result;
}