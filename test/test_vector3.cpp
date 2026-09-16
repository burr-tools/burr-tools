#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "halfedge/vector3.h"

#include <cmath>

using Catch::Approx;

/* Vector3D<float>: 211 uncovered lines of small, total functions and the
   cheapest real coverage in the repository, per the mesh/STL design doc.
   This is that document's step one -- it needs no mesh fixture at all.

   Only Vector3D<float> is instantiated (vector3.cpp's explicit
   instantiation list has every other type commented out), so everything
   here is Vector3Df.

   The assertions are identities wherever one exists -- the cross product is
   perpendicular to both operands, rotating by an angle and back is the
   identity, a projection lands in its plane -- rather than constants read
   off a calculator. An identity cannot be satisfied by an implementation
   that is consistently wrong in the way a recorded constant can. Where a
   constant genuinely is known (a unit vector's module is 1) it is used. */

namespace {

/* One tolerance, chosen once, as the design doc asks. It is relative:
   an absolute epsilon on values of wildly different magnitude produces
   cases that pass or fail depending on the fixture's scale, and the
   fixtures here range from unit vectors to coordinates in the hundreds.

   1e-5 rather than something tighter because these are floats, not
   doubles, and several of the functions route through double-precision
   trigonometry and back. */
constexpr double EPS = 1e-5;

Approx approx(double v) { return Approx(v).epsilon(EPS).margin(1e-6); }

bool nearlyEqual(const Vector3Df & a, const Vector3Df & b) {
  return a.x() == approx(b.x()) && a.y() == approx(b.y()) && a.z() == approx(b.z());
}

} // namespace

/* ------------------------------------------------------------------ */
/* construction and access                                             */
/* ------------------------------------------------------------------ */

TEST_CASE("vector3: the constructors agree on the components they store", "[vector3]") {
  const float data[3] = { 1.5f, -2.5f, 3.5f };

  Vector3Df fromComponents(1.5f, -2.5f, 3.5f);
  Vector3Df fromArray(data);
  Vector3Df fromCopy(fromComponents);
  Vector3Df fromPointer(&fromComponents);

  /* four separate constructor bodies that must produce the same vector --
     asserted through operator== so that is exercised too */
  REQUIRE(fromArray == fromComponents);
  REQUIRE(fromCopy == fromComponents);
  REQUIRE(fromPointer == fromComponents);

  /* and the components really are the ones handed in, in order, so a
     constructor that transposed two of them is caught rather than being
     consistent with the others */
  REQUIRE(fromComponents.x() == 1.5f);
  REQUIRE(fromComponents.y() == -2.5f);
  REQUIRE(fromComponents.z() == 3.5f);

  /* the default constructor is the zero vector */
  REQUIRE(Vector3Df() == Vector3Df(0, 0, 0));
}

TEST_CASE("vector3: indexed access reaches the same storage as the named accessors", "[vector3]") {
  Vector3Df v(1, 2, 3);

  REQUIRE(v[0] == v.x());
  REQUIRE(v[1] == v.y());
  REQUIRE(v[2] == v.z());

  /* the non-const overload is a distinct function body and is writable */
  v[1] = 20;
  REQUIRE(v.y() == 20);

  const Vector3Df & cv = v;
  REQUIRE(cv[1] == 20);
}

TEST_CASE("vector3: set replaces every component", "[vector3]") {
  Vector3Df v(1, 2, 3);

  v.set(4, 5, 6);
  REQUIRE(v == Vector3Df(4, 5, 6));

  const float data[3] = { 7, 8, 9 };
  v.set(data);
  REQUIRE(v == Vector3Df(7, 8, 9));
}

TEST_CASE("vector3: assignment copies and leaves the source alone", "[vector3]") {
  Vector3Df source(1, 2, 3);
  Vector3Df target(9, 9, 9);

  target = source;

  REQUIRE(target == source);

  /* independence, not aliasing -- without this the case holds against an
     operator= that stored a reference */
  target.set(4, 5, 6);
  REQUIRE(source == Vector3Df(1, 2, 3));
}

