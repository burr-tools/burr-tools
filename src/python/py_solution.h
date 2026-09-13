#pragma once

#include <vector>
#include <string>

struct PyPlacement {
  unsigned int piece_id{0};
  int x{0};
  int y{0};
  int z{0};
  unsigned char transformation{0};
  bool is_placed{false};
};

struct PySolution {
  unsigned int assembly_number{0};
  unsigned int solution_number{0};
  std::vector<PyPlacement> placements;
  bool has_disassembly{false};
  std::string moves_text;
  unsigned int total_moves{0};
  unsigned int level{0};
};
