#include "ppi_connect.h"

#include <zephyr/kernel.h>
#include "nrf.h"

static int _index_get(void);

unsigned int ppi_connect(int eep, int tep) {
    const int ppi_index = _index_get();
    NRF_PPI->CH[ppi_index].EEP = eep;
    NRF_PPI->CH[ppi_index].TEP = tep;
    NRF_PPI->CHENSET = 1 << ppi_index;
    return ppi_index;
}

void ppi_fork(unsigned int idx, int tep) {
    __ASSERT_NO_MSG(idx < ARRAY_SIZE(NRF_PPI->FORK));
    NRF_PPI->FORK[idx].TEP = tep;
}

static int _index_get(void) {
    static int ppi_index = 0;
    __ASSERT(ppi_index < ARRAY_SIZE(NRF_PPI->CH), "PPI index out of bounds");
    return ppi_index++;
}