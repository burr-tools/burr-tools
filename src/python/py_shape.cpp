#include "py_shape.h"
#include "lib/puzzle.h"
#include "lib/voxel.h"

#include <stdexcept>

PyShape::PyShape(std::shared_ptr<puzzle_c> puz, unsigned int idx)
  : puzzle(std::move(puz)), shape_idx(idx) {
  if (!puzzle) {
    throw std::invalid_argument("Puzzle pointer is null");
  }
  if (shape_idx >= puzzle->getNumberOfShapes()) {
    throw std::out_of_range("Shape index out of range");
  }
}

voxel_c * PyShape::get_voxel() const {
  if (shape_idx >= puzzle->getNumberOfShapes()) {
    throw std::out_of_range("Shape index out of range");
  }
  return puzzle->getShape(shape_idx);
}

std::string PyShape::get_name() const {
  return get_voxel()->getName();
}

void PyShape::set_name(const std::string & name) {
  get_voxel()->setName(name);
}

unsigned int PyShape::get_size_x() const {
  return get_voxel()->getX();
}

unsigned int PyShape::get_size_y() const {
  return get_voxel()->getY();
}

unsigned int PyShape::get_size_z() const {
  return get_voxel()->getZ();
}

std::tuple<unsigned int, unsigned int, unsigned int> PyShape::get_dimensions() const {
  voxel_c * v = get_voxel();
  return {v->getX(), v->getY(), v->getZ()};
}

int PyShape::get(unsigned int x, unsigned int y, unsigned int z) const {
  return get_voxel()->get(x, y, z);
}

void PyShape::set(unsigned int x, unsigned int y, unsigned int z, int val) {
  get_voxel()->set(x, y, z, val);
}

int PyShape::get_state(unsigned int x, unsigned int y, unsigned int z) const {
  return get_voxel()->getState(x, y, z);
}

void PyShape::set_state(unsigned int x, unsigned int y, unsigned int z, int state) {
  get_voxel()->setState(x, y, z, state);
}

unsigned int PyShape::get_color(unsigned int x, unsigned int y, unsigned int z) const {
  return get_voxel()->getColor(x, y, z);
}

void PyShape::set_color(unsigned int x, unsigned int y, unsigned int z, unsigned int color) {
  get_voxel()->setColor(x, y, z, color);
}

bool PyShape::is_filled(unsigned int x, unsigned int y, unsigned int z) const {
  return get_voxel()->isFilled(x, y, z);
}

bool PyShape::is_empty(unsigned int x, unsigned int y, unsigned int z) const {
  return get_voxel()->isEmpty(x, y, z);
}

bool PyShape::is_variable(unsigned int x, unsigned int y, unsigned int z) const {
  return get_voxel()->isVariable(x, y, z);
}

void PyShape::fill(const std::vector<std::tuple<unsigned int, unsigned int, unsigned int>> & coords) {
  voxel_c * v = get_voxel();
  v->skipRecalcBoundingBox(true);
  for (const auto & c : coords) {
    v->set(std::get<0>(c), std::get<1>(c), std::get<2>(c), voxel_c::VX_FILLED);
  }
  v->skipRecalcBoundingBox(false);
}

unsigned int PyShape::count_filled() const {
  return get_voxel()->countState(voxel_c::VX_FILLED);
}

unsigned int PyShape::count_empty() const {
  return get_voxel()->countState(voxel_c::VX_EMPTY);
}
