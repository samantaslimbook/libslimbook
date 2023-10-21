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

#include "slimbook.h"

#include <iostream>
#include <string>

using namespace std;

void show_help()
{
    cout<<"Slimbook control tool"<<endl;
    cout<<"Usage: slimbookctl [command]"<<endl;
    cout<<"\n"<<endl;
    cout<<"Commands:"<<endl;
    cout<<"info: display Slimbook model information"<<endl;
    cout<<"help: show this help"<<endl;
}

void show_info()
{
    cout<<"product:"<<slb_info_product_name()<<"\n";
    cout<<"vendor:"<<slb_info_board_vendor()<<"\n";
    cout<<"serial:"<<slb_info_product_serial()<<"\n";
    cout<<"model:0x"<<std::hex<<slb_info_get_model()<<"\n";
    cout<<"platform:0x"<<slb_info_get_platform()<<"\n";
    cout<<"module loaded:"<<slb_info_is_module_loaded();
    cout<<endl;
}

int main(int argc,char* argv[])
{
    string command;
    
    if (argc>1) {
        command = argv[1];
    }
    else {
    
    }
    
    if (command == "info") {
        show_info();
        return 0;
    }
    
    if (command == "help") {
        show_help();
        return 0;
    }
    
    return 0;
}
