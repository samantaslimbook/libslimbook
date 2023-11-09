/*
Copyright (C) 2023 Slimbook <dev@slimbook.es>

This file is part of libslimbook.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "configuration.h"

#include <iostream>
#include <sstream>
#include <iomanip>

#define DB_FILE "/var/lib/slimbook.db"

using namespace std;

Configuration::Configuration()
{

}


Configuration::~Configuration()
{

}

void Configuration::load()
{
    ifstream db;
    char buffer[256];
    
    db.open(DB_FILE);
    
    if (db.good()) {
    
        while (!db.eof()) {
            db.getline(buffer,256);
            string tmp = buffer;
            size_t sep = tmp.find(':');
            
            if (sep != std::string::npos) {
                string key = tmp.substr(0,sep);
                string value = tmp.substr(sep+1,tmp.size());
                clog<<"["<<key<<"]:["<<value<<"]"<<endl;
            }
        }
        
        db.close();
    }
}

void Configuration::store()
{
    ofstream db;
    
    db.open(DB_FILE);
    
    if (db.good()) {
        for (auto p : m_data) {
            
        }
        db.close();
    }
}

string Configuration::get(string key)
{
    return m_data[key];
}

uint32_t Configuration::get_u32(string key)
{
    uint32_t value = std::stoi(m_data[key],0,16);
    
    return value;
}

void Configuration::set(string key, string value)
{
    m_data[key] = value;
}

void Configuration::set_u32(string key, uint32_t value)
{
    stringstream ss;
    
    ss<<std::setfill('0')<<std::setw(8)<<value;
    m_data[key] = ss.str();
}

