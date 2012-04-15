#if !defined(LE_ARCHITECTURE_H)
#define LE_ARCHITECTURE_H

#if WINDOWS
#include <sys/param.h>          /* #define BYTE_ORDER */
#endif

/* This is copied directly from GNUnet headers */

#ifndef __BYTE_ORDER
#ifdef _BYTE_ORDER
#define __BYTE_ORDER _BYTE_ORDER
#else
#ifdef BYTE_ORDER
#define __BYTE_ORDER BYTE_ORDER
#endif
#endif
#endif
#ifndef __BIG_ENDIAN
#ifdef _BIG_ENDIAN
#define __BIG_ENDIAN _BIG_ENDIAN
#else
#ifdef BIG_ENDIAN
#define __BIG_ENDIAN BIG_ENDIAN
#endif
#endif
#endif
#ifndef __LITTLE_ENDIAN
#ifdef _LITTLE_ENDIAN
#define __LITTLE_ENDIAN _LITTLE_ENDIAN
#else
#ifdef LITTLE_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#endif
#endif
#endif

/**
 * Endian operations
 */

# if __BYTE_ORDER == __LITTLE_ENDIAN
#  define LE_htobe16(x) __bswap_16 (x)
#  define LE_htole16(x) (x)
#  define LE_be16toh(x) __bswap_16 (x)
#  define LE_le16toh(x) (x)

#  define LE_htobe32(x) __bswap_32 (x)
#  define LE_htole32(x) (x)
#  define LE_be32toh(x) __bswap_32 (x)
#  define LE_le32toh(x) (x)

#  define LE_htobe64(x) __bswap_64 (x)
#  define LE_htole64(x) (x)
#  define LE_be64toh(x) __bswap_64 (x)
#  define LE_le64toh(x) (x)
#endif
# if __BYTE_ORDER == __BIG_ENDIAN
#  define LE_htobe16(x) (x)
#  define LE_htole16(x) __bswap_16 (x)
#  define LE_be16toh(x) (x)
#  define LE_le16toh(x) __bswap_16 (x)

#  define LE_htobe32(x) (x)
#  define LE_htole32(x) __bswap_32 (x)
#  define LE_be32toh(x) (x)
#  define LE_le32toh(x) __bswap_32 (x)

#  define LE_htobe64(x) (x)
#  define LE_htole64(x) __bswap_64 (x)
#  define LE_be64toh(x) (x)
#  define LE_le64toh(x) __bswap_64 (x)
#endif




/**
 * gcc-ism to get packed structs.
 */
#define LE_PACKED __attribute__((packed))

/**
 * gcc-ism to get gcc bitfield layout when compiling with -mms-bitfields
 */
#ifdef __MINGW32__
#define LE_GCC_STRUCT_LAYOUT __attribute__((gcc_struct))
#else
#define LE_GCC_STRUCT_LAYOUT
#endif

/**
 * gcc-ism to document unused arguments
 */
#define LE_UNUSED __attribute__((unused))

/**
 * gcc-ism to document functions that don't return
 */
#define LE_NORETURN __attribute__((noreturn))

#if __GNUC__ > 3
/**
 * gcc 4.x-ism to pack structures even on W32 (to be used before structs)
 */
#define LE_NETWORK_STRUCT_BEGIN \
  _Pragma("pack(push)") \
  _Pragma("pack(1)")

/**
 * gcc 4.x-ism to pack structures even on W32 (to be used after structs)
 */
#define LE_NETWORK_STRUCT_END _Pragma("pack(pop)")
#else
#ifdef __MINGW32__
#error gcc 4.x or higher required on W32 systems
#endif
/**
 * Good luck, LE_PACKED should suffice, but this won't work on W32
 */
#define LE_NETWORK_STRUCT_BEGIN 

/**
 * Good luck, LE_PACKED should suffice, but this won't work on W32
 */
#define LE_NETWORK_STRUCT_END
#endif


#endif