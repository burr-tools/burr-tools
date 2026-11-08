/* Tests for btui::placeWindow: where the main window opens. */
#include <catch2/catch_test_macros.hpp>

#include "../src/uicore/windowplacement.h"

using namespace btui;

namespace {
  const WindowRect screen1{ 0, 0, 1920, 1080 };
  const WindowRect screen2{ 1920, 0, 2560, 1440 };
  // a taskbar docked at the top takes 48 px
  const WindowRect available{ 0, 48, 1920, 1032 };
}

TEST_CASE("the first start centres the window on the free area", "[ui][windowplacement]") {
  const WindowPlacement p = placeWindow(std::nullopt, false, { screen1 }, available, 960, 640);
  CHECK_FALSE(p.restored);
  CHECK_FALSE(p.maximized);
  CHECK(p.rect.width == 1600);                       // capped
  CHECK(p.rect.height == 949);                       // 92 % of 1032
  CHECK(p.rect.x == (1920 - 1600) / 2);
  CHECK(p.rect.y == 48 + (1032 - 949) / 2);          // below the taskbar
  CHECK(p.rect.y >= available.y);
}

TEST_CASE("a small screen still gets the minimum size", "[ui][windowplacement]") {
  const WindowRect small{ 0, 0, 800, 600 };
  const WindowPlacement p = placeWindow(std::nullopt, false, { small }, small, 960, 640);
  CHECK(p.rect.width == 960);
  CHECK(p.rect.height == 640);
}

TEST_CASE("the saved place comes back while it is on a screen", "[ui][windowplacement]") {
  const WindowRect saved{ 2100, 100, 1200, 800 };    // on the second monitor
  const WindowPlacement p = placeWindow(saved, false, { screen1, screen2 }, available, 960, 640);
  CHECK(p.restored);
  CHECK(p.rect.x == 2100);
  CHECK(p.rect.y == 100);
  CHECK(p.rect.width == 1200);
  CHECK(p.rect.height == 800);
}

TEST_CASE("a place on an unplugged monitor falls back to the centre", "[ui][windowplacement]") {
  const WindowRect saved{ 2100, 100, 1200, 800 };
  const WindowPlacement p = placeWindow(saved, false, { screen1 }, available, 960, 640);
  CHECK_FALSE(p.restored);
  CHECK(p.rect.x == (1920 - 1600) / 2);
}

TEST_CASE("maximised is kept, and a saved size never undercuts the minimum", "[ui][windowplacement]") {
  const WindowRect saved{ 10, 60, 400, 300 };
  const WindowPlacement p = placeWindow(saved, true, { screen1 }, available, 960, 640);
  CHECK(p.restored);
  CHECK(p.maximized);
  CHECK(p.rect.width == 960);
  CHECK(p.rect.height == 640);
  // an empty save is no place
  CHECK_FALSE(placeWindow(WindowRect{ 0, 0, 0, 0 }, false, { screen1 }, available, 960, 640).restored);
}
