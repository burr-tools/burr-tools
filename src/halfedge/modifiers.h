#ifndef __MODIFIERS_H__
#define __MODIFIERS_H__

class Polyhedron;

void bevelPolyhedron(Polyhedron & poly, float val);
void offsetPolyhedron(Polyhedron & poly, float val);
void scalePolyhedron(Polyhedron & poly, float val);
void fillPolyhedronHoles(Polyhedron &poly, bool fillOutsides);

#endif
