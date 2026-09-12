#pragma once

#include "py_problem.h"
#include "py_shape.h"

#include <memory>
#include <string>
#include <vector>

class puzzle_c;

class PyPuzzle {
public:
  PyPuzzle();
  explicit PyPuzzle(std::shared_ptr<puzzle_c> puz);

  static std::shared_ptr<PyPuzzle> load(const std::string & filename);
  void save(const std::string & filename) const;

  std::string get_comment() const;
  void set_comment(const std::string & com);

  unsigned int get_num_shapes() const;
  unsigned int get_num_problems() const;

  PyShape add_shape(unsigned int sx, unsigned int sy, unsigned int sz, const std::string & name = "");
  std::vector<PyShape> get_shapes();
  PyShape get_shape(unsigned int idx) const;

  PyProblem add_problem(const std::string & name = "");
  std::vector<PyProblem> get_problems();
  PyProblem get_problem(unsigned int idx) const;

  std::shared_ptr<puzzle_c> get_raw_puzzle() const { return puzzle; }

private:
  std::shared_ptr<puzzle_c> puzzle;
};
