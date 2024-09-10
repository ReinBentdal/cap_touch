/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _MACROS_H_
#define _MACROS_H_

#include <errno.h>


#define POW_2(exp) (1 << exp)

#define RETURN_ON_ERR(ret) \
	if (ret) { \
		return ret;	\
	}

#define RETURN_ON_ERR_MSG(ret, msg, ...) \
	if (ret) { \
		LOG_ERR(msg, ##__VA_ARGS__); \
		return; \
	}

#define RETURN_ON_WRN_MSG(ret, msg, ...) \
	if (ret) { \
		LOG_WRN(msg, ##__VA_ARGS__); \
		return; \
	}

#define PRINT_AND_OOPS(code)                                                                       \
	do {                                                                                       \
		LOG_ERR("ERR_CHK Err_code: [%d] @ line: %d\t", code, __LINE__);                    \
		k_oops();                                                                          \
	} while (0)

#define LOG_INF_IF(condition, msg, ...) \
	if ((condition)) { \
		LOG_INF(msg, ##__VA_ARGS__); \
	} \

#define LOG_WRN_IF(condition, msg, ...) \
	if ((condition)) { \
		LOG_WRN(msg, ##__VA_ARGS__); \
	} \

#define LOG_ERR_IF(condition, msg, ...) \
	if ((condition)) { \
		LOG_ERR(msg, ##__VA_ARGS__); \
	} \

#define ERR_CHK(err_code)                                                                          \
	do {                                                                                       \
		if (err_cod < 0) {                                                                    \
			PRINT_AND_OOPS(err_code);                                                  \
		}                                                                                  \
	} while (0)

#define ERR_CHK_MSG(err_code, msg, ...)                                                                 \
	do {                                                                                       \
		if (err_code < 0) {                                                                    \
			LOG_ERR(msg, ##__VA_ARGS__);                                                        \
			PRINT_AND_OOPS(err_code);                                                  \
		}                                                                                  \
	} while (0)

#if (defined(CONFIG_INIT_STACKS) && defined(CONFIG_THREAD_ANALYZER))

#define STACK_USAGE_PRINT(thread_name, p_thread)                                                   \
	do {                                                                                       \
		static uint64_t thread_ts;                                                         \
		size_t unused_space_in_thread_bytes;                                               \
		if (k_uptime_get() - thread_ts > CONFIG_PRINT_STACK_USAGE_MS) {                    \
			k_thread_stack_space_get(p_thread, &unused_space_in_thread_bytes);         \
			thread_ts = k_uptime_get();                                                \
			LOG_DBG("Unused space in %s thread: %d bytes", thread_name,                \
				unused_space_in_thread_bytes);                                     \
		}                                                                                  \
	} while (0)
#else
#define STACK_USAGE_PRINT(thread_name, p_stack)
#endif /* (defined(CONFIG_INIT_STACKS) && defined(CONFIG_THREAD_ANALYZER)) */

#endif /* _MACROS_H_ */
