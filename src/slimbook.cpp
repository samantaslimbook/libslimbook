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

#include <string>
#include <cstring>
#include <fstream>
#include <thread>

using namespace std;

#define SYSFS_DMI "/sys/devices/virtual/dmi/id/"

thread_local std::string buffer;

static void read_device(string path,string& out)
{
    ifstream file;

    file.open(path.c_str());
    std::getline(file,out);
    file.close();
}

const char* slb_info_product_name()
{
    buffer.clear();

    read_device(SYSFS_DMI"product_name",buffer);

    return buffer.c_str();
}

const char* slb_info_board_vendor()
{
    buffer.clear();

    read_device(SYSFS_DMI"board_vendor",buffer);

    return buffer.c_str();
}

const char* slb_info_serial_number()
{
    buffer.clear();

    read_device(SYSFS_DMI"serial_number",buffer);

    return buffer.c_str();
}