TEST_CASE("vector3: the ordering is a strict weak ordering over distinct vectors", "[vector3]") {
  Vector3Df a(1, 2, 3);
  Vector3Df b(1, 2, 4);
  Vector3Df c(1, 3, 0);

  /* exactly one of a < b and b < a holds for distinct vectors, and neither
     for equal ones -- the property a container built on operator< needs,
     and the one a comparison that forgot a component would break */
  REQUIRE(a < b);
  REQUIRE_FALSE(b < a);

  REQUIRE(b < c);
  REQUIRE_FALSE(c < b);

  REQUIRE_FALSE(a < a);

  /* and it is transitive on this triple */
  REQUIRE(a < c);
}

/* ------------------------------------------------------------------ */
/* arithmetic                                                          */
/* ------------------------------------------------------------------ */

TEST_CASE("vector3: addition and subtraction are inverse", "[vector3]") {
  Vector3Df a(1, 2, 3);
  Vector3Df b(10, -20, 30);

  REQUIRE(nearlyEqual((a + b) - b, a));

  /* the compound forms are separate function bodies from the value ones */
  Vector3Df acc(a);
  acc += b;
  REQUIRE(nearlyEqual(acc, a + b));

  acc -= b;
  REQUIRE(nearlyEqual(acc, a));

  /* and the named aliases route to the same places */
  REQUIRE(nearlyEqual(a.sum(b), a + b));
  REQUIRE(nearlyEqual(a.difference(b), a - b));

  /* unary negation */
  REQUIRE(nearlyEqual(-a, Vector3Df(-1, -2, -3)));
  REQUIRE(nearlyEqual(a + (-a), Vector3Df(0, 0, 0)));
}

TEST_CASE("vector3: scalar multiplication and division are inverse and commute with the free "
          "operator", "[vector3]") {
  Vector3Df a(1, 2, 3);

  REQUIRE(nearlyEqual((a * 4.0f) / 4.0f, a));

  /* the free operator*(factor, vector) is a separate template function from
     the member operator*(factor) */
  REQUIRE(nearlyEqual(4.0f * a, a * 4.0f));

  Vector3Df acc(a);
  acc *= 4.0f;
  REQUIRE(nearlyEqual(acc, a * 4.0f));

  acc /= 4.0f;
  REQUIRE(nearlyEqual(acc, a));
}

TEST_CASE("vector3: the dot product is symmetric and gives the squared module against itself",
          "[vector3]") {
  Vector3Df a(1, 2, 3);
  Vector3Df b(-4, 5, 6);

  REQUIRE(a * b == approx(b * a));
  REQUIRE(a * b == approx(1.0 * -4 + 2.0 * 5 + 3.0 * 6));

  /* the identity that ties the dot product to the module: a . a is |a|^2 */
  REQUIRE(a * a == approx(a.squaredModule()));
  REQUIRE(a.module() == approx(std::sqrt(a.squaredModule())));

  /* the named alias is the same operation */
  REQUIRE(a.dotProduct(b) == approx(a * b));

  /* perpendicular vectors have a zero dot product -- the case that gives
     the operation its meaning, and one a sign error would not disturb */
  REQUIRE(Vector3Df(1, 0, 0) * Vector3Df(0, 1, 0) == approx(0.0));
}

TEST_CASE("vector3: the cross product is perpendicular to both operands and anticommutes",
          "[vector3]") {
  Vector3Df a(1, 2, 3);
  Vector3Df b(-4, 5, 6);

  Vector3Df c = a ^ b;

  /* the defining property, and one no recorded constant is needed for */
  REQUIRE(c * a == approx(0.0));
  REQUIRE(c * b == approx(0.0));

  /* anticommutativity: swapping the operands negates the result. A cross
     product with two components transposed would still be perpendicular to
     one operand, so this is what pins the orientation. */
  REQUIRE(nearlyEqual(b ^ a, -c));

  /* right-handedness, fixed by the standard basis */
  REQUIRE(nearlyEqual(Vector3Df(1, 0, 0) ^ Vector3Df(0, 1, 0), Vector3Df(0, 0, 1)));

  /* a vector crossed with itself is zero */
  REQUIRE(nearlyEqual(a ^ a, Vector3Df(0, 0, 0)));

  REQUIRE(nearlyEqual(a.crossProduct(b), c));
}

