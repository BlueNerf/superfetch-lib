#include "include/superfetch/superfetch.h"

#ifndef _WIN64
#error This can only be compiled for 64bit
#endif

int main() {
    Superfetch& superfetch = Superfetch::GetInstance();
}
