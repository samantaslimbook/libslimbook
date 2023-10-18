
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

const char* slb_product_name()
{
    buffer.clear();

    read_device(SYSFS_DMI"product_name",buffer);

    return buffer.c_str();
}

const char* slb_board_vendor()
{
    buffer.clear();

    read_device(SYSFS_DMI"board_vendor",buffer);

    return buffer.c_str();
}
