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

vector<string> get_modules()
{
    vector<string> modules;
    
    ifstream file;
    
    file.open("/proc/modules");
    
    while (file.good()) {
        string module_name;
        string tmp;
        
        file>>module_name;
        std::getline(file,tmp);
        modules.push_back(module_name);
    }
    
    file.close();
    
    return modules;
}