TEST_CASE("vector3: the scalar triple product is invariant under cyclic permutation", "[vector3]") {
  Vector3Df a(1, 2, 3);
  Vector3Df b(-4, 5, 6);
  Vector3Df c(7, -8, 9);

  const double abc = a * (b ^ c);

  REQUIRE(b * (c ^ a) == approx(abc));
  REQUIRE(c * (a ^ b) == approx(abc));

  /* and swapping any two negates it, which distinguishes the cyclic
     invariance above from plain symmetry */
  REQUIRE(b * (a ^ c) == approx(-abc));

  /* the premise: the triple is not coplanar, or every product above is
     zero and all four assertions hold vacuously */
  REQUIRE(abc != approx(0.0));
}

/* ------------------------------------------------------------------ */
/* length                                                              */
/* ------------------------------------------------------------------ */

TEST_CASE("vector3: normalize keeps the direction and sets the module to one", "[vector3]") {
  Vector3Df v(3, 4, 12);
  const double before = v.module();

  REQUIRE(before == approx(13.0));

  Vector3Df unit(v);
  unit.normalize();

  REQUIRE(unit.module() == approx(1.0));

  /* direction preserved: the unit vector scaled back by the original
     module is the original. Asserting this rather than "each component
     divided by 13" catches a normalize that also permuted them. */
  REQUIRE(nearlyEqual(unit * (float)before, v));

  /* and it is parallel, not antiparallel -- a normalize that negated would
     satisfy both assertions above if they were written on modules alone */
  REQUIRE(unit * v == approx(before));
}

TEST_CASE("vector3: setLength rescales to the requested module without turning the vector",
          "[vector3]") {
  Vector3Df v(3, 4, 12);
  Vector3Df scaled(v);

  scaled.setLength(26.0f);

  REQUIRE(scaled.module() == approx(26.0));

  /* exactly twice the original, since 26 is twice 13 */
  REQUIRE(nearlyEqual(scaled, v * 2.0f));
}

TEST_CASE("vector3: the distance metrics agree with their definitions and with each other on the "
          "unit steps", "[vector3]") {
  Vector3Df a(1, 2, 3);
  Vector3Df b(4, 6, 15);

  /* b - a is (3, 4, 12), module 13 */
  REQUIRE(a.distance(b) == approx(13.0));
  REQUIRE(a.squaredDistance(b) == approx(169.0));

  /* Chebyshev: the largest single-axis difference */
  REQUIRE(a.infDistance(b) == approx(12.0));

  /* Manhattan: the sum of them */
  REQUIRE(a.manhattanDistance(b) == approx(19.0));

  /* all four are symmetric, which a metric must be and which an
     implementation that subtracted in one fixed order could get wrong */
  REQUIRE(b.distance(a) == approx(a.distance(b)));
  REQUIRE(b.squaredDistance(a) == approx(a.squaredDistance(b)));
  REQUIRE(b.infDistance(a) == approx(a.infDistance(b)));
  REQUIRE(b.manhattanDistance(a) == approx(a.manhattanDistance(b)));

  /* and all four vanish on a point against itself */
  REQUIRE(a.distance(a) == approx(0.0));
  REQUIRE(a.infDistance(a) == approx(0.0));
  REQUIRE(a.manhattanDistance(a) == approx(0.0));
}

