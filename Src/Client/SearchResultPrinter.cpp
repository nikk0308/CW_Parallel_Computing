#include "SearchResultPrinter.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>

#include "SearchClient.h"

using namespace std;

void SearchResultPrinter::PrintFiles(const unordered_map<string, vector<int>>& results, const string& phrase) {
    if (results.empty()) {
        cout << "No documents found" << endl;
        return;
    }

    vector<pair<string, int>> sortedFiles;
    sortedFiles.reserve(results.size());
    for (const auto& entry : results)
        sortedFiles.emplace_back(entry.first, entry.second.size());

    sort(sortedFiles.begin(), sortedFiles.end(),
         [](const auto& a, const auto& b) {
             return a.second > b.second;
         });

    int totalPages = (sortedFiles.size() + FILES_PER_PAGE - 1) / FILES_PER_PAGE;
    int currentPage = 0;

    while (true) {
        PrintFileListPage(sortedFiles, currentPage);

        cout << endl;
        if (totalPages > 1)
            cout << "[n] next page" << endl << "[p] previous page" << endl;
        cout << "[<index>] open file by index" << endl
             << "[<path>] open file by path" << endl
             << "[q] quit to search" << endl;
        cout << "> ";

        string command;
        getline(cin, command);

        if (command == "q") {
            break;
        }
        else if (command == "n") {
            currentPage = (currentPage + 1 >= totalPages) ? 0 : currentPage + 1;
            continue;
        }
        else if (command == "p") {
            currentPage = (currentPage - 1 < 0) ? totalPages - 1 : currentPage - 1;
            continue;
        }

        try {
            int index = stoi(command);
            if (index >= 0 && index < sortedFiles.size()) {
                const auto& [filePath, _] = sortedFiles[index];
                ShowFileMenu(filePath, results.at(filePath), phrase);
            }
            else {
                cout << "[!] Invalid index" << endl;
            }
        } catch (...) {
            if (results.count(command)) {
                ShowFileMenu(command, results.at(command), phrase);
            }
            else {
                cout << "[!] Unknown command" << endl;
            }
        }
    }
}

void SearchResultPrinter::PrintHighlighted(const string& before, const string& phrase, const string& after) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    cout << before;

    SetConsoleTextAttribute(hConsole, BACKGROUND_RED | BACKGROUND_GREEN);
    cout << phrase;

    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    cout << after;
}

void SearchResultPrinter::PrintFileListPage(const vector<pair<string, int>>& sortedFiles, int page) {
    int start = page * FILES_PER_PAGE;
    int end = min((int)sortedFiles.size(), start + FILES_PER_PAGE);

    cout << "\n--- Files page " << (page + 1) << "/" << (int)ceil(sortedFiles.size() * 1.0 / FILES_PER_PAGE) << " ---" << endl;
    for (int i = start; i < end; ++i)
        cout << "[" << i << "] " << sortedFiles[i].first << " (" << sortedFiles[i].second << " matches)" << endl;
}

void SearchResultPrinter::ShowFileMenu(const string& filePath, const vector<int>& positions, const string& phrase) {
    string content = LoadFileContent(filePath);
    if (content.empty()) {
        cout << "[!] Failed to read file: " << filePath << endl;
        return;
    }

    int totalPages = (positions.size() + SNIPPETS_PER_PAGE - 1) / SNIPPETS_PER_PAGE;
    int currentPage = 0;

    PrintSnippets(filePath, positions, phrase, currentPage);
    while (true) {
        cout << endl;
        if (totalPages > 1)
            cout << "[n] next page" << endl << "[p] previous page" << endl;
        cout << "[q] quit to files" << endl;
        cout << "> ";

        string command;
        getline(cin, command);

        if (command == "q")
            break;
        else if (command == "n")
            currentPage = (currentPage + 1 >= totalPages) ? 0 : currentPage + 1;
        else if (command == "p")
            currentPage = (currentPage - 1 < 0) ? totalPages - 1 : currentPage - 1;
        else {
            cout << "[!] Unknown command" << endl;
            continue;
        }

        PrintSnippets(filePath, positions, phrase, currentPage);
    }
}

void SearchResultPrinter::PrintSnippets(const string& filePath, const vector<int>& positions, const string& phrase, int page) {
    int start = page * SNIPPETS_PER_PAGE;
    int end = min((int)positions.size(), start + SNIPPETS_PER_PAGE);

    string content = LoadFileContent(filePath);

    cout << "\n--- Snippets from " + filePath + " (page " + to_string(page + 1) + "/" +
        to_string((int)ceil(positions.size() * 1.0 / SNIPPETS_PER_PAGE)) + ") ---"  << endl;

    cout << string(75, '-') << endl;
    for (int i = start; i < end; ++i) {
        cout << "[" << i << "] ";
        PrintSnippet(content, positions[i], phrase);
        cout << endl;

        cout << string(75, '-') << endl;
    }
}

string SearchResultPrinter::LoadFileContent(const string& filePath) {
    ifstream file(filePath);
    if (!file.is_open())
        return "";

    return string(istreambuf_iterator<char>(file), istreambuf_iterator<char>());
}

static string ToLower(const string& str) {
    string res;
    res.reserve(str.size());
    for (unsigned char curChar : str)
        res.push_back(tolower(curChar));
    return res;
}

static bool IsTokenChar(unsigned char curChar) {
    return isalnum(curChar) || curChar == '_';
}

static string ExtractLastWord(const string& text) {
    string lastWord;
    string currentWord;

    for (unsigned char curChar : text) {
        if (IsTokenChar(curChar))
            currentWord.push_back(tolower(curChar));
        else {
            if (!currentWord.empty()) {
                lastWord = currentWord;
                currentWord.clear();
            }
        }
    }

    if (!currentWord.empty())
        lastWord = currentWord;

    return lastWord;
}

void SearchResultPrinter::PrintSnippet(const string& content, int position, const string& phrase) {
    string lastWord = ExtractLastWord(phrase);

    int highlightStart = position;
    int highlightEnd = position + phrase.size();

    if (!lastWord.empty()) {
        string lowered = ToLower(content);
        string loweredWord = ToLower(lastWord);

        size_t found = lowered.find(loweredWord, position);
        if (found != string::npos)
            highlightEnd = found + lastWord.size();
    }

    int beforeStart = max(0, highlightStart - CONTEXT_CHARS);
    int afterEnd = min((int)content.size(), highlightEnd + CONTEXT_CHARS);

    string before = "..." + content.substr(beforeStart, highlightStart - beforeStart);
    string match = content.substr(highlightStart, highlightEnd - highlightStart);
    string after = content.substr(highlightEnd, afterEnd - highlightEnd) + "...";

    PrintHighlighted(before, match, after);
}