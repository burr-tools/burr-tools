#include "py_problem.h"
#include "py_solution_iterator.h"
#include "py_shape.h"
#include "lib/puzzle.h"
#include "lib/problem.h"

#include <stdexcept>

PyProblem::PyProblem(std::shared_ptr<puzzle_c> puz, unsigned int idx)
  : puzzle(std::move(puz)), problem_idx(idx) {
  if (!puzzle) {
    throw std::invalid_argument("Puzzle pointer is null");
  }
  if (problem_idx >= puzzle->getNumberOfProblems()) {
    throw std::out_of_range("Problem index out of range");
  }
}

problem_c * PyProblem::get_problem() const {
  return puzzle->getProblem(problem_idx);
}

std::string PyProblem::get_name() const {
  return get_problem()->getName();
}

void PyProblem::set_name(const std::string & name) {
  get_problem()->setName(name);
}

unsigned int PyProblem::get_num_pieces() const {
  return get_problem()->getNumberOfPieces();
}

void PyProblem::set_result(unsigned int shape_idx) {
  if (shape_idx >= puzzle->getNumberOfShapes()) {
    throw std::out_of_range("Shape index out of range");
  }
  get_problem()->setResultId(shape_idx);
}

void PyProblem::set_result_shape(const PyShape & shape) {
  set_result(shape.get_index());
}

int PyProblem::get_result() const {
  if (!get_problem()->resultValid()) {
    return -1;
  }
  return static_cast<int>(get_problem()->getResultId());
}

void PyProblem::set_piece_range(unsigned int shape_idx, unsigned int min_count, unsigned int max_count) {
  if (shape_idx >= puzzle->getNumberOfShapes()) {
    throw std::out_of_range("Shape index out of range");
  }
  get_problem()->setShapeMinimum(shape_idx, min_count);
  get_problem()->setShapeMaximum(shape_idx, max_count);
}

void PyProblem::set_piece_range_shape(const PyShape & shape, unsigned int min_count, unsigned int max_count) {
  set_piece_range(shape.get_index(), min_count, max_count);
}

void PyProblem::set_piece_count(unsigned int shape_idx, unsigned int count) {
  set_piece_range(shape_idx, count, count);
}

void PyProblem::set_piece_count_shape(const PyShape & shape, unsigned int count) {
  set_piece_range(shape.get_index(), count, count);
}

unsigned int PyProblem::get_piece_min(unsigned int shape_idx) const {
  if (shape_idx >= puzzle->getNumberOfShapes()) {
    throw std::out_of_range("Shape index out of range");
  }
  return get_problem()->getShapeMinimum(shape_idx);
}

unsigned int PyProblem::get_piece_max(unsigned int shape_idx) const {
  if (shape_idx >= puzzle->getNumberOfShapes()) {
    throw std::out_of_range("Shape index out of range");
  }
  return get_problem()->getShapeMaximum(shape_idx);
}

std::unique_ptr<SolutionIterator> PyProblem::solve(bool disassemble,
                                                    bool reduce,
                                                    bool keep_rotations,
                                                    bool keep_mirror) {
  return std::make_unique<SolutionIterator>(
    puzzle,
    problem_idx,
    disassemble,
    reduce,
    keep_rotations,
    keep_mirror
  );
}