TEST_CASE("vector3: the angle between vectors is symmetric and matches the axes it is measured "
          "between", "[vector3]") {
  Vector3Df x(1, 0, 0);
  Vector3Df y(0, 1, 0);

  REQUIRE(x.angle(y) == approx(M_PI / 2));
  REQUIRE(y.angle(x) == approx(M_PI / 2));

  REQUIRE(x.angle(x) == approx(0.0));
  REQUIRE(x.angle(-x) == approx(M_PI));

  /* the implementation clamps the dot product into [-1, 1] before acos,
     precisely so that a vector against itself does not fall out as NaN
     when rounding pushes the product a hair over 1. That clamp is what
     the two degenerate cases above exercise. */
  REQUIRE_FALSE(std::isnan(x.angle(x)));
  REQUIRE_FALSE(std::isnan(x.angle(-x)));

  /* the angle is independent of length -- it is a direction comparison */
  REQUIRE((x * 100.0f).angle(y * 0.01f) == approx(M_PI / 2));
}

/* ------------------------------------------------------------------ */
/* rotation                                                            */
/* ------------------------------------------------------------------ */

TEST_CASE("vector3: rotating about an axis and back is the identity on all three axes",
          "[vector3]") {
  const double a = 0.7;   // nothing special about it, and deliberately not
                          // a multiple of pi/2, so no component stays put

  Vector3Df base(1, 2, 3);

  {
    Vector3Df v(base);
    v.rotateX(a);
    REQUIRE_FALSE(nearlyEqual(v, base));   // the rotation did something
    v.rotateX(-a);
    REQUIRE(nearlyEqual(v, base));
  }
  {
    Vector3Df v(base);
    v.rotateY(a);
    REQUIRE_FALSE(nearlyEqual(v, base));
    v.rotateY(-a);
    REQUIRE(nearlyEqual(v, base));
  }
  {
    Vector3Df v(base);
    v.rotateZ(a);
    REQUIRE_FALSE(nearlyEqual(v, base));
    v.rotateZ(-a);
    REQUIRE(nearlyEqual(v, base));
  }
}

TEST_CASE("vector3: rotation preserves length and fixes its own axis", "[vector3]") {
  Vector3Df v(1, 2, 3);
  const double len = v.module();

  Vector3Df rx(v); rx.rotateX(0.7);
  Vector3Df ry(v); ry.rotateY(0.7);
  Vector3Df rz(v); rz.rotateZ(0.7);

  REQUIRE(rx.module() == approx(len));
  REQUIRE(ry.module() == approx(len));
  REQUIRE(rz.module() == approx(len));

  /* each rotation leaves its own axis component alone -- which is what
     distinguishes rotateX from rotateY in a way that a length check
     cannot */
  REQUIRE(rx.x() == approx(v.x()));
  REQUIRE(ry.y() == approx(v.y()));
  REQUIRE(rz.z() == approx(v.z()));

  /* a vector lying along the axis is fixed entirely */
  Vector3Df alongX(5, 0, 0);
  alongX.rotateX(0.7);
  REQUIRE(nearlyEqual(alongX, Vector3Df(5, 0, 0)));
}

TEST_CASE("vector3: four quarter turns about an axis return the original vector", "[vector3]") {
  Vector3Df v(1, 2, 3);
  Vector3Df turned(v);

  for (int i = 0; i < 4; i++)
    turned.rotateZ(M_PI / 2);

  REQUIRE(nearlyEqual(turned, v));

  /* one quarter turn about z maps (x, y) to (-y, x) or (y, -x); pin which,
     so the rotation's sense is fixed and not merely its order */
  Vector3Df quarter(1, 0, 0);
  quarter.rotateZ(M_PI / 2);
  REQUIRE(nearlyEqual(quarter, Vector3Df(0, 1, 0)));
}

/* ------------------------------------------------------------------ */
/* spherical coordinates                                               */
/* ------------------------------------------------------------------ */

