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
// Code ported from Python (python-chess), credits to niklasf
#ifndef TBPROBE_H
#define TBPROBE_H
#include <array>
#include <condition_variable>
#include <fwd_decl.h>
#include <mutex>
#include <optional>
#include <regex>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>
namespace tbprobe::syzygy {
//clang-format off
constexpr int TBPIECES = 7;

constexpr std::array<int, 64> TRIANGLE = {
    6, 0, 1, 2, 2, 1, 0, 6, 0, 7, 3, 4, 4, 3, 7, 0, 1, 3, 8, 5, 5, 8,
    3, 1, 2, 4, 5, 9, 9, 5, 4, 2, 2, 4, 5, 9, 9, 5, 4, 2, 1, 3, 8, 5,
    5, 8, 3, 1, 0, 7, 3, 4, 4, 3, 7, 0, 6, 0, 1, 2, 2, 1, 0, 6,
};

constexpr std::array<int, 10> INVTRIANGLE = {1, 2, 3, 10, 11, 19, 0, 9, 18, 27};
constexpr std::array<int, 64> LOWER = {
    28, 0,  1,  2,  3,  4,  5,  6,  0, 29, 7,  8,  9,  10, 11, 12,
    1,  7,  30, 13, 14, 15, 16, 17, 2, 8,  13, 31, 18, 19, 20, 21,
    3,  9,  14, 18, 32, 22, 23, 24, 4, 10, 15, 19, 22, 33, 25, 26,
    5,  11, 16, 20, 23, 25, 34, 27, 6, 12, 17, 21, 24, 26, 27, 35,
};

constexpr std::array<int, 64> DIAG = {
    0, 0, 0, 0, 0, 0,  0,  8, 0, 1, 0, 0, 0,  0,  9, 0, 0, 0, 2, 0, 0,  10,
    0, 0, 0, 0, 0, 3,  11, 0, 0, 0, 0, 0, 0,  12, 4, 0, 0, 0, 0, 0, 13, 0,
    0, 5, 0, 0, 0, 14, 0,  0, 0, 0, 6, 0, 15, 0,  0, 0, 0, 0, 0, 7,
};

constexpr std::array<int, 64> FLAP = {
    0, 0,  0,  0,  0,  0,  0,  0, 0, 6,  12, 18, 18, 12, 6,  0,
    1, 7,  13, 19, 19, 13, 7,  1, 2, 8,  14, 20, 20, 14, 8,  2,
    3, 9,  15, 21, 21, 15, 9,  3, 4, 10, 16, 22, 22, 16, 10, 4,
    5, 11, 17, 23, 23, 17, 11, 5, 0, 0,  0,  0,  0,  0,  0,  0,
};

constexpr std::array<int, 64> PTWIST = {
    0,  0,  0,  0, 0, 0,  0,  0,  47, 35, 23, 11, 10, 22, 34, 46,
    45, 33, 21, 9, 8, 20, 32, 44, 43, 31, 19, 7,  6,  18, 30, 42,
    41, 29, 17, 5, 4, 16, 28, 40, 39, 27, 15, 3,  2,  14, 26, 38,
    37, 25, 13, 1, 0, 12, 24, 36, 0,  0,  0,  0,  0,  0,  0,  0,
};

constexpr std::array<int, 24> INVFLAP = {
    8,  16, 24, 32, 40, 48, 9,  17, 25, 33, 41, 49,
    10, 18, 26, 34, 42, 50, 11, 19, 27, 35, 43, 51,
};

constexpr std::array<int, 8> FILE_TO_FILE = {0, 1, 2, 3, 3, 2, 1, 0};
constexpr std::array<std::array<int, 64>, 10> KK_IDX = {
    {{{
         -1, -1, -1, 0,  1,  2,  3,  4,  -1, -1, -1, 5,  6,  7,  8,  9,
         10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
         26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
         42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
     }},
     {{
         58,  -1,  -1,  -1,  59,  60,  61,  62,  63,  -1,  -1,  -1,  64,
         65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,
         78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,
         91,  92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103,
         104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
     }},
     {{
         116, 117, -1,  -1,  -1,  118, 119, 120, 121, 122, -1,  -1,  -1,
         123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135,
         136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148,
         149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161,
         162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173,
     }},
     {{
         174, -1,  -1,  -1,  175, 176, 177, 178, 179, -1,  -1,  -1,  180,
         181, 182, 183, 184, -1,  -1,  -1,  185, 186, 187, 188, 189, 190,
         191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203,
         204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216,
         217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228,
     }},
     {{
         229, 230, -1,  -1,  -1,  231, 232, 233, 234, 235, -1,  -1,  -1,
         236, 237, 238, 239, 240, -1,  -1,  -1,  241, 242, 243, 244, 245,
         246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258,
         259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271,
         272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283,
     }},
     {{
         284, 285, 286, 287, 288, 289, 290, 291, 292, 293, -1,  -1,  -1,
         294, 295, 296, 297, 298, -1,  -1,  -1,  299, 300, 301, 302, 303,
         -1,  -1,  -1,  304, 305, 306, 307, 308, 309, 310, 311, 312, 313,
         314, 315, 316, 317, 318, 319, 320, 321, 322, 323, 324, 325, 326,
         327, 328, 329, 330, 331, 332, 333, 334, 335, 336, 337, 338,
     }},
     {{
         -1,  -1,  339, 340, 341, 342, 343, 344, -1,  -1,  345, 346, 347,
         348, 349, 350, -1,  -1,  441, 351, 352, 353, 354, 355, -1,  -1,
         -1,  442, 356, 357, 358, 359, -1,  -1,  -1,  -1,  443, 360, 361,
         362, -1,  -1,  -1,  -1,  -1,  444, 363, 364, -1,  -1,  -1,  -1,
         -1,  -1,  445, 365, -1,  -1,  -1,  -1,  -1,  -1,  -1,  446,
     }},
     {{
         -1,  -1,  -1,  366, 367, 368, 369, 370, -1,  -1,  -1,  371, 372,
         373, 374, 375, -1,  -1,  -1,  376, 377, 378, 379, 380, -1,  -1,
         -1,  447, 381, 382, 383, 384, -1,  -1,  -1,  -1,  448, 385, 386,
         387, -1,  -1,  -1,  -1,  -1,  449, 388, 389, -1,  -1,  -1,  -1,
         -1,  -1,  450, 390, -1,  -1,  -1,  -1,  -1,  -1,  -1,  451,
     }},
     {{
         452, 391, 392, 393, 394, 395, 396, 397, -1,  -1,  -1,  -1,  398,
         399, 400, 401, -1,  -1,  -1,  -1,  402, 403, 404, 405, -1,  -1,
         -1,  -1,  406, 407, 408, 409, -1,  -1,  -1,  -1,  453, 410, 411,
         412, -1,  -1,  -1,  -1,  -1,  454, 413, 414, -1,  -1,  -1,  -1,
         -1,  -1,  455, 415, -1,  -1,  -1,  -1,  -1,  -1,  -1,  456,
     }},
     {{
         457, 416, 417, 418, 419, 420, 421, 422, -1,  458, 423, 424, 425,
         426, 427, 428, -1,  -1,  -1,  -1,  -1,  429, 430, 431, -1,  -1,
         -1,  -1,  -1,  432, 433, 434, -1,  -1,  -1,  -1,  -1,  435, 436,
         437, -1,  -1,  -1,  -1,  -1,  459, 438, 439, -1,  -1,  -1,  -1,
         -1,  -1,  460, 440, -1,  -1,  -1,  -1,  -1,  -1,  -1,  461,
     }}}};

constexpr std::array<std::array<int, 64>, 10> PP_IDX = {
    {{{
         0,  -1, 1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
         15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
         31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46,
         -1, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
     }},
     {{
         62,  -1,  -1,  63, 64,  65,  -1,  66,  -1,  67,  68,  69,  70,
         71,  72,  -1,  73, 74,  75,  76,  77,  78,  79,  80,  81,  82,
         83,  84,  85,  86, 87,  88,  89,  90,  91,  92,  93,  94,  95,
         96,  -1,  97,  98, 99,  100, 101, 102, 103, -1,  104, 105, 106,
         107, 108, 109, -1, 110, -1,  111, 112, 113, 114, -1,  115,
     }},
     {{
         116, -1,  -1,  -1,  117, -1,  -1,  118, -1,  119, 120, 121, 122,
         123, 124, -1,  -1,  125, 126, 127, 128, 129, 130, -1,  131, 132,
         133, 134, 135, 136, 137, 138, -1,  139, 140, 141, 142, 143, 144,
         145, -1,  146, 147, 148, 149, 150, 151, -1,  -1,  152, 153, 154,
         155, 156, 157, -1,  158, -1,  -1,  159, 160, -1,  -1,  161,
     }},
     {{
         162, -1,  -1,  -1,  -1,  -1,  -1,  163, -1,  164, -1,  165, 166,
         167, 168, -1,  -1,  169, 170, 171, 172, 173, 174, -1,  -1,  175,
         176, 177, 178, 179, 180, -1,  -1,  181, 182, 183, 184, 185, 186,
         -1,  -1,  -1,  187, 188, 189, 190, 191, -1,  -1,  192, 193, 194,
         195, 196, 197, -1,  198, -1,  -1,  -1,  -1,  -1,  -1,  199,
     }},
     {{
         200, -1,  -1,  -1,  -1,  -1,  -1,  201, -1,  202, -1,  -1,  203,
         -1,  204, -1,  -1,  -1,  205, 206, 207, 208, -1,  -1,  -1,  209,
         210, 211, 212, 213, 214, -1,  -1,  -1,  215, 216, 217, 218, 219,
         -1,  -1,  -1,  220, 221, 222, 223, -1,  -1,  -1,  224, -1,  225,
         226, -1,  227, -1,  228, -1,  -1,  -1,  -1,  -1,  -1,  229,
     }},
     {{
         230, -1,  -1,  -1,  -1,  -1,  -1,  231, -1,  232, -1,  -1,  -1,
         -1,  233, -1,  -1,  -1,  234, -1,  235, 236, -1,  -1,  -1,  -1,
         237, 238, 239, 240, -1,  -1,  -1,  -1,  -1,  241, 242, 243, -1,
         -1,  -1,  -1,  244, 245, 246, 247, -1,  -1,  -1,  248, -1,  -1,
         -1,  -1,  249, -1,  250, -1,  -1,  -1,  -1,  -1,  -1,  251,
     }},
     {{
         -1, -1,  -1,  -1, -1, -1,  -1,  259, -1,  252, -1,  -1,  -1,
         -1, 260, -1,  -1, -1, 253, -1,  -1,  261, -1,  -1,  -1,  -1,
         -1, 254, 262, -1, -1, -1,  -1,  -1,  -1,  -1,  255, -1,  -1,
         -1, -1,  -1,  -1, -1, -1,  256, -1,  -1,  -1,  -1,  -1,  -1,
         -1, -1,  257, -1, -1, -1,  -1,  -1,  -1,  -1,  -1,  258,
     }},
     {{
         -1, -1, -1,  -1, -1,  -1,  -1,  -1, -1, -1, -1, -1,  -1,  -1,  268, -1,
         -1, -1, 263, -1, -1,  269, -1,  -1, -1, -1, -1, 264, 270, -1,  -1,  -1,
         -1, -1, -1,  -1, 265, -1,  -1,  -1, -1, -1, -1, -1,  -1,  266, -1,  -1,
         -1, -1, -1,  -1, -1,  -1,  267, -1, -1, -1, -1, -1,  -1,  -1,  -1,  -1,
     }},
     {{
         -1, -1, -1, -1, -1,  -1,  -1, -1, -1, -1, -1, -1,  -1,  -1,  -1, -1,
         -1, -1, -1, -1, -1,  274, -1, -1, -1, -1, -1, 271, 275, -1,  -1, -1,
         -1, -1, -1, -1, 272, -1,  -1, -1, -1, -1, -1, -1,  -1,  273, -1, -1,
         -1, -1, -1, -1, -1,  -1,  -1, -1, -1, -1, -1, -1,  -1,  -1,  -1, -1,
     }},
     {{
         -1, -1, -1, -1, -1,  -1, -1, -1, -1, -1, -1, -1, -1,  -1, -1, -1,
         -1, -1, -1, -1, -1,  -1, -1, -1, -1, -1, -1, -1, 277, -1, -1, -1,
         -1, -1, -1, -1, 276, -1, -1, -1, -1, -1, -1, -1, -1,  -1, -1, -1,
         -1, -1, -1, -1, -1,  -1, -1, -1, -1, -1, -1, -1, -1,  -1, -1, -1,
     }}}};
constexpr std::array<int, 64> MTWIST = {
    15, 63, 55, 47, 40, 48, 56, 12, 62, 11, 39, 31, 24, 32, 8,  57,
    54, 38, 7,  23, 16, 4,  33, 49, 46, 30, 22, 3,  0,  17, 25, 41,
    45, 29, 21, 2,  1,  18, 26, 42, 53, 37, 6,  20, 19, 5,  34, 50,
    61, 10, 36, 28, 27, 35, 9,  58, 14, 60, 52, 44, 43, 51, 59, 13,
};
extern std::array<std::array<int, 24>, 5> PAWNIDX;
extern std::array<std::array<int, 24>, 5> PFACTOR;
extern std::array<std::array<int, 10>, 5> MULTIDX;
extern std::array<int, 5> MFACTOR;
constexpr std::array<int, 5> WDL_TO_MAP = {1, 3, 0, 2, 0};
constexpr std::array<int, 5> PA_FLAGS = {8, 0, 0, 0, 4};
constexpr std::array<int, 5> WDL_TO_DTZ = {-1, -101, 0, 101, 1};
constexpr std::string_view PCHR = "KQRBNP";
extern std::regex TABLEBASE_REGEX;

//clang-format on
bool is_tablename(const std::string &name, bool one_king = true,
                  std::optional<int> piece_count = TBPIECES,
                  bool normalized = true);
std::vector<std::string>
all_dependencies(const std::vector<std::string> &targets, bool one_king = true);
std::vector<std::string> tablenames(bool one_king = true, int piece_count = 6);
std::vector<std::string> dependencies(const std::string &target,
                                      bool one_king = true);
std::vector<std::string> _dependencies(const std::string &target,
                                       bool one_king = true);
std::string normalize_tablename(const std::string &name, bool mirror = false);
std::string recalc_key(const std::vector<chess::PieceType> &pieces,
                       bool mirror = false);
std::string calc_key(const chess::Position &board, bool mirror = false);
uint64_t binom(int n, int k);
uint64_t subfactor(int k, int n);
class PairsData {
public:
  int indextable;
  int sizetable;
  int data;
  int offset;
  std::vector<int> symlen;
  int sympat;
  int blocksize;
  int idxbits;
  int min_len;
  std::vector<uint64_t> base;
};

class PawnFileData {
public:
  std::map<int, PairsData> precomp;
  std::map<int, std::vector<uint64_t>> factor;
  std::map<int, std::vector<int>> pieces;
  std::map<int, std::vector<int>> norm;
};
class PawnFileDataDtz {
public:
  PairsData precomp;
  std::vector<uint64_t> factor;
  std::vector<int> pieces;
  std::vector<int> norm;
};
class Table {
public:
  std::vector<int> size;
  std::string path;

