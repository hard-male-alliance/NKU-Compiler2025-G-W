#include <backend/target/registry.h>

/** Verify that object-library linking preserves both backend factory registrations. */
int main()
{
    using BE::Targeting::TargetRegistry;
    if (!TargetRegistry::getTarget("riscv64")) return 1;
    if (!TargetRegistry::getTarget("aarch64")) return 2;
    if (TargetRegistry::getTarget("not-a-target")) return 3;
    return 0;
}