TEST_CASE("vector3: setSpheric and getSpheric are inverse", "[vector3]") {
  /* phi is measured from +z and lies in [0, pi]; theta is the atan2 of y
     over x and lies in [-pi, pi]. Both ranges are asserted by the
     three-argument getSpheric itself. Choosing values strictly inside both
     ranges keeps this a roundtrip rather than a test of how the
     implementation wraps at the boundaries. */
  const double distance = 7.0;
  const double theta = 0.9;
  const double phi = 1.2;

  Vector3Df v;
  v.setSpheric(distance, theta, phi);

  double d2 = 0, t2 = 0, p2 = 0;
  v.getSpheric(d2, t2, p2);

  REQUIRE(d2 == approx(distance));
  REQUIRE(t2 == approx(theta));
  REQUIRE(p2 == approx(phi));

  /* and the distance really is the module, not a separately tracked
     number */
  REQUIRE(v.module() == approx(distance));
}

TEST_CASE("vector3: the two-argument getSpheric reports the same angles as the three-argument one",
          "[vector3]") {
  Vector3Df v(1, 2, 3);

  double theta = 0, phi = 0;
  v.getSpheric(theta, phi);

  double d2 = 0, t2 = 0, p2 = 0;
  v.getSpheric(d2, t2, p2);

  /* two separate function bodies that must not drift apart */
  REQUIRE(theta == approx(t2));
  REQUIRE(phi == approx(p2));
}

TEST_CASE("vector3: spherical coordinates place the axes where the convention says", "[vector3]") {
  /* Concrete anchors for the convention, so the roundtrip case above
     cannot be satisfied by a self-consistent but wrong pair. phi is
     measured from +z: straight up is 0, the xy-plane is pi/2. */
  Vector3Df up(0, 0, 1);
  double theta = 0, phi = 0;
  up.getSpheric(theta, phi);
  REQUIRE(phi == approx(0.0));

  Vector3Df alongX(1, 0, 0);
  alongX.getSpheric(theta, phi);
  REQUIRE(phi == approx(M_PI / 2));
  REQUIRE(theta == approx(0.0));

  Vector3Df alongY(0, 1, 0);
  alongY.getSpheric(theta, phi);
  REQUIRE(phi == approx(M_PI / 2));
  REQUIRE(theta == approx(M_PI / 2));
}

/* ------------------------------------------------------------------ */
/* projection                                                          */
/* ------------------------------------------------------------------ */

TEST_CASE("vector3: the axis projections zero exactly their own component", "[vector3]") {
  Vector3Df v(1, 2, 3);

  REQUIRE(v.projectX() == Vector3Df(0, 2, 3));
  REQUIRE(v.projectY() == Vector3Df(1, 0, 3));
  REQUIRE(v.projectZ() == Vector3Df(1, 2, 0));
}

TEST_CASE("vector3: projecting onto a plane through the origin lands in that plane", "[vector3]") {
  Vector3Df n(1, 1, 1);
  n.normalize();                 // project() documents n as a unit vector

  Vector3Df v(3, -1, 5);
  Vector3Df p = v.project(n);

  /* in the plane: the projection has no component along the normal */
  REQUIRE(p * n == approx(0.0));

  /* and what was removed is parallel to the normal, which together with
     the above is the definition of an orthogonal projection */
  Vector3Df removed = v - p;
  REQUIRE(nearlyEqual(removed ^ n, Vector3Df(0, 0, 0)));

  /* a vector already in the plane is unchanged -- the idempotence that a
     projection scaling by the wrong factor would break */
  REQUIRE(nearlyEqual(p.project(n), p));
}