  std::recursive_mutex write_lock;
  bool initialized;
  uint8_t *data;
  size_t data_size;

  std::condition_variable_any read_condition;
  int read_count;

  std::string key;
  std::string mirrored_key;
  bool symmetric;

  int num;
  bool has_pawns;
  std::vector<int> pawns;
  int enc_type;
  uint8_t _flags;
  uint32_t _next;
  Table(std::string path);
  virtual ~Table() { close(); }
  virtual bool is_dtz() const { return false; }
  void init_mmap();
  void check_magic(const std::vector<uint8_t> &magic,
                   const std::vector<uint8_t> &pawnless_magic);
  PairsData setup_pairs(uint32_t data_ptr, uint32_t tb_size, int size_idx,
                        int wdl);
  void set_norm_piece(std::vector<int> &norm,
                      const std::vector<chess::PieceType> &pieces);
  uint64_t calc_factors_piece(std::vector<uint64_t> &factor, int order,
                              const std::vector<int> &norm);
  uint64_t calc_factors_pawn(std::vector<uint64_t> &factor, int order,
                             int order2, const std::vector<int> &norm,
                             uint64_t f);
  void set_norm_pawn(std::vector<int> &norm, const std::vector<int> &pieces);
  void calc_symlen(PairsData &d, int s, std::vector<int> &tmp);
  chess::File pawn_file(std::vector<chess::Square> &pos);
  uint64_t encode_piece(std::vector<int> &norm, std::vector<chess::Square> pos,
                        std::vector<uint64_t> &factor);
  uint64_t encode_pawn(std::vector<int> &norm, std::vector<chess::Square> pos,
                       std::vector<uint64_t> &factor);
  int decompress_pairs(PairsData &d, uint64_t idx);
  uint64_t read_uint64_be(uint32_t data_ptr);
  uint32_t read_uint32(uint32_t data_ptr);
  uint32_t read_uint32_be(uint32_t data_ptr);
  uint16_t read_uint16(uint32_t data_ptr);
  void close();
};
class WdlTable : public Table {
public:
  std::vector<int> tb_size;
  std::unordered_map<int, PairsData> precomp;
  std::unordered_map<int, std::vector<chess::PieceType>> pieces;
  std::vector<std::vector<uint64_t>> factor;
  std::vector<int> norm[2];
  std::vector<PawnFileData> files;

