/*
Copyright (C) 2025 Slimbook <dev@slimbook.es>

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

#ifndef SLB_COMMON_H
#define SLB_COMMON_H

#include <string>

/* Tries to find file recursively in path */
bool find_file(std::string path, std::string file, std::string& out);

/* Tries to find text in file passed by string */
bool find_in_filestr(std::string str, std::string path);

/* Tries to find text in file passed by a stream*/
bool find_in_file(std::string str, std::ifstream& f);

/* Reads from the device's file */
void read_device(std::string path, std::string& out);

/* Writes to the device's file */
void write_device(std::string in, std::string out);

#endif
