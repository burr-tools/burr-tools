#ifndef __MILLABLE_H__
#define __MILLABLE_H__

class voxel_c;

/* a piece is notchable, if it can be made purely by cutting */
bool isNotchable(const voxel_c * v);

/* a piece is millable, if it contains no inside corners */
bool isMillable(const voxel_c * v);

#endif
