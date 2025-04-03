#include "common.h"

using namespace std;

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>

bool find_file(string path, string file, string& out) {
    out = "";

    for (const auto &entryRecDir : filesystem::recursive_directory_iterator(path)) {
        string entryPath = entryRecDir.path().string();
        if (entryPath.find(file) != string::npos) {
            out = entryPath.substr(0, entryPath.size() - file.size());
            return true;
        }
    }

    return false;
}

bool find_in_file(string str, ifstream& f){
    string line;
    regex e(str);

    while (getline(f, line)) {
        if (regex_search(line, e)) {
            return true;
        }
    }

    return false;
}

bool find_in_filestr(string str, string path) {
    ifstream f;

    f.open(path.c_str());
    bool ret = find_in_file(str, f);
    
    f.close();

    return ret;
}

void read_device(string path, string &out) {
    ifstream file;

    file.open(path.c_str());
    std::getline(file, out);
    file.close();
}

void write_device(string path, string in) {
    ofstream file;

    file.open(path.c_str());
    file << in;
    file.close();
}
