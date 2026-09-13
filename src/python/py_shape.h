#pragma once

#include <memory>
#include <string>
#include <tuple>
#include <vector>

class puzzle_c;
class voxel_c;

class PyShape {
public:
  PyShape(std::shared_ptr<puzzle_c> puz, unsigned int idx);

  unsigned int get_index() const { return shape_idx; }

  std::string get_name() const;
  void set_name(const std::string & name);

  unsigned int get_size_x() const;
  unsigned int get_size_y() const;
  unsigned int get_size_z() const;
  std::tuple<unsigned int, unsigned int, unsigned int> get_dimensions() const;

  int get(unsigned int x, unsigned int y, unsigned int z) const;
  void set(unsigned int x, unsigned int y, unsigned int z, int val);

  int get_state(unsigned int x, unsigned int y, unsigned int z) const;
  void set_state(unsigned int x, unsigned int y, unsigned int z, int state);

  unsigned int get_color(unsigned int x, unsigned int y, unsigned int z) const;
  void set_color(unsigned int x, unsigned int y, unsigned int z, unsigned int color);

  bool is_filled(unsigned int x, unsigned int y, unsigned int z) const;
  bool is_empty(unsigned int x, unsigned int y, unsigned int z) const;
  bool is_variable(unsigned int x, unsigned int y, unsigned int z) const;

  void fill(const std::vector<std::tuple<unsigned int, unsigned int, unsigned int>> & coords);
  unsigned int count_filled() const;
  unsigned int count_empty() const;

private:
  std::shared_ptr<puzzle_c> puzzle;
  unsigned int shape_idx;
  voxel_c * get_voxel() const;
};
