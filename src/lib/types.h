#ifndef __TYPES_H__
#define __TYPES_H__

#include <sys/types.h>

/**
 * the type used for one voxel, \c u_int8_t
 * allows up to 255 differen pieces this should
 * be enough for almost all puzzles, but just for
 * the case it isn't we can easily change the type
 */
#ifdef WIN32
typedef unsigned char voxel_type;
#else
typedef u_int8_t voxel_type;
#endif

#endif
