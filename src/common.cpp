#include "common.h"

using namespace std;

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>

bool find_file(string path, string file, string *out) {
    *out = "";

    for (const auto &entryRecDir : filesystem::recursive_directory_iterator(path)) {
        string entryPath = entryRecDir.path().string();
        if (entryPath.find(file) != string::npos) {
            *out = entryPath.substr(0, entryPath.size() - file.size());
            return true;
        }
    }

    return false;
}

bool find_in_file(string str, FILE* f){
    if (f == nullptr) {
        return false;
    }

    char buf[256];
    regex e(str);

    while (fgets(buf, sizeof(buf), f) != nullptr) {
        if (regex_search(buf, e)) {
            return true;
        }
    }

}

bool find_in_filestr(string str, string path) {
    FILE *f = fopen(path.c_str(), "r+");
    bool ret = find_in_file(str, f);
    
    fclose(f);

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
