#ifndef __MODIFIERS_H__
#define __MODIFIERS_H__

class Polyhedron;

void scalePolyhedron(Polyhedron & poly, float val);
void fillPolyhedronHoles(Polyhedron &poly, bool fillOutsides);

// inverts the inv polyhedron and adds those faces to poly
void joinPolyhedronInverse(Polyhedron & poly, const Polyhedron & inv);

#endif
