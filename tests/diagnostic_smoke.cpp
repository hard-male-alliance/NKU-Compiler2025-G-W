#include <debug.h>

/** Exercise each fatal diagnostic with assertions disabled by the build target. */
int main(int argc, char**)
{
    if (argc > 1) ERROR("error-smoke");
    TODO("todo-smoke");
}