  inline WdlTable(const std::string &path) : Table(path) {
    Table::_next = 0;
    Table::_flags = 0;
    factor.resize(2, std::vector<uint64_t>(TBPIECES, 0));
  }
  void init_table_wdl();
  void setup_pieces_pawn(size_t p_data, int p_tb_size, int f);
  void setup_pieces_piece(size_t p_data);
  int probe_wdl_table(const chess::Board &board);
  int _probe_wdl_table(const chess::Board &board);
};
class DtzTable : public Table {
public:
  std::vector<uint64_t> factor;
  std::vector<int> norm;
  std::vector<int> tb_size;
  std::vector<PawnFileDataDtz> files;
  std::vector<std::vector<size_t>> map_idx;
  std::variant<int, std::vector<int>> flags;
  PairsData precomp;
  size_t p_map;
  std::vector<chess::PieceType> pieces;

  inline DtzTable(const std::string &path) : Table(path) {
    Table::_next = 0;
    Table::_flags = 0;
    factor.assign(TBPIECES, 0);
    norm.assign(num, 0);
    tb_size.assign(4, 0);
    size.assign(12, 0);
    files.assign(4, PawnFileDataDtz());
  }
  void init_table_dtz();
  std::pair<int, int> probe_dtz_table(const chess::Board &board, int wdl);
  std::pair<int, int> _probe_dtz_table(const chess::Board &board, int wdl);
  void setup_pieces_piece_dtz(size_t p_data, int p_tb_size);
  void setup_pieces_pawn_dtz(size_t p_data, int p_tb_size, int f);
};
class Tablebase {
public:
  /**
   * Manages a collection of tablebase files for probing.
   */
  inline Tablebase(std::optional<int> max_fds = 128) : max_fds(max_fds) {}

  std::optional<int> max_fds;
  std::deque<Table *> lru;
  std::mutex lru_lock;

  std::unordered_map<std::string, Table *> wdl;
  std::unordered_map<std::string, Table *> dtz;
  int add_directory(const std::string &directory, bool load_wdl = true,
                    bool load_dtz = true);
  int add_file(const std::string &path, bool load_wdl = true,
               bool load_dtz = true);
  int probe_wdl_table(chess::Board &board);
  std::pair<int, int> probe_ab(chess::Board &board, int alpha, int beta,
                               bool threats = false);
  std::pair<int, int> sprobe_ab(chess::Board &board, int alpha, int beta,
                                bool threats = false);
  std::pair<int, int> sprobe_capts(chess::Board &board, int alpha, int beta);
  int probe_wdl(chess::Board &board);
  std::optional<int> get_wdl(chess::Board &board,
                             std::optional<int> default_val = std::nullopt);
  std::pair<int, int> probe_dtz_table(chess::Board &board, int wdl_val);
  int probe_dtz_no_ep(chess::Board &board);
  int probe_dtz(chess::Board &board);
  std::optional<int> get_dtz(chess::Board &board,
                             std::optional<int> default_val = std::nullopt);
  void close();
  inline ~Tablebase() { this->close(); }

private:
  void _bump_lru(Table *table);

  template <typename T>
  inline int _open_table(std::unordered_map<std::string, Table *> &hashtable,
                         const std::string &path) {
    T *table = new T(path);

    if (hashtable.find(table->key) != hashtable.end()) {
      hashtable[table->key]->close();
      delete hashtable[table->key];
      hashtable.erase(table->key);
    }

    hashtable[table->key] = table;
    hashtable[table->mirrored_key] = table;
    return 1;
  }
};
std::unique_ptr<Tablebase> open_tablebase(const std::string &directory,
                                          bool load_wdl = true,
                                          bool load_dtz = true,
                                          std::optional<int> max_fds = 128);
void initialize();
} // namespace tbprobe::syzygy
#endif
