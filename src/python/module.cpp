#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "py_solution.h"
#include "py_solution_iterator.h"
#include "py_shape.h"
#include "py_problem.h"
#include "py_puzzle.h"
#include "lib/voxel.h"

namespace py = pybind11;

PYBIND11_MODULE(burrtools, m) {
  m.doc() = "Python bindings for the BurrTools 3D interlocking puzzle library";

  // Voxel state enum
  py::enum_<voxel_c::VoxelState>(m, "Voxel", "Voxel state values")
    .value("EMPTY", voxel_c::VX_EMPTY)
    .value("FILLED", voxel_c::VX_FILLED)
    .value("VARIABLE", voxel_c::VX_VARIABLE)
    .export_values();

  // Placement
  py::class_<PyPlacement>(m, "Placement", "A piece placement within an assembly")
    .def_readonly("piece_id", &PyPlacement::piece_id, "Index of the piece")
    .def_readonly("x", &PyPlacement::x, "X coordinate")
    .def_readonly("y", &PyPlacement::y, "Y coordinate")
    .def_readonly("z", &PyPlacement::z, "Z coordinate")
    .def_readonly("transformation", &PyPlacement::transformation, "Orientation/transformation index")
    .def_readonly("is_placed", &PyPlacement::is_placed, "Whether this piece is placed in the assembly")
    .def("__repr__", [](const PyPlacement & p) {
      return "<Placement piece=" + std::to_string(p.piece_id) +
             " pos=(" + std::to_string(p.x) + "," + std::to_string(p.y) + "," + std::to_string(p.z) + ")" +
             " trans=" + std::to_string(p.transformation) +
             (p.is_placed ? " placed>" : " unplaced>");
    });

  // Solution
  py::class_<PySolution>(m, "Solution", "A puzzle solution found by the solver")
    .def_readonly("assembly_number", &PySolution::assembly_number, "Sequence number of the assembly")
    .def_readonly("solution_number", &PySolution::solution_number, "Sequence number among valid solutions")
    .def_readonly("has_disassembly", &PySolution::has_disassembly, "Whether disassembly analysis was performed")
    .def_readonly("moves_text", &PySolution::moves_text, "Disassembly sequence string (e.g. '14.2.1')")
    .def_readonly("total_moves", &PySolution::total_moves, "Total number of moves to fully disassemble")
    .def_readonly("level", &PySolution::level, "Number of moves to remove the first piece")
    .def_readonly("placements", &PySolution::placements, "List of piece placements in the assembly")
    .def("__repr__", [](const PySolution & s) {
      std::string res = "<Solution #" + std::to_string(s.solution_number);
      if (s.has_disassembly) {
        res += " level=" + std::to_string(s.level) +
               " moves='" + s.moves_text + "'";
      }
      res += " pieces=" + std::to_string(s.placements.size()) + ">";
      return res;
    });

  // SolutionIterator
  py::class_<SolutionIterator>(m, "SolutionIterator", "Streaming iterator yielding solutions as they are found")
    .def("__iter__", &SolutionIterator::iter)
    .def("__next__", &SolutionIterator::next)
    .def("stop", &SolutionIterator::stop, "Stop the background solving process immediately")
    .def_property_readonly("is_finished", &SolutionIterator::is_finished, "Whether the solver has finished searching")
    .def_property_readonly("iterations", &SolutionIterator::get_iterations, "Number of internal solver iterations")
    .def("__repr__", [](const SolutionIterator & it) {
      return "<SolutionIterator finished=" + std::string(it.is_finished() ? "True" : "False") +
             " iterations=" + std::to_string(it.get_iterations()) + ">";
    });

  // Shape
  py::class_<PyShape>(m, "Shape", "A 3D voxel grid representing a puzzle shape or piece")
    .def_property("name", &PyShape::get_name, &PyShape::set_name, "Name or label of the shape")
    .def_property_readonly("index", &PyShape::get_index, "Index of the shape in the puzzle")
    .def_property_readonly("size_x", &PyShape::get_size_x, "Width along X axis")
    .def_property_readonly("size_y", &PyShape::get_size_y, "Height along Y axis")
    .def_property_readonly("size_z", &PyShape::get_size_z, "Depth along Z axis")
    .def_property_readonly("dimensions", &PyShape::get_dimensions, "Dimensions as (x, y, z) tuple")
    .def("get", &PyShape::get, py::arg("x"), py::arg("y"), py::arg("z"), "Get raw voxel value at (x, y, z)")
    .def("set", [](PyShape & s, unsigned int x, unsigned int y, unsigned int z, int val) {
      s.set(x, y, z, val);
    }, py::arg("x"), py::arg("y"), py::arg("z"), py::arg("val"), "Set raw voxel value at (x, y, z)")
    .def("get_state", &PyShape::get_state, py::arg("x"), py::arg("y"), py::arg("z"), "Get voxel state (EMPTY, FILLED, VARIABLE)")
    .def("set_state", &PyShape::set_state, py::arg("x"), py::arg("y"), py::arg("z"), py::arg("state"), "Set voxel state (EMPTY, FILLED, VARIABLE)")
    .def("get_color", &PyShape::get_color, py::arg("x"), py::arg("y"), py::arg("z"), "Get voxel constraint color index")
    .def("set_color", &PyShape::set_color, py::arg("x"), py::arg("y"), py::arg("z"), py::arg("color"), "Set voxel constraint color index")
    .def("is_filled", &PyShape::is_filled, py::arg("x"), py::arg("y"), py::arg("z"), "Check if voxel is filled")
    .def("is_empty", &PyShape::is_empty, py::arg("x"), py::arg("y"), py::arg("z"), "Check if voxel is empty")
    .def("is_variable", &PyShape::is_variable, py::arg("x"), py::arg("y"), py::arg("z"), "Check if voxel is variable")
    .def("fill", &PyShape::fill, py::arg("coords"), "Fill voxels at all given (x, y, z) coordinate tuples")
    .def("count_filled", &PyShape::count_filled, "Number of filled voxels in this shape")
    .def("count_empty", &PyShape::count_empty, "Number of empty voxels in this shape")
    .def("__repr__", [](const PyShape & s) {
      return "<Shape #" + std::to_string(s.get_index()) +
             " '" + s.get_name() +
             "' size=(" + std::to_string(s.get_size_x()) + "," +
             std::to_string(s.get_size_y()) + "," +
             std::to_string(s.get_size_z()) + ")" +
             " filled=" + std::to_string(s.count_filled()) + ">";
    });

  // Problem
  py::class_<PyProblem>(m, "Problem", "A puzzle problem defining target shape and piece constraints")
    .def_property("name", &PyProblem::get_name, &PyProblem::set_name, "Name of the problem")
    .def_property_readonly("num_pieces", &PyProblem::get_num_pieces, "Number of pieces in this problem")
    .def_property_readonly("index", &PyProblem::get_index, "Zero-based problem index in the puzzle")
    .def_property_readonly("result_shape_index", &PyProblem::get_result, "Shape index of the target result shape, or -1 if unset")
    .def("set_result", py::overload_cast<unsigned int>(&PyProblem::set_result), py::arg("shape_index"), "Set the target result shape by index")
    .def("set_result", py::overload_cast<const PyShape &>(&PyProblem::set_result_shape), py::arg("shape"), "Set the target result shape")
    .def("set_piece_count", py::overload_cast<unsigned int, unsigned int>(&PyProblem::set_piece_count), py::arg("shape_index"), py::arg("count"), "Set fixed piece count for a shape index")
    .def("set_piece_count", py::overload_cast<const PyShape &, unsigned int>(&PyProblem::set_piece_count_shape), py::arg("shape"), py::arg("count"), "Set fixed piece count for a shape")
    .def("set_piece_range", py::overload_cast<unsigned int, unsigned int, unsigned int>(&PyProblem::set_piece_range), py::arg("shape_index"), py::arg("min_count"), py::arg("max_count"), "Set min and max piece count for a shape index")
    .def("set_piece_range", py::overload_cast<const PyShape &, unsigned int, unsigned int>(&PyProblem::set_piece_range_shape), py::arg("shape"), py::arg("min_count"), py::arg("max_count"), "Set min and max piece count for a shape")
    .def("get_piece_min", &PyProblem::get_piece_min, py::arg("shape_index"), "Get minimum piece count for a shape index")
    .def("get_piece_max", &PyProblem::get_piece_max, py::arg("shape_index"), "Get maximum piece count for a shape index")
    .def("solve", &PyProblem::solve,
         py::arg("disassemble") = true,
         py::arg("reduce") = false,
         py::arg("keep_rotations") = false,
         py::arg("keep_mirror") = false,
         "Start solving the problem and return an iterator yielding Solution objects")
    .def("__repr__", [](const PyProblem & p) {
      return "<Problem index=" + std::to_string(p.get_index()) +
             " name='" + p.get_name() +
             "' pieces=" + std::to_string(p.get_num_pieces()) + ">";
    });

  // Puzzle
  py::class_<PyPuzzle, std::shared_ptr<PyPuzzle>>(m, "Puzzle", "A BurrTools puzzle definition containing shapes and problems")
    .def(py::init<>(), "Create a new empty puzzle with standard 3D cubic/bricks grid")
    .def_static("load", &PyPuzzle::load, py::arg("filename"), "Load a puzzle from an .xmpuzzle file")
    .def("save", &PyPuzzle::save, py::arg("filename"), "Save the puzzle to an .xmpuzzle file")
    .def_property("comment", &PyPuzzle::get_comment, &PyPuzzle::set_comment, "Puzzle description or comments")
    .def_property_readonly("num_shapes", &PyPuzzle::get_num_shapes, "Number of shapes in the puzzle")
    .def_property_readonly("num_problems", &PyPuzzle::get_num_problems, "Number of problems in the puzzle")
    .def("add_shape", &PyPuzzle::add_shape,
         py::arg("sx"), py::arg("sy"), py::arg("sz"),
         py::arg("name") = "",
         "Add a new empty shape of size (sx, sy, sz) to the puzzle and return it")
    .def_property_readonly("shapes", &PyPuzzle::get_shapes, "List of all shapes in the puzzle")
    .def("shape", &PyPuzzle::get_shape, py::arg("index"), "Get a specific shape by index")
    .def("add_problem", &PyPuzzle::add_problem,
         py::arg("name") = "",
         "Add a new problem to the puzzle and return it")
    .def_property_readonly("problems", &PyPuzzle::get_problems, "List of all problems in the puzzle")
    .def("problem", &PyPuzzle::get_problem, py::arg("index"), "Get a specific problem by index")
    .def("__repr__", [](const PyPuzzle & p) {
      return "<Puzzle problems=" + std::to_string(p.get_num_problems()) +
             " shapes=" + std::to_string(p.get_num_shapes()) + ">";
    });

  m.def("load", &PyPuzzle::load, py::arg("filename"), "Load a puzzle from an .xmpuzzle file");
}
