
#include "slimbook.h"

#include <iostream>

using namespace std;

int main(int argc,char* argv[])
{
    cout<<"product:["<<slb_product_name()<<"]"<<endl;
    cout<<"vendor:["<<slb_board_vendor()<<"]"<<endl;
    return 0;
}