TEST_CASE("vector3: distanceToPlane is signed -- and its sign is the opposite of the usual "
          "convention", "[vector3]") {
  Vector3Df origin(0, 0, 0);
  Vector3Df up(0, 0, 1);

  /* The implementation is `return (P - *this) * N;` -- it measures from
     the point TOWARDS the plane's reference point, not from the plane
     towards the point. So a point on the positive side of the normal gets
     a NEGATIVE distance, which is the reverse of what "signed distance to
     a plane" normally means.

     Pinned as it is, not as it ought to be. Anyone reaching for this
     function needs to know which way round it goes, and a case asserting
     the conventional sign would just be red. Whether to flip it is a
     separate question: doing so silently would invert the behaviour of any
     caller that has already compensated. */
  REQUIRE(Vector3Df(5, -2, 3).distanceToPlane(origin, up) == approx(-3.0));
  REQUIRE(Vector3Df(5, -2, -3).distanceToPlane(origin, up) == approx(3.0));

  /* the magnitude is right, whatever the sign: three units from the plane */
  REQUIRE(std::fabs(Vector3Df(5, -2, 3).distanceToPlane(origin, up)) == approx(3.0));

  /* a point in the plane is at distance zero however far from the centre,
     and zero has no sign to get wrong */
  REQUIRE(Vector3Df(100, -100, 0).distanceToPlane(origin, up) == approx(0.0));

  /* and the plane's reference point need not be the origin */
  Vector3Df raised(0, 0, 10);
  REQUIRE(Vector3Df(0, 0, 12).distanceToPlane(raised, up) == approx(-2.0));
}

TEST_CASE("vector3: closestPointInLine lands on the line and meets it at a right angle",
          "[vector3]") {
  Vector3Df P(0, 0, 0);          // a point on the line
  Vector3Df V(1, 0, 0);          // its direction

  Vector3Df q(3, 4, 0);
  Vector3Df c = q.closestPointInLine(P, V);

  /* on the line: the offset from P is parallel to V */
  Vector3Df offset = c - P;
  REQUIRE(nearlyEqual(offset ^ V, Vector3Df(0, 0, 0)));

  /* and the connecting segment is perpendicular to it, which is what
     "closest" means and what a projection onto the wrong axis would fail */
  REQUIRE((q - c) * V == approx(0.0));

  /* for this geometry the answer is plainly (3, 0, 0) -- stated so the two
     properties above cannot both hold for a degenerate c == q */
  REQUIRE(nearlyEqual(c, Vector3Df(3, 0, 0)));
}

TEST_CASE("vector3: perpendicular returns a vector at right angles to its source", "[vector3]") {
  /* The direction is unspecified -- any perpendicular will do -- so the
     assertion is the property, not a value. Checked on several sources
     including ones aligned with an axis, which is where an implementation
     that always crosses with a fixed vector degenerates to zero. */
  const Vector3Df sources[] = {
    Vector3Df(1, 2, 3),
    Vector3Df(1, 0, 0),
    Vector3Df(0, 1, 0),
    Vector3Df(0, 0, 1),
    Vector3Df(-5, 0, 0),
  };

  for (const Vector3Df & v : sources) {
    INFO("source (" << v.x() << ", " << v.y() << ", " << v.z() << ")");

    Vector3Df p = v.perpendicular();

    REQUIRE(p * v == approx(0.0));

    /* and it is not the zero vector, which would satisfy the dot product
       above for free */
    REQUIRE(p.squaredModule() > 0.0);
  }
}

TEST_CASE("vector3: transposing a three-vector matrix twice restores it", "[vector3]") {
  Vector3Df mat[3] = {
    Vector3Df(1, 2, 3),
    Vector3Df(4, 5, 6),
    Vector3Df(7, 8, 9),
  };

  Vector3Df original[3] = { mat[0], mat[1], mat[2] };

  Vector3Df::transpose(mat);

  /* the rows became the columns */
  REQUIRE(mat[0] == Vector3Df(1, 4, 7));
  REQUIRE(mat[1] == Vector3Df(2, 5, 8));
  REQUIRE(mat[2] == Vector3Df(3, 6, 9));

  Vector3Df::transpose(mat);

  REQUIRE(mat[0] == original[0]);
  REQUIRE(mat[1] == original[1]);
  REQUIRE(mat[2] == original[2]);
}

