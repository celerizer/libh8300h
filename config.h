#ifndef H8_CONFIG_H
#define H8_CONFIG_H

#ifndef H8_BIG_ENDIAN
/**
 * Whether or not the target compilation platform uses big-endian byte order
 * Swaps the layout of some data structures
 */
#define H8_BIG_ENDIAN 0
#endif

#ifndef H8_NO_DMA
#define H8_NO_DMA 0
#endif

#ifndef H8_SAFETY
/**
 * Enables some additional error handling and bounds checking for situations
 * that should never happen
 */
#define H8_SAFETY 0
#endif

#ifndef H8_PROFILING
#define H8_PROFILING 1
#endif

#ifndef H8_REVERSE_BITFIELDS
/**
 * Reverses bitfields for compilers where the most significant bit is defined first
 */
#define H8_REVERSE_BITFIELDS 0
#endif

#ifndef H8_TESTS
#define H8_TESTS 1
#endif

#endif
