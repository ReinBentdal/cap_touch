#pragma once

#include <stdint.h>

uint32_t ppi_new_group_find(void);
uint32_t ppi_connect(volatile uint32_t* eep, volatile uint32_t* tep);
void ppi_fork(uint32_t idx, volatile uint32_t* tep);