TEST_CASE("vector3: multiplying by a matrix applies it to the vector", "[vector3]") {
  /* This case used to pin a defect. vector3.cpp's
     operator*(const Vector3D<T>* mat) read `V[i] += mat[i][j];` -- it never
     multiplied by anything, so it returned the matrix's row sums and
     discarded the vector. Every vector times a given matrix came back the
     same. Nothing in the tree called it, nor its companion transpose(),
     which is how it survived.

     It now computes the row-major product, and the assertions below are the
     properties that fix must satisfy rather than a recorded output. */
  Vector3Df identity[3] = {
    Vector3Df(1, 0, 0),
    Vector3Df(0, 1, 0),
    Vector3Df(0, 0, 1),
  };

  /* the identity leaves a vector alone -- and does so for DIFFERENT
     vectors, which is exactly what the old version could not do */
  REQUIRE(nearlyEqual(Vector3Df(1, 2, 3) * identity, Vector3Df(1, 2, 3)));
  REQUIRE(nearlyEqual(Vector3Df(9, -4, 0.5f) * identity, Vector3Df(9, -4, 0.5f)));

  Vector3Df doubling[3] = {
    Vector3Df(2, 0, 0),
    Vector3Df(0, 2, 0),
    Vector3Df(0, 0, 2),
  };

  REQUIRE(nearlyEqual(Vector3Df(1, 2, 3) * doubling, Vector3Df(1, 2, 3) * 2.0f));

  /* A matrix that permutes, so the row/column convention is pinned rather
     than left to a symmetric example that cannot tell them apart. With
     mat[i] read as row i, row 0 here is (0,1,0), so the first component of
     the result is the vector's SECOND component. */
  Vector3Df swapXY[3] = {
    Vector3Df(0, 1, 0),
    Vector3Df(1, 0, 0),
    Vector3Df(0, 0, 1),
  };

  REQUIRE(nearlyEqual(Vector3Df(1, 2, 3) * swapXY, Vector3Df(2, 1, 3)));

  /* a fully asymmetric matrix: every entry contributes, so a transposed
     implementation gives a different answer and is caught. Rows dotted
     with (1, 0, 2): 1+6=7, 4+12=16, 7+18=25. */
  Vector3Df m[3] = {
    Vector3Df(1, 2, 3),
    Vector3Df(4, 5, 6),
    Vector3Df(7, 8, 9),
  };

  REQUIRE(nearlyEqual(Vector3Df(1, 0, 2) * m, Vector3Df(7, 16, 25)));
}

TEST_CASE("vector3: multiplying by the transpose gives the transposed product", "[vector3]") {
  /* transpose() was already correct and is covered above. This ties the two
     together: multiplying by the transpose must equal the product taken
     down columns, which only holds if the multiply reads the matrix the
     same way round that transpose() rearranges it. */
  Vector3Df m[3] = {
    Vector3Df(1, 2, 3),
    Vector3Df(4, 5, 6),
    Vector3Df(7, 8, 9),
  };

  Vector3Df v(1, 0, 2);

  /* columns dotted with v, by hand: 1+14=15, 2+16=18, 3+18=21 */
  Vector3Df::transpose(m);
  REQUIRE(nearlyEqual(v * m, Vector3Df(15, 18, 21)));

  /* and transposing back restores the row-wise product */
  Vector3Df::transpose(m);
  REQUIRE(nearlyEqual(v * m, Vector3Df(7, 16, 25)));
}

TEST_CASE("vector3: sign reports which side of a vector another one points to", "[vector3]") {
  Vector3Df v(1, 0, 0);

  REQUIRE(v.sign(Vector3Df(5, 0, 0)) == 1);
  REQUIRE(v.sign(Vector3Df(-5, 0, 0)) == -1);

  /* the boundary: a perpendicular vector has a zero dot product, and the
     implementation's >= puts that on the positive side */
  REQUIRE(v.sign(Vector3Df(0, 1, 0)) == 1);
}
