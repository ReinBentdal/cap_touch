#include "ppi_index.h"

#include <zephyr/kernel.h>
#include "nrf.h"

inline int ppi_index_get(void) {
    static int ppi_index = 0;
    __ASSERT(ppi_index < ARRAY_SIZE(NRF_PPI->CH), "PPI index out of bounds");
    return ppi_index++;
}