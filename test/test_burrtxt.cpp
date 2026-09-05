#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#else
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;

namespace {

struct CommandResult {
  int exitCode{0};
  std::string output;
};

int extractExitCode(int status) {
#ifdef _WIN32
  return status;
#else
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return status;
#endif
}

std::string getBurrTxtBinary() {
  const char * env = std::getenv("BURRTXT_BIN");
  if (env && fs::exists(env)) {
    return env;
  }
  for (const auto & candidate : {"./build/burrTxt", "build/burrTxt", "./burrTxt"}) {
    if (fs::exists(candidate)) {
      return candidate;
    }
#ifdef _WIN32
    std::string candidateExe = std::string(candidate) + ".exe";
    if (fs::exists(candidateExe)) {
      return candidateExe;
    }
#endif
  }
  return "./build/burrTxt";
}

CommandResult runBurrTxt(const std::string & args) {
  std::string binary = getBurrTxtBinary();
  std::string command = binary + " " + args + " 2>&1";

  std::array<char, 256> buffer;
  std::string output;

  FILE * pipe = popen(command.c_str(), "r");
  if (!pipe) {
    return {-1, "Failed to execute: " + command};
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    output += buffer.data();
  }
  int status = pclose(pipe);
  return {extractExitCode(status), output};
}

} // namespace

TEST_CASE("burrTxt --json produces valid JSON statistics", "[cli][burrtxt][json]") {
  SECTION("Cube in Cage puzzle JSON output") {
    CommandResult res = runBurrTxt("--json examples/CubeInCage.xmpuzzle");

    CHECK(res.exitCode == 0);
    CHECK(res.output.find("\"assemblies\":96") != std::string::npos);
    CHECK(res.output.find("\"solutions\":1") != std::string::npos);
    CHECK(res.output.find("\"dotlevel\":\"4.9.3\"") != std::string::npos);
    CHECK(res.output.find("\"level\":4") != std::string::npos);
    CHECK(res.output.find("\"totalmoves\":16") != std::string::npos);
    CHECK(!res.output.empty());
    CHECK(res.output.front() == '{');
  }

  SECTION("Pelikan Burr puzzle JSON output") {
    CommandResult res = runBurrTxt("--json examples/PelikanBurr.xmpuzzle");

    CHECK(res.exitCode == 0);
    CHECK(res.output.find("\"assemblies\":12") != std::string::npos);
    CHECK(res.output.find("\"solutions\":1") != std::string::npos);
    CHECK(res.output.find("\"dotlevel\":\"98.2.4.2\"") != std::string::npos);
    CHECK(res.output.find("\"level\":98") != std::string::npos);
  }
}

TEST_CASE("burrTxt clustered flags execution", "[cli][burrtxt][flags]") {
  SECTION("Clustered -dq runs disassemble and quiet") {
    CommandResult res = runBurrTxt("-dq examples/CubeInCage.xmpuzzle");

    CHECK(res.exitCode == 0);
    CHECK(res.output.find("96 assemblies and 1 solutions found with") != std::string::npos);
    // Quiet mode should suppress verbose shape rendering
    CHECK(res.output.find("The puzzle:") == std::string::npos);
  }

  SECTION("Clustered -drq runs disassemble, reduce, and quiet") {
    CommandResult res = runBurrTxt("-drq examples/CubeInCage.xmpuzzle");

    CHECK(res.exitCode == 0);
    CHECK(res.output.find("96 assemblies and 1 solutions found with") != std::string::npos);
  }
}

TEST_CASE("burrTxt argument validation and error rejection", "[cli][burrtxt][validation]") {
  SECTION("--json cannot be combined with conflicting flags") {
    CommandResult resSolutions = runBurrTxt("--json -s examples/CubeInCage.xmpuzzle");
    CHECK(resSolutions.exitCode == 2);
    CHECK(resSolutions.output.find("cannot be combined with") != std::string::npos);

    CommandResult resDisasm = runBurrTxt("--json -p examples/CubeInCage.xmpuzzle");
    CHECK(resDisasm.exitCode == 2);
    CHECK(resDisasm.output.find("cannot be combined with") != std::string::npos);

    CommandResult resAll = runBurrTxt("--json -o all examples/CubeInCage.xmpuzzle");
    CHECK(resAll.exitCode == 2);
    CHECK(resAll.output.find("cannot be combined with") != std::string::npos);
  }

  SECTION("Clustered flags requiring separate arguments are rejected") {
    CommandResult res = runBurrTxt("-do examples/CubeInCage.xmpuzzle");
    CHECK(res.exitCode == 2);
    CHECK(res.output.find("-o cannot be clustered") != std::string::npos);
  }

  SECTION("Unknown options are rejected with usage notice") {
    CommandResult res = runBurrTxt("-dz examples/CubeInCage.xmpuzzle");
    CHECK(res.exitCode == 2);
    CHECK(res.output.find("unknown option -z") != std::string::npos);
    CHECK(res.output.find("burrTxt [options] file [options]") != std::string::npos);
  }

  SECTION("Missing file argument exits with error") {
    CommandResult res = runBurrTxt("--json");
    CHECK(res.exitCode == 1);
  }
}
