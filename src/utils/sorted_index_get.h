/*
 * File: sorted_index_get.h
 * Author: Rein Gundersen Bentdal
 * Created: 19.Des 2024
 * Description: Finds the n'th largest value in the array without performing a sort
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