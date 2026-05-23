/*
  a tablebase prober for chesslib
  Copyright (C) 2025-2026  winapiadmin

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#define DOCTEST_CONFIG_IMPLEMENT
#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#include "tbprobe.h"
#include <doctest/doctest.h>
#include <position.h>
TEST_CASE("TB "
          "probing") {
  const char *tb_path_env = std::getenv("TBPROBE_TESTS_GAVIOTA_PATH");
  const std::string tb_path = tb_path_env ? tb_path_env : "tb";
  tbprobe::gaviota::PythonTablebase tb =
      tbprobe::gaviota::open_tablebase(tb_path);
  const char *csv_path_env = std::getenv("TBPROBE_TESTS_CSV");
  const std::string csv_path = csv_path_env ? csv_path_env : "tests.csv";
  std::ifstream f(csv_path);
  REQUIRE(f.is_open());
  std::string line;
  std::getline(f, line); // header

  while (std::getline(f, line)) {
    std::stringstream ss(line);

    std::string fen, wdl_s, dtz_s, wdl_g, dtm_g;

    std::getline(ss, fen, ',');
    std::getline(ss, wdl_s, ',');
    std::getline(ss, dtz_s, ',');
    std::getline(ss, dtm_g, ',');
    std::getline(ss, wdl_g, ',');

    chess::Board board(fen);
    int expected_wdl = std::stoi(wdl_g);
    int expected_dtm = std::stoi(dtm_g);
    int actual_wdl, actual_dtm;
    // try {
    actual_wdl = tb.probe_wdl(board);
    actual_dtm = tb.probe_dtm(board);
    //} catch (const std::runtime_error &e) {
    //  FAIL("Probing failed for FEN \"" << fen << "\": " << e.what());
    //  return;
    //}
    REQUIRE(expected_wdl == actual_wdl);
    REQUIRE(expected_dtm == actual_dtm);
  }
}
int main(int argc, char **argv) {
  doctest::Context ctx;
  ctx.setOption("success", true);
  ctx.setOption("no-breaks", true);
  ctx.setOption("abort-after", 1);
  return ctx.run();
}
