#pragma once

#include <stdint.h>
#include <stddef.h>

static inline uint16_t sorted_index_get(uint16_t *buf, size_t size, size_t index) {
    size_t i, j, less_count, equal_count;
    for (i = 0; i < size; i++) {
        less_count = 0;
        equal_count = 0;
        for (j = 0; j < size; j++) {
            if (buf[j] < buf[i])
                less_count++;
            else if (buf[j] == buf[i])
                equal_count++;
        }
        if (less_count <= index && (less_count + equal_count) > index)
            return buf[i];
    }
    return buf[0]; // Fallback (should not reach here)
}