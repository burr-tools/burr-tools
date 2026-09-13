#include "py_puzzle.h"
#include "py_shape.h"
#include "lib/puzzle.h"
#include "lib/problem.h"
#include "lib/gridtype.h"
#include "lib/voxel.h"

#include <stdexcept>

PyPuzzle::PyPuzzle()
  : puzzle(std::make_shared<puzzle_c>(new gridType_c(gridType_c::GT_BRICKS))) {}

PyPuzzle::PyPuzzle(std::shared_ptr<puzzle_c> puz) : puzzle(std::move(puz)) {
  if (!puzzle) {
    throw std::invalid_argument("Puzzle pointer is null");
  }
}

std::shared_ptr<PyPuzzle> PyPuzzle::load(const std::string & filename) {
  return std::make_shared<PyPuzzle>(puzzle_c::load(filename));
}

void PyPuzzle::save(const std::string & filename) const {
  puzzle->save(filename);
}

std::string PyPuzzle::get_comment() const {
  return puzzle->getComment();
}

void PyPuzzle::set_comment(const std::string & com) {
  puzzle->setComment(com);
}

unsigned int PyPuzzle::get_num_shapes() const {
  return puzzle->getNumberOfShapes();
}

unsigned int PyPuzzle::get_num_problems() const {
  return puzzle->getNumberOfProblems();
}

PyShape PyPuzzle::add_shape(unsigned int sx, unsigned int sy, unsigned int sz, const std::string & name) {
  unsigned int idx = puzzle->addShape(sx, sy, sz);
  if (!name.empty()) {
    puzzle->getShape(idx)->setName(name);
  }
  return PyShape(puzzle, idx);
}

std::vector<PyShape> PyPuzzle::get_shapes() {
  std::vector<PyShape> result;
  result.reserve(puzzle->getNumberOfShapes());
  for (unsigned int i = 0; i < puzzle->getNumberOfShapes(); i++) {
    result.emplace_back(puzzle, i);
  }
  return result;
}

PyShape PyPuzzle::get_shape(unsigned int idx) const {
  return PyShape(puzzle, idx);
}

PyProblem PyPuzzle::add_problem(const std::string & name) {
  unsigned int idx = puzzle->addProblem();
  if (!name.empty()) {
    puzzle->getProblem(idx)->setName(name);
  }
  return PyProblem(puzzle, idx);
}

std::vector<PyProblem> PyPuzzle::get_problems() {
  std::vector<PyProblem> result;
  result.reserve(puzzle->getNumberOfProblems());
  for (unsigned int i = 0; i < puzzle->getNumberOfProblems(); i++) {
    result.emplace_back(puzzle, i);
  }
  return result;
}

PyProblem PyPuzzle::get_problem(unsigned int idx) const {
  return PyProblem(puzzle, idx);
}
