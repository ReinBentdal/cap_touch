/*
 * File: ppi_connect.c
 * Author: Rein Gundersen Bentdal
 * Created: 19.Des 2024
 *
 * Copyright (c) 2024, Rein Gundersen Bentdal
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "ppi_connect.h"

#include <zephyr/kernel.h>
#include "nrf.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ppi_connect, LOG_LEVEL_INF);

#define NUM_PPI_CHANNELS 20
#define NUM_PPI_GROUPS 5

uint32_t ppi_new_group_find(void) {
    static uint8_t internal_used_groups = 0;
    uint32_t index = 0;
    
    while (index < NUM_PPI_GROUPS) {
        if ((NRF_PPI->CHG[index] == 0) && ((internal_used_groups & (1U << index)) == 0)) {
            internal_used_groups |= (1U << index);
            return index;
        }
        index++;
    }
    
    __ASSERT(0, "No available PPI group");
    return 0;
}

uint32_t ppi_connect(volatile uint32_t* eep, volatile uint32_t* tep) {
    static uint32_t internal_used_channels = 0;
    uint32_t ppi_index = 0;
    while ((NRF_PPI->CHEN & (1U << ppi_index)) && ((internal_used_channels & (1U << ppi_index))) && (NRF_PPI->CH[ppi_index].EEP != (uint32_t)NULL || NRF_PPI->CH[ppi_index].TEP != (uint32_t)NULL)) {
        ppi_index++;
        __ASSERT(ppi_index < NUM_PPI_CHANNELS, "No available PPI channel");
    }
    internal_used_channels |= 1 << ppi_index;
    LOG_DBG("Found channel %d", ppi_index);

    // __ASSERT((NRF_PPI->CHEN & (1 << ppi_index) == 0), "PPI channel %d already in use", ppi_index);
    __ASSERT((uint32_t*)NRF_PPI->CH[ppi_index].EEP == NULL, "PPI channel %d already in use", ppi_index);
    __ASSERT((uint32_t*)NRF_PPI->CH[ppi_index].TEP == NULL, "PPI channel %d already in use", ppi_index);
    NRF_PPI->CH[ppi_index].EEP = (uint32_t)eep;
    NRF_PPI->CH[ppi_index].TEP = (uint32_t)tep;
    NRF_PPI->CHENSET = 1 << ppi_index;
    return ppi_index;
}

void ppi_fork(uint32_t idx, volatile uint32_t* tep) {
    __ASSERT_NO_MSG(idx < ARRAY_SIZE(NRF_PPI->FORK));
    NRF_PPI->FORK[idx].TEP = (uint32_t)tep;
}
