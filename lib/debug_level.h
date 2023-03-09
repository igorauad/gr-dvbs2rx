/* -*- c++ -*- */
/*
 * Copyright (c) 2023 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_DEBUG_LEVEL_H
#define INCLUDED_DVBS2RX_DEBUG_LEVEL_H

namespace gr {
namespace dvbs2rx {

// Low-level debug logging controlled by level
#ifdef DEBUG_LOGS

#define GR_LOG_DEBUG_LEVEL(level, ...)    \
    do {                                  \
        if (d_debug_level >= level) {     \
            d_logger->debug(__VA_ARGS__); \
        }                                 \
    } while (0)

#define GR_LOG_DEBUG_LEVEL_IF(level, condition, ...) \
    do {                                             \
        if (d_debug_level >= level && condition) {   \
            d_logger->debug(__VA_ARGS__);            \
        }                                            \
    } while (0)

#else // DEBUG_LOGS

#define GR_LOG_DEBUG_LEVEL(level, ...) \
    while (false)                      \
    GR_LOG_DEBUG(__VA_ARGS__)

#define GR_LOG_DEBUG_LEVEL_IF(level, condition, ...) \
    while (false)                                    \
    GR_LOG_DEBUG(__VA_ARGS__)

#endif // DEBUG_LOGS

} // namespace dvbs2rx
} // namespace gr

#endif