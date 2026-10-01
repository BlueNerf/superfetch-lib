#include "include/superfetch/superfetch.h"

#include <conio.h>

#ifndef _WIN64
#error This can only be compiled for 64bit
#endif

int main() {
    Superfetch& superfetch = Superfetch::GetInstance();
    uintptr_t physical_addr = superfetch.translate(0xFFFFF78000000000);

    std::cout << physical_addr << std::endl;

    _getch();
}
