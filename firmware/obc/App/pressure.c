#include "pressure.h"

bool pressure_is_valid(int32_t pressure_pa)
{
    return pressure_pa > 1000 && pressure_pa < 120000;
}
