#pragma once

#include <memory>
#include <string>

class puzzle_c;
class problem_c;
class SolutionIterator;
class PyShape;

class PyProblem {
public:
  PyProblem(std::shared_ptr<puzzle_c> puz, unsigned int idx);

  std::string get_name() const;
  void set_name(const std::string & name);

  unsigned int get_num_pieces() const;
  unsigned int get_index() const { return problem_idx; }

  void set_result(unsigned int shape_idx);
  void set_result_shape(const PyShape & shape);
  int get_result() const;

  void set_piece_count(unsigned int shape_idx, unsigned int count);
  void set_piece_count_shape(const PyShape & shape, unsigned int count);
  void set_piece_range(unsigned int shape_idx, unsigned int min_count, unsigned int max_count);
  void set_piece_range_shape(const PyShape & shape, unsigned int min_count, unsigned int max_count);
  unsigned int get_piece_min(unsigned int shape_idx) const;
  unsigned int get_piece_max(unsigned int shape_idx) const;

  std::unique_ptr<SolutionIterator> solve(bool disassemble = true,
                                          bool reduce = false,
                                          bool keep_rotations = false,
                                          bool keep_mirror = false);

private:
  std::shared_ptr<puzzle_c> puzzle;
  unsigned int problem_idx;
  problem_c * get_problem() const;
};
