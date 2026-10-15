#include <sylib.h>

/** Read, transform and print a value through the runtime linked by CMake. */
int main(void)
{
    starttime();
    putint(getint() + 1);
    putch('\n');
    stoptime();
    return 0;
}
