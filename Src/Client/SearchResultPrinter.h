#ifndef CW_PARALLEL_COMPUTING_IML_SEARCHRESULTPRINTER_H
#define CW_PARALLEL_COMPUTING_IML_SEARCHRESULTPRINTER_H

#include <unordered_map>
#include <vector>
#include <string>

using namespace std;

class SearchResultPrinter {
private:
    static const int FILES_PER_PAGE = 20;
    static const int SNIPPETS_PER_PAGE = 10;
    static const int CONTEXT_CHARS = 30;

    static void PrintFileListPage(const vector<pair<string, int>>& sortedFiles, int page);
    static void ShowFileMenu(const string& filePath, const vector<int>& positions, const string& phrase);
    static void PrintSnippets(const string& filePath, const vector<int>& positions, const string& phrase, int page);

    static string LoadFileContent(const string& filePath);
    static void PrintSnippet(const string& content, int position, const string& phrase);

public:
    static void PrintFiles(const unordered_map<string, vector<int>>& results, const string& phrase);
    static void PrintHighlighted(const string& before, const string& phrase, const string& after);
};


#endif //CW_PARALLEL_COMPUTING_IML_SEARCHRESULTPRINTER_H