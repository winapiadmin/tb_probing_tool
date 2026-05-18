#include "tbprobe.h"
#include <condition_variable>
#include <mutex>
#include <numeric>
#include <position.h>
#include <set>
#include <unordered_map>
#include <variant>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <filesystem>
using namespace chess;
namespace tbprobe::syzygy {
std::array<std::array<int, 24>, 5> PAWNIDX{{}};
std::array<std::array<int, 24>, 5> PFACTOR{{}};
std::array<std::array<int, 10>, 5> MULTIDX{{}};
std::array<int, 5> MFACTOR{{}};

void initialize() {
  for (int i = 0; i < 5; ++i) {
    int j = 0;

    int s = 0;
    while (j < 6) {
      PAWNIDX[i][j] = s;
      s += (i == 0) ? 1 : (int)binom(PTWIST[INVFLAP[j]], i);
      j += 1;
    }
    PFACTOR[i][0] = s;

    s = 0;
    while (j < 12) {
      PAWNIDX[i][j] = s;
      s += (i == 0) ? 1 : (int)binom(PTWIST[INVFLAP[j]], i);
      j += 1;
    }
    PFACTOR[i][1] = s;

    s = 0;
    while (j < 18) {
      PAWNIDX[i][j] = s;
      s += (i == 0) ? 1 : (int)binom(PTWIST[INVFLAP[j]], i);
      j += 1;
    }
    PFACTOR[i][2] = s;

    s = 0;
    while (j < 24) {
      PAWNIDX[i][j] = s;
      s += (i == 0) ? 1 : (int)binom(PTWIST[INVFLAP[j]], i);
      j += 1;
    }
    PFACTOR[i][3] = s;
  }

  for (int i = 0; i < 5; ++i) {
    int s = 0;
    for (int j = 0; j < 10; ++j) {
      MULTIDX[i][j] = s;
      s += (i == 0) ? 1 : (int)binom(MTWIST[INVTRIANGLE[j]], i);
    }
    MFACTOR[i] = s;
  }
}

std::regex TABLEBASE_REGEX("^[KQRBNP]+v[KQRBNP]+$");

std::string normalize_tablename(const std::string &name, bool mirror) {
  auto index_in_PCHR = [](char c) -> int {
    auto pos = PCHR.find(c);
    return pos == std::string_view::npos ? -1 : static_cast<int>(pos);
  };
  auto pos = name.find('v');
  std::string w = name.substr(0, pos);
  std::string b = name.substr(pos + 1);

  std::sort(w.begin(), w.end(), [index_in_PCHR](char a, char b) {
    return index_in_PCHR(a) < index_in_PCHR(b);
  });
  std::sort(b.begin(), b.end(), [index_in_PCHR](char a, char b) {
    return index_in_PCHR(a) < index_in_PCHR(b);
  });

  std::vector<int> b_indices;
  for (char c : b) {
    b_indices.push_back(index_in_PCHR(c));
  }
  std::vector<int> w_indices;
  for (char c : w) {
    w_indices.push_back(index_in_PCHR(c));
  }

  bool condition = (std::make_pair(w.size(), b_indices) <
                    std::make_pair(b.size(), w_indices));
  if (mirror ^ condition) {
    return b + "v" + w;
  } else {
    return w + "v" + b;
  }
}
bool is_tablename(const std::string &name, bool one_king,
                  std::optional<int> piece_count, bool normalized) {
  return ((!piece_count || name.size() <= piece_count.value_or(TBPIECES) + 1) &&
          (std::regex_match(name, TABLEBASE_REGEX) &&
           (!normalized || normalize_tablename(name) == name)) &&
          (!one_king || (name != "KvK" && name[0] == 'K' &&
                         name.find("vK") != std::string::npos)));
}

std::vector<std::string> _dependencies(const std::string &target,
                                       bool one_king) {
  std::vector<std::string> result;
  auto pos = target.find('v');
  std::string w = target.substr(0, pos);
  std::string b = target.substr(pos + 1);

  for (char p : PCHR) {
    if (p == 'K' && one_king) {
      continue;
    }

    // Promotions.
    if (p != 'P' && w.find('P') != std::string::npos) {
      std::string new_w = w;
      auto idx = new_w.find('P');
      new_w.replace(idx, 1, 1, p);
      result.push_back(normalize_tablename(new_w + "v" + b));
    }
    if (p != 'P' && b.find('P') != std::string::npos) {
      std::string new_b = b;
      auto idx = new_b.find('P');
      new_b.replace(idx, 1, 1, p);
      result.push_back(normalize_tablename(w + "v" + new_b));
    }

    // Captures.
    if (w.find(p) != std::string::npos && w.size() > 1) {
      std::string new_w = w;
      auto idx = new_w.find(p);
      new_w.erase(idx, 1);
      result.push_back(normalize_tablename(new_w + "v" + b));
    }
    if (b.find(p) != std::string::npos && b.size() > 1) {
      std::string new_b = b;
      auto idx = new_b.find(p);
      new_b.erase(idx, 1);
      result.push_back(normalize_tablename(w + "v" + new_b));
    }
  }
  return result;
}

std::vector<std::string> dependencies(const std::string &target,
                                      bool one_king) {
  std::set<std::string> closed;
  std::vector<std::string> result;
  if (one_king) {
    closed.insert("KvK");
  }

  for (const auto &dependency : _dependencies(target, one_king)) {
    if (closed.find(dependency) == closed.end() && dependency.size() > 2) {
      result.push_back(dependency);
      closed.insert(dependency);
    }
  }
  return result;
}

std::vector<std::string>
all_dependencies(const std::vector<std::string> &targets, bool one_king) {
  std::set<std::string> closed;
  if (one_king) {
    closed.insert("KvK");
  }

  std::vector<std::string> open_list;
  for (const auto &target : targets) {
    open_list.push_back(normalize_tablename(target));
  }

  std::vector<std::string> result;
  while (!open_list.empty()) {
    std::string name = open_list.back();
    open_list.pop_back();

    if (closed.find(name) != closed.end()) {
      continue;
    }

    result.push_back(name);
    closed.insert(name);

    std::vector<std::string> deps = _dependencies(name, one_king);
    open_list.insert(open_list.end(), deps.begin(), deps.end());
  }
  return result;
}

std::vector<std::string> tablenames(bool one_king, int piece_count) {
  std::string first = one_king ? "K" : "P";
  std::vector<std::string> targets;

  int white_pieces = piece_count - 2;
  int black_pieces = 0;
  while (white_pieces >= black_pieces) {
    std::string s = first + std::string(white_pieces, 'P') + "v" + first +
                    std::string(black_pieces, 'P');
    targets.push_back(s);
    white_pieces -= 1;
    black_pieces += 1;
  }

  return all_dependencies(targets, one_king);
}

/**
 * Calculates a key string representing the piece configuration on the board.
 *
 * @param board The chess board object.
 * @param mirror If true, white and black pieces are swapped in the key.
 * @return A string key like "KQRBNP v KQRBNP".
 */
std::string calc_key(const Position &board, bool mirror) {
  Color w = static_cast<Color>(chess::WHITE ^ (mirror ? 1 : 0));
  Color b = static_cast<Color>(chess::BLACK ^ (mirror ? 1 : 0));

  return std::string(chess::popcount(board.pieces<chess::KING>(w)), 'K') +
         std::string(chess::popcount(board.pieces<chess::QUEEN>(w)), 'Q') +
         std::string(chess::popcount(board.pieces<chess::ROOK>(w)), 'R') +
         std::string(chess::popcount(board.pieces<chess::BISHOP>(w)), 'B') +
         std::string(chess::popcount(board.pieces<chess::KNIGHT>(w)), 'N') +
         std::string(chess::popcount(board.pieces<chess::PAWN>(w)), 'P') + "v" +
         std::string(chess::popcount(board.pieces<chess::KING>(b)), 'K') +
         std::string(chess::popcount(board.pieces<chess::QUEEN>(b)), 'Q') +
         std::string(chess::popcount(board.pieces<chess::ROOK>(b)), 'R') +
         std::string(chess::popcount(board.pieces<chess::BISHOP>(b)), 'B') +
         std::string(chess::popcount(board.pieces<chess::KNIGHT>(b)), 'N') +
         std::string(chess::popcount(board.pieces<chess::PAWN>(b)), 'P');
}

/**
 * Recalculates a key string from a list of piece types.
 *
 * @param pieces A list of piece type integers (e.g., KING | (COLOR << 3)).
 * @param mirror If true, white and black pieces are swapped.
 * @return A string key representing the endgame configuration.
 */
std::string recalc_key(const std::vector<chess::PieceType> &pieces,
                       bool mirror) {
  // Some endgames are stored with a different key than their filename
  // indicates: http://talkchess.com/forum/viewtopic.php?p=695509#695509

  int w = mirror ? 8 : 0;
  int b = mirror ? 0 : 8;

  // Helper lambda to mimic Python's list.count()
  auto count_pieces = [&](int value) {
    return (int)std::count(pieces.begin(), pieces.end(), value);
  };

  return std::string(count_pieces(6 ^ w), 'K') +
         std::string(count_pieces(5 ^ w), 'Q') +
         std::string(count_pieces(4 ^ w), 'R') +
         std::string(count_pieces(3 ^ w), 'B') +
         std::string(count_pieces(2 ^ w), 'N') +
         std::string(count_pieces(1 ^ w), 'P') + "v" +
         std::string(count_pieces(6 ^ b), 'K') +
         std::string(count_pieces(5 ^ b), 'Q') +
         std::string(count_pieces(4 ^ b), 'R') +
         std::string(count_pieces(3 ^ b), 'B') +
         std::string(count_pieces(2 ^ b), 'N') +
         std::string(count_pieces(1 ^ b), 'P');
}
// Helper functions usually defined in the syzygy module
uint64_t binom(int n, int k) {
  if (k < 0 || k > n)
    return 0;
  if (k == 0 || k == n)
    return 1;
  if (k > n / 2)
    k = n - k;
  uint64_t res = 1;
  for (int i = 1; i <= k; ++i) {
    res = res * (n - i + 1) / i;
  }
  return res;
}

uint64_t subfactor(int k, int n) { return binom(n, k); }

int dtz_before_zeroing(int wdl) {
  return ((wdl > 0) - (wdl < 0)) * (abs(wdl) == 2 ? 1 : 101);
}
int offdiag(Square s) {
  int r = s >> 3;
  int f = s & 7;
  return r - f;
}

Square flipdiag(Square s) {
  return static_cast<Square>(((s >> 3) & 7) | ((s & 7) << 3));
}

bool test45(Square s) { return (s >> 3) >= 4; }

// Byte reading helpers
uint16_t read_u16_le(const uint8_t *data, size_t pos) {
  return data[pos] | (data[pos + 1] << 8);
}

uint32_t read_u32_le(const uint8_t *data, size_t pos) {
  return data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16) |
         (data[pos + 3] << 24);
}

uint32_t read_u32_be(const uint8_t *data, size_t pos) {
  return (data[pos] << 24) | (data[pos + 1] << 16) | (data[pos + 2] << 8) |
         data[pos + 3];
}

uint64_t read_u64_be(const uint8_t *data, size_t pos) {
  uint64_t res = 0;
  for (int i = 0; i < 8; ++i)
    res = (res << 8) | data[pos + i];
  return res;
}

Table::Table(std::string path) : path(path) {
  this->initialized = false;
  this->data = nullptr;
  this->data_size = 0;
  this->read_count = 0;

  size_t last_slash = path.find_last_of("/\\");
  std::string filename =
      (last_slash == std::string::npos) ? path : path.substr(last_slash + 1);
  size_t last_dot = filename.find_last_of(".");
  std::string tablename =
      (last_dot == std::string::npos) ? filename : filename.substr(0, last_dot);

  this->key = normalize_tablename(tablename);
  this->mirrored_key = normalize_tablename(tablename, true);
  this->symmetric = (this->key == this->mirrored_key);

  // Leave the v out of the tablename to get the number of pieces.
  this->num = tablename.length() - 1;

  this->has_pawns = (tablename.find('P') != std::string::npos);

  size_t v_pos = tablename.find('v');
  std::string black_part = tablename.substr(0, v_pos);
  std::string white_part = tablename.substr(v_pos + 1);

  if (this->has_pawns) {
    int white_pawns = std::count(white_part.begin(), white_part.end(), 'P');
    int black_pawns = std::count(black_part.begin(), black_part.end(), 'P');

    this->pawns = {white_pawns, black_pawns};
    if (this->pawns[1] > 0 &&
        (this->pawns[0] == 0 || this->pawns[1] < this->pawns[0])) {
      std::swap(this->pawns[0], this->pawns[1]);
    }
  } else {
    int j = 0;
    for (char piece_type : PCHR) {
      int b_count =
          std::count(black_part.begin(), black_part.end(), piece_type);
      int w_count =
          std::count(white_part.begin(), white_part.end(), piece_type);
      if (b_count == 1)
        j++;

      if (w_count == 1)
        j++;
    }
    if (j >= 3) {
      this->enc_type = 0;
    } else if (j == 2) {
      this->enc_type = 2;
    } else { // only for suicide
      j = 16;
      for (int _repeat = 0; _repeat < 16; ++_repeat) {
        for (char piece_type : PCHR) {
          int b_count = 0;
          for (char c : black_part)
            if (c == piece_type)
              b_count++;
          if (b_count > 1 && b_count < j)
            j = b_count;

          int w_count = 0;
          for (char c : white_part)
            if (c == piece_type)
              w_count++;
          if (w_count > 1 && w_count < j)
            j = w_count;

          this->enc_type = 1 + j;
        }
      }
    }
  }
}

void Table::init_mmap() {
  if (this->data == nullptr) {
#ifndef _WIN32
    int fd = open(this->path.c_str(), O_RDONLY);
    if (fd == -1)
      throw std::runtime_error("Could not open file");

    struct stat st;
    fstat(fd, &st);
    this->data_size = st.st_size;

    void *map = mmap(NULL, this->data_size, PROT_READ, MAP_PRIVATE, fd, 0);
    ::close(fd);

    if (map == MAP_FAILED)
      throw std::runtime_error("mmap failed");
    this->data = static_cast<uint8_t *>(map);

    if (this->data_size % 64 != 16) {
      throw std::runtime_error("invalid file size: ensure " + this->path +
                               " is a valid syzygy tablebase file");
    }

#ifdef MADV_RANDOM
    madvise(this->data, this->data_size, MADV_RANDOM);
#endif
#else
    std::wstring wpath(this->path.begin(), this->path.end());

    HANDLE hFile = CreateFileW(
        wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
      throw std::runtime_error("Could not open file: " + this->path);
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
      CloseHandle(hFile);
      throw std::runtime_error("Could not get file size: " + this->path);
    }
    this->data_size = static_cast<size_t>(fileSize.QuadPart);

    if (this->data_size == 0) {
      CloseHandle(hFile);
      throw std::runtime_error("File is empty: " + this->path);
    }

    HANDLE hMap = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);

    CloseHandle(hFile);

    if (hMap == NULL) {
      throw std::runtime_error("CreateFileMapping failed for: " + this->path);
    }

    void *map = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);

    CloseHandle(hMap);

    if (map == NULL) {
      throw std::runtime_error("MapViewOfFile failed for: " + this->path);
    }

    this->data = static_cast<uint8_t *>(map);

    if (this->data_size % 64 != 16) {
      UnmapViewOfFile(this->data);
      this->data = nullptr;
      throw std::runtime_error("invalid file size: ensure " + this->path +
                               " is a valid syzygy tablebase file");
    }
#endif
  }
}

void Table::check_magic(const std::vector<uint8_t> &magic,
                        const std::vector<uint8_t> &pawnless_magic) {
  if (!this->data)
    throw std::runtime_error("Data not initialized");

  bool magic_match = true;
  for (size_t i = 0; i < std::min((size_t)4, magic.size()); ++i) {
    if (this->data[i] != magic[i]) {
      magic_match = false;
      break;
    }
  }

  bool pawnless_match = false;
  if (this->has_pawns && !pawnless_magic.empty()) {
    pawnless_match = true;
    for (size_t i = 0; i < std::min((size_t)4, pawnless_magic.size()); ++i) {
      if (this->data[i] != pawnless_magic[i]) {
        pawnless_match = false;
        break;
      }
    }
  }

  if (!magic_match && !pawnless_match) {
    throw std::runtime_error("invalid magic header: ensure " + this->path +
                             " is a valid syzygy tablebase file");
  }
}

PairsData Table::setup_pairs(uint32_t data_ptr, uint32_t tb_size, int size_idx,
                             int wdl) {
  if (!this->data)
    throw std::runtime_error("Data not initialized");

  PairsData d;

  this->_flags = this->data[data_ptr];
  if (this->data[data_ptr] & 0x80) {
    d.idxbits = 0;
    if (wdl) {
      d.min_len = this->data[data_ptr + 1];
    } else {
      d.min_len = /*this->variant.captures_compulsory*/ false ? 1 : 0;
    }
    this->_next = data_ptr + 2;
    this->size[size_idx + 0] = 0;
    this->size[size_idx + 1] = 0;
    this->size[size_idx + 2] = 0;
    return d;
  }

  d.blocksize = this->data[data_ptr + 1];
  d.idxbits = this->data[data_ptr + 2];

  uint32_t real_num_blocks = this->read_uint32(data_ptr + 4);
  uint32_t num_blocks = real_num_blocks + this->data[data_ptr + 3];
  uint8_t max_len = this->data[data_ptr + 8];
  uint8_t min_len = this->data[data_ptr + 9];
  int h = max_len - min_len + 1;
  uint16_t num_syms = this->read_uint16(data_ptr + 10 + 2 * h);

  d.offset = data_ptr + 10;
  d.symlen.assign(h * 8 + num_syms, 0);
  d.sympat = data_ptr + 12 + 2 * h;
  d.min_len = min_len;

  this->_next = data_ptr + 12 + 2 * h + 3 * num_syms + (num_syms & 1);

  uint32_t num_indices = (tb_size + (1LL << d.idxbits) - 1) >> d.idxbits;
  this->size[size_idx + 0] = 6 * num_indices;
  this->size[size_idx + 1] = 2 * num_blocks;
  this->size[size_idx + 2] = (1LL << d.blocksize) * real_num_blocks;

  std::vector<int> tmp(num_syms, 0);
  for (int i = 0; i < num_syms; ++i) {
    if (!tmp[i]) {
      this->calc_symlen(d, i, tmp);
    }
  }

  d.base.assign(h, 0);
  d.base[h - 1] = 0;
  for (int i = h - 2; i >= 0; --i) {
    d.base[i] = (d.base[i + 1] + this->read_uint16(d.offset + i * 2) -
                 this->read_uint16(d.offset + i * 2 + 2)) /
                2;
  }
  for (int i = 0; i < h; ++i) {
    d.base[i] <<= 64 - (min_len + i);
  }

  d.offset -= 2 * d.min_len;

  return d;
}

void Table::set_norm_piece(std::vector<int> &norm,
                           const std::vector<PieceType> &pieces) {
  if (this->enc_type == 0) {
    norm[0] = 3;
  } else if (this->enc_type == 2) {
    norm[0] = 2;
  } else {
    norm[0] = this->enc_type - 1;
  }

  int i = norm[0];
  while (i < this->num) {
    int j = i;
    while (j < this->num && pieces[j] == pieces[i]) {
      norm[i]++;
      j++;
    }
    i += norm[i];
  }
}

uint64_t Table::calc_factors_piece(std::vector<uint64_t> &factor, int order,
                                   const std::vector<int> &norm) {
  std::vector<int> PIVFAC;
  if (false) { // this->variant.connected_kings) {
    PIVFAC = {31332, 0, 518, 278};
  } else {
    PIVFAC = {31332, 28056, 462};
  }

  int n = 64 - norm[0];
  uint64_t f = 1;
  int i = norm[0];
  int k = 0;
  while (i < this->num || k == order) {
    if (k == order) {
      factor[0] = f;
      if (this->enc_type < 4) {
        f *= PIVFAC[this->enc_type];
      } else {
        f *= MFACTOR[this->enc_type - 2];
      }
    } else {
      factor[i] = f;
      f *= subfactor(norm[i], n);
      n -= norm[i];
      i += norm[i];
    }
    k++;
  }
  return f;
}

uint64_t Table::calc_factors_pawn(std::vector<uint64_t> &factor, int order,
                                  int order2, const std::vector<int> &norm,
                                  uint64_t f) {
  int i = norm[0];
  if (order2 < 0x0f) {
    i += norm[i];
  }
  int n = 64 - i;

  uint64_t fac = 1;
  int k = 0;
  while (i < this->num || k == order || k == order2) {
    if (k == order) {
      factor[0] = fac;
      fac *= PFACTOR[norm[0] - 1][f];
    } else if (k == order2) {
      factor[norm[0]] = fac;
      fac *= subfactor(norm[norm[0]], 48 - norm[0]);
    } else {
      factor[i] = fac;
      fac *= subfactor(norm[i], n);
      n -= norm[i];
      i += norm[i];
    }
    k++;
  }
  return fac;
}

void Table::set_norm_pawn(std::vector<int> &norm,
                          const std::vector<int> &pieces) {
  norm[0] = this->pawns[0];
  if (this->pawns[1]) {
    norm[this->pawns[0]] = this->pawns[1];
  }

  int i = this->pawns[0] + this->pawns[1];
  while (i < this->num) {
    int j = i;
    while (j < this->num && pieces[j] == pieces[i]) {
      norm[i]++;
      j++;
    }
    i += norm[i];
  }
}

void Table::calc_symlen(PairsData &d, int s, std::vector<int> &tmp) {
  if (!this->data)
    throw std::runtime_error("Data not initialized");
  if (s < 0 || s >= (int)tmp.size())
    throw std::runtime_error("calc_symlen index out of range: " +
                             std::to_string(s));

  if (tmp[s])
    return; // optional cycle guard
  uint32_t w = d.sympat + 3 * s;
  int s2 = (this->data[w + 2] << 4) | (this->data[w + 1] >> 4);
  if (s2 == 0x0fff) {
    d.symlen[s] = 0;
  } else {
    int s1 = ((this->data[w + 1] & 0xf) << 8) | this->data[w];
    if (!tmp[s1]) {
      this->calc_symlen(d, s1, tmp);
    }
    if (!tmp[s2]) {
      this->calc_symlen(d, s2, tmp);
    }
    d.symlen[s] = d.symlen[s1] + d.symlen[s2] + 1;
  }
  tmp[s] = 1;
}

File Table::pawn_file(std::vector<Square> &pos) {
  for (int i = 1; i < this->pawns[0]; ++i) {
    if (FLAP[pos[0]] > FLAP[pos[i]]) {
      std::swap(pos[0], pos[i]);
    }
  }
  return static_cast<File>(FILE_TO_FILE[pos[0] & 0x07]);
}

uint64_t Table::encode_piece(std::vector<int> &norm, std::vector<Square> pos,
                             std::vector<uint64_t> &factor) {
  int n = this->num;

  if (this->enc_type < 3) {
    if (pos[0] & 0x04) {
      for (int i = 0; i < n; ++i)
        pos[i] = static_cast<Square>(pos[i] ^ 0x07);
    }
    if (pos[0] & 0x20) {
      for (int i = 0; i < n; ++i)
        pos[i] = static_cast<Square>(pos[i] ^ 0x38);
    }

    int i = 0;
    for (i = 0; i < n; ++i) {
      if (offdiag(pos[i]))
        break;
    }
    if (i < (this->enc_type == 0 ? 3 : 2) && offdiag(pos[i]) > 0) {
      for (int k = 0; k < n; ++k)
        pos[k] = flipdiag(pos[k]);
    }
  }

  uint64_t idx = 0;
  int i = 0;

  if (this->enc_type == 0) { // 111
    int b10 = (pos[1] > pos[0]);
    int b20 = (pos[2] > pos[0]);
    int b21 = (pos[2] > pos[1]);
    int j_val = b20 + b21;

    if (offdiag(pos[0])) {
      idx = (uint64_t)TRIANGLE[pos[0]] * 63 * 62 + (pos[1] - b10) * 62 +
            (pos[2] - j_val);
    } else if (offdiag(pos[1])) {
      idx = 6ULL * 63 * 62 + (uint64_t)DIAG[pos[0]] * 28 * 62 +
            (uint64_t)LOWER[pos[1]] * 62 + pos[2] - j_val;
    } else if (offdiag(pos[2])) {
      idx = 6ULL * 63 * 62 + 4ULL * 28 * 62 + (uint64_t)DIAG[pos[0]] * 7 * 28 +
            (uint64_t)(DIAG[pos[1]] - b10) * 28 + LOWER[pos[2]];
    } else {
      idx = 6ULL * 63 * 62 + 4ULL * 28 * 62 + 4ULL * 7 * 28 +
            ((uint64_t)DIAG[pos[0]] * 7 * 6) +
            (uint64_t)(DIAG[pos[1]] - b10) * 6 + (pos[2] - j_val);
    }
    i = 3;
  } else if (this->enc_type == 2) { // K2
    if (!(/*this->variant.connected_kings*/ false)) {
      idx = KK_IDX[TRIANGLE[pos[0]]][pos[1]];
    } else {
      int b10 = pos[1] > pos[0];
      if (offdiag(pos[0])) {
        idx = (uint64_t)TRIANGLE[pos[0]] * 63 + (pos[1] - b10);
      } else if (offdiag(pos[1])) {
        idx = 6ULL * 63 + (uint64_t)DIAG[pos[0]] * 28 + LOWER[pos[1]];
      } else {
        idx = 6ULL * 63 + 4ULL * 28 + (uint64_t)DIAG[pos[0]] * 7 +
              (DIAG[pos[1]] - b10);
      }
    }
    i = 2;
  } else if (this->enc_type == 3) { // 2, e.g. KKvK
    if (TRIANGLE[pos[0]] > TRIANGLE[pos[1]])
      std::swap(pos[0], pos[1]);
    if (pos[0] & 0x04)
      for (int k = 0; k < n; ++k)
        pos[k] = static_cast<Square>(pos[k] ^ 0x07);
    if (pos[0] & 0x20)
      for (int k = 0; k < n; ++k)
        pos[k] = static_cast<Square>(pos[k] ^ 0x38);
    if (offdiag(pos[0]) > 0 || (offdiag(pos[0]) == 0 && offdiag(pos[1]) > 0)) {
      for (int k = 0; k < n; ++k)
        pos[k] = flipdiag(pos[k]);
    }
    if (test45(pos[1]) && TRIANGLE[pos[0]] == TRIANGLE[pos[1]]) {
      std::swap(pos[0], pos[1]);
      for (int k = 0; k < n; ++k)
        pos[k] = flipdiag(static_cast<Square>((int)pos[k] ^ 0x38));
    }
    idx = PP_IDX[TRIANGLE[pos[0]]][pos[1]];
    i = 2;
  } else { // 3 and higher, e.g. KKKvK and KKKKvK
    for (int k = 1; k < norm[0]; ++k) {
      if (TRIANGLE[pos[0]] > TRIANGLE[pos[k]])
        std::swap(pos[0], pos[k]);
    }
    if (pos[0] & 0x04)
      for (int k = 0; k < n; ++k)
        pos[k] = static_cast<Square>(pos[k] ^ 0x07);
    if (pos[0] & 0x20)
      for (int k = 0; k < n; ++k)
        pos[k] = static_cast<Square>(pos[k] ^ 0x38);
    if (offdiag(pos[0]) > 0)
      for (int k = 0; k < n; ++k)
        pos[k] = flipdiag(pos[k]);
    for (int k = 1; k < norm[0]; ++k) {
      for (int l = k + 1; l < norm[0]; ++l) {
        if (MTWIST[pos[k]] > MTWIST[pos[l]])
          std::swap(pos[k], pos[l]);
      }
    }

    idx = MULTIDX[norm[0] - 1][TRIANGLE[pos[0]]];
    i = 1;
    while (i < norm[0]) {
      idx += binom(MTWIST[pos[i]], i);
      i++;
    }
  }

  idx *= factor[0];

  while (i < n) {
    int t = norm[i];
    for (int j = i; j < i + t; ++j) {
      for (int k = j + 1; k < i + t; ++k) {
        if (pos[j] > pos[k])
          std::swap(pos[j], pos[k]);
      }
    }

    uint64_t s = 0;
    for (int m = i; m < i + t; ++m) {
      Square p = pos[m];
      int j_count = 0;
      for (int l = 0; l < i; ++l) {
        if (p > pos[l])
          j_count++;
      }
      s += binom(p - j_count, m - i + 1);
    }

    idx += s * factor[i];
    i += t;
  }

  return idx;
}

uint64_t Table::encode_pawn(std::vector<int> &norm, std::vector<Square> pos,
                            std::vector<uint64_t> &factor) {
  int n = this->num;

  if (pos[0] & 0x04) {
    for (int i = 0; i < n; ++i)
      pos[i] = static_cast<Square>(pos[i] ^ 0x07);
  }

  for (int i = 1; i < this->pawns[0]; ++i) {
    for (int j = i + 1; j < this->pawns[0]; ++j) {
      if (PTWIST[pos[i]] < PTWIST[pos[j]])
        std::swap(pos[i], pos[j]);
    }
  }

  int t_val = this->pawns[0] - 1;
  uint64_t idx = PAWNIDX[t_val][FLAP[pos[0]]];
  for (int i = t_val; i > 0; --i) {
    idx += binom(PTWIST[pos[i]], t_val - i + 1);
  }
  idx *= factor[0];

  // Remaining pawns.
  int i = this->pawns[0];
  int t = i + this->pawns[1];
  if (t > i) {
    for (int j = i; j < t; ++j) {
      for (int k = j + 1; k < t; ++k) {
        if (pos[j] > pos[k])
          std::swap(pos[j], pos[k]);
      }
    }
    uint64_t s = 0;
    for (int m = i; m < t; ++m) {
      Square p = pos[m];
      int j_count = 0;
      for (int k = 0; k < i; ++k) {
        if (p > pos[k])
          j_count++;
      }
      s += binom(p - j_count - 8, m - i + 1);
    }
    idx += s * factor[i];
    i = t;
  }

  while (i < n) {
    int t_sub = norm[i];
    for (int j = i; j < i + t_sub; ++j) {
      for (int k = j + 1; k < i + t_sub; ++k) {
        if (pos[j] > pos[k])
          std::swap(pos[j], pos[k]);
      }
    }

    uint64_t s = 0;
    for (int m = i; m < i + t_sub; ++m) {
      Square p = pos[m];
      int j_count = 0;
      for (int k = 0; k < i; ++k) {
        if (p > pos[k])
          j_count++;
      }
      s += binom(p - j_count, m - i + 1);
    }

    idx += s * factor[i];
    i += t_sub;
  }

  return idx;
}

int Table::decompress_pairs(PairsData &d, uint64_t idx) {
  if (!this->data)
    throw std::runtime_error("Data not initialized");

  if (!d.idxbits) {
    return d.min_len;
  }

  uint64_t mainidx = idx >> d.idxbits;
  int64_t litidx =
      (idx & ((1ULL << d.idxbits) - 1)) - (1ULL << (d.idxbits - 1));
  uint32_t block = this->read_uint32(d.indextable + 6 * mainidx);

  uint16_t idx_offset = this->read_uint16(d.indextable + 6 * mainidx + 4);
  litidx += idx_offset;

  if (litidx < 0) {
    while (litidx < 0) {
      block -= 1;
      litidx += this->read_uint16(d.sizetable + 2 * block) + 1;
    }
  } else {
    while (litidx > (int64_t)this->read_uint16(d.sizetable + 2 * block)) {
      litidx -= (int64_t)this->read_uint16(d.sizetable + 2 * block) + 1;
      block += 1;
    }
  }

  uint32_t ptr = d.data + (block << d.blocksize);

  int m = d.min_len;
  int base_idx = -m;
  int symlen_idx = 0;

  uint64_t code = this->read_uint64_be(ptr);

  ptr += 2 * 4;
  int bitcnt = 0; // Number of empty bits in code
  int sym;
  while (true) {
    int l = m;
    while (code < d.base[base_idx + l]) {
      l += 1;
    }
    sym = this->read_uint16(d.offset + l * 2);
    sym += (code - d.base[base_idx + l]) >> (64 - l);
    if (litidx < (int64_t)d.symlen[symlen_idx + sym] + 1) {
      break;
    }
    litidx -= (int64_t)d.symlen[symlen_idx + sym] + 1;
    code <<= l;
    bitcnt += l;
    if (bitcnt >= 32) {
      bitcnt -= 32;
      code |= (uint64_t)this->read_uint32_be(ptr) << bitcnt;
      ptr += 4;
    }
    code &= 0xFFFFFFFFFFFFFFFFULL;
  }

  uint32_t sympat = d.sympat;
  while (d.symlen[symlen_idx + sym]) {
    uint32_t w = sympat + 3 * sym;
    int s1 = ((this->data[w + 1] & 0xf) << 8) | this->data[w];
    if (litidx < (int64_t)d.symlen[symlen_idx + s1] + 1) {
      sym = s1;
    } else {
      litidx -= (int64_t)d.symlen[symlen_idx + s1] + 1;
      sym = (this->data[w + 2] << 4) | (this->data[w + 1] >> 4);
    }
  }

  uint32_t w = sympat + 3 * sym;
  if (this->is_dtz()) {
    return ((this->data[w + 1] & 0x0f) << 8) | this->data[w];
  } else {
    return this->data[w];
  }
}

uint64_t Table::read_uint64_be(uint32_t data_ptr) {
  return read_u64_be(this->data, data_ptr);
}

uint32_t Table::read_uint32(uint32_t data_ptr) {
  return read_u32_le(this->data, data_ptr);
}

uint32_t Table::read_uint32_be(uint32_t data_ptr) {
  return read_u32_be(this->data, data_ptr);
}

uint16_t Table::read_uint16(uint32_t data_ptr) {
  return read_u16_le(this->data, data_ptr);
}
void Table::close() {
  std::unique_lock<std::recursive_mutex> lock(this->write_lock);

  while (this->read_count > 0) {
    this->read_condition.wait(lock);
  }

  if (this->data != nullptr) {
#ifndef _WIN32
    munmap(this->data, this->data_size);
#else
    UnmapViewOfFile(this->data);
#endif

    this->data = nullptr;
    this->data_size = 0;
  }
}
void WdlTable::init_table_wdl() {
  std::lock_guard<std::recursive_mutex> lock(write_lock);
  init_mmap();
  assert(data);

  if (initialized) {
    return;
  }

  check_magic({0x71, 0xe8, 0x23, 0x5d}, {});

  tb_size.assign(8, 0);
  size.assign(8 * 3, 0);

  // Used if there are only pieces.
  precomp.clear();
  pieces.clear();
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < TBPIECES; ++j)
      factor[i][j] = 0;
    norm[i].assign(num, 0);
  }

  // Used if there are pawns.
  files.assign(4, PawnFileData());

  int split = data[4] & 0x01;
  int num_files = (data[4] & 0x02) ? 4 : 1;

  size_t data_ptr = 5;

  if (!has_pawns) {
    setup_pieces_piece(data_ptr);
    data_ptr += num + 1;
    data_ptr += (data_ptr & 0x01);

    precomp[0] = setup_pairs(data_ptr, tb_size[0], 0, true);
    data_ptr = _next;
    if (split) {
      precomp[1] = setup_pairs(data_ptr, tb_size[1], 3, true);
      data_ptr = _next;
    }

    precomp[0].indextable = data_ptr;
    data_ptr += size[0];
    if (split) {
      precomp[1].indextable = data_ptr;
      data_ptr += size[3];
    }

    precomp[0].sizetable = data_ptr;
    data_ptr += size[1];
    if (split) {
      precomp[1].sizetable = data_ptr;
      data_ptr += size[4];
    }

    data_ptr = (data_ptr + 0x3f) & ~0x3f;
    precomp[0].data = data_ptr;
    data_ptr += size[2];
    if (split) {
      data_ptr = (data_ptr + 0x3f) & ~0x3f;
      precomp[1].data = data_ptr;
    }

    key = recalc_key(pieces[0]);
    mirrored_key = recalc_key(pieces[0], true);
  } else {
    int s = 1 + (int)(pawns[1] > 0);
    for (int f = 0; f < 4; ++f) {
      setup_pieces_pawn(data_ptr, 2 * f, f);
      data_ptr += num + s;
    }
    data_ptr += (data_ptr & 0x01);

    for (int f = 0; f < num_files; ++f) {
      files[f].precomp[0] = setup_pairs(data_ptr, tb_size[2 * f], 6 * f, true);
      data_ptr = _next;
      if (split) {
        files[f].precomp[1] =
            setup_pairs(data_ptr, tb_size[2 * f + 1], 6 * f + 3, true);
        data_ptr = _next;
      }
    }

    for (int f = 0; f < num_files; ++f) {
      files[f].precomp[0].indextable = data_ptr;
      data_ptr += size[6 * f];
      if (split) {
        files[f].precomp[1].indextable = data_ptr;
        data_ptr += size[6 * f + 3];
      }
    }

    for (int f = 0; f < num_files; ++f) {
      files[f].precomp[0].sizetable = data_ptr;
      data_ptr += size[6 * f + 1];
      if (split) {
        files[f].precomp[1].sizetable = data_ptr;
        data_ptr += size[6 * f + 4];
      }
    }

    for (int f = 0; f < num_files; ++f) {
      data_ptr = (data_ptr + 0x3f) & ~0x3f;
      files[f].precomp[0].data = data_ptr;
      data_ptr += size[6 * f + 2];
      if (split) {
        data_ptr = (data_ptr + 0x3f) & ~0x3f;
        files[f].precomp[1].data = data_ptr;
        data_ptr += size[6 * f + 5];
      }
    }
  }

  initialized = true;
}

void WdlTable::setup_pieces_pawn(size_t p_data, int p_tb_size, int f) {
  assert(data);

  int j = 1 + (int)(pawns[1] > 0);
  int order = data[p_data] & 0x0f;
  int order2 = pawns[1] ? (data[p_data + 1] & 0x0f) : 0x0f;

  files[f].pieces[0].resize(num);
  for (int i = 0; i < num; ++i)
    files[f].pieces[0][i] = static_cast<PieceType>(data[p_data + i + j] & 0x0f);

  files[f].norm[0].assign(num, 0);
  set_norm_pawn(files[f].norm[0], files[f].pieces[0]);
  files[f].factor[0].assign(TBPIECES, 0);
  tb_size[p_tb_size] =
      calc_factors_pawn(files[f].factor[0], order, order2, files[f].norm[0], f);

  order = data[p_data] >> 4;
  order2 = pawns[1] ? (data[p_data + 1] >> 4) : 0x0f;

  files[f].pieces[1].resize(num);
  for (int i = 0; i < num; ++i)
    files[f].pieces[1][i] = static_cast<PieceType>(data[p_data + i + j] >> 4);

  files[f].norm[1].assign(num, 0);
  set_norm_pawn(files[f].norm[1], files[f].pieces[1]);
  files[f].factor[1].assign(TBPIECES, 0);
  tb_size[p_tb_size + 1] =
      calc_factors_pawn(files[f].factor[1], order, order2, files[f].norm[1], f);
}

void WdlTable::setup_pieces_piece(size_t p_data) {
  assert(data);

  pieces[0].resize(num);
  for (int i = 0; i < num; ++i)
    pieces[0][i] = static_cast<PieceType>(data[p_data + i + 1] & 0x0f);
  int order = data[p_data] & 0x0f;
  set_norm_piece(norm[0], pieces[0]);
  tb_size[0] = calc_factors_piece(factor[0], order, norm[0]);

  pieces[1].resize(num);
  for (int i = 0; i < num; ++i)
    pieces[1][i] = static_cast<PieceType>(data[p_data + i + 1] >> 4);
  order = data[p_data] >> 4;
  set_norm_piece(norm[1], pieces[1]);
  tb_size[1] = calc_factors_piece(factor[1], order, norm[1]);
}

int WdlTable::probe_wdl_table(const chess::Board &board) {
  {
    std::lock_guard<std::recursive_mutex> lock(write_lock);
    read_count++;
  }

  int res = _probe_wdl_table(board);

  {
    std::lock_guard<std::recursive_mutex> lock(write_lock);
    read_count--;
    read_condition.notify_all();
  }

  return res;
}

int WdlTable::_probe_wdl_table(const chess::Board &board) {
  init_table_wdl();

  std::string key_str = calc_key(board);

  int cmirror, mirror, bside;
  if (!symmetric) {
    if (key_str != key) {
      cmirror = 8;
      mirror = 0x38;
      bside = (board.sideToMove() == chess::WHITE) ? 1 : 0;
    } else {
      cmirror = mirror = 0;
      bside = (board.sideToMove() != chess::WHITE) ? 1 : 0;
    }
  } else {
    cmirror = (board.sideToMove() == chess::WHITE) ? 0 : 8;
    mirror = (board.sideToMove() == chess::WHITE) ? 0 : 0x38;
    bside = 0;
  }

  int res = 0;
  if (!has_pawns) {
    std::vector<Square> p(TBPIECES, SQ_NONE);
    int i = 0;
    while (i < num) {
      PieceType piece_type = static_cast<PieceType>(pieces[bside][i] & 0x07);
      Color color = static_cast<Color>((pieces[bside][i] ^ cmirror) >> 3);
      chess::Bitboard bb =
          board.pieces(piece_type, (color == 0) ? chess::WHITE : chess::BLACK);

      while (bb) {
        chess::Square sq = (chess::Square)pop_lsb(bb);
        p[i] = sq;
        i++;
      }
    }

    size_t idx = encode_piece(norm[bside], p, factor[bside]);
    res = decompress_pairs(precomp[bside], idx);
  } else {
    std::vector<Square> p(TBPIECES, SQ_NONE);
    int i = 0;
    int k = files[0].pieces[0][0] ^ cmirror;
    Color color = static_cast<Color>(k >> 3);
    PieceType piece_type = static_cast<PieceType>(k & 0x07);
    chess::Bitboard bb =
        board.pieces(piece_type, (color == 0) ? chess::WHITE : chess::BLACK);

    while (bb) {
      Square sq = (Square)pop_lsb(bb);
      p[i] = static_cast<Square>(sq ^ mirror);
      i++;
    }

    int f = pawn_file(p);
    const std::vector<int> &pc = files[f].pieces[bside];
    while (i < num) {
      color = static_cast<Color>((pc[i] ^ cmirror) >> 3);
      piece_type = static_cast<PieceType>(pc[i] & 0x07);
      bb = board.pieces(piece_type, (color == 0) ? chess::WHITE : chess::BLACK);

      while (bb) {
        Square sq = (Square)pop_lsb(bb);
        p[i] = static_cast<Square>(sq ^ mirror);
        i++;
      }
    }

    size_t idx = encode_pawn(files[f].norm[bside], p, files[f].factor[bside]);
    res = decompress_pairs(files[f].precomp.at(bside), idx);
  }

  return res - 2;
}

void DtzTable::init_table_dtz() {
  std::lock_guard<std::recursive_mutex> lock(write_lock);
  init_mmap();
  assert(data);

  if (initialized) {
    return;
  }

  check_magic({0xd7, 0x66, 0x0c, 0xa5}, {});

  factor.assign(TBPIECES, 0);
  norm.assign(num, 0);
  tb_size.assign(4, 0);
  size.assign(12, 0);
  files.assign(4, PawnFileDataDtz());

  int num_files = (data[4] & 0x02) ? 4 : 1;

  size_t p_data = 5;

  if (!has_pawns) {
    map_idx.assign(1, std::vector<size_t>(4, 0));

    setup_pieces_piece_dtz(p_data, 0);
    p_data += num + 1;
    p_data += (p_data & 0x01);

    precomp = setup_pairs(p_data, tb_size[0], 0, false);
    flags = _flags;
    p_data = _next;
    p_map = p_data;

    int f_val = std::get<int>(flags);
    if (f_val & 2) {
      if (!(f_val & 16)) {
        for (int i = 0; i < 4; ++i) {
          map_idx[0][i] = p_data + 1 - p_map;
          p_data += 1 + data[p_data];
        }
      } else {
        for (int i = 0; i < 4; ++i) {
          map_idx[0][i] = (p_data + 2 - p_map) / 2;
          p_data += 2 + 2 * read_uint16(p_data);
        }
      }
    }
    p_data += (p_data & 0x01);

    precomp.indextable = p_data;
    p_data += size[0];

    precomp.sizetable = p_data;
    p_data += size[1];

    p_data = (p_data + 0x3f) & ~0x3f;
    precomp.data = p_data;
    p_data += size[2];

    key = recalc_key(pieces);
    mirrored_key = recalc_key(pieces, true);
  } else {
    int s = 1 + (int)(pawns[1] > 0);
    for (int f = 0; f < 4; ++f) {
      setup_pieces_pawn_dtz(p_data, f, f);
      p_data += num + s;
    }
    p_data += (p_data & 0x01);

    std::vector<int> flags_vec;
    for (int f = 0; f < num_files; ++f) {
      files[f].precomp = setup_pairs(p_data, tb_size[f], 3 * f, false);
      p_data = _next;
      flags_vec.push_back(_flags);
    }
    flags = flags_vec;

    map_idx.clear();
    p_map = p_data;
    const auto &f_vec = std::get<std::vector<int>>(flags);
    for (int f = 0; f < num_files; ++f) {
      map_idx.push_back({});
      if (f_vec[f] & 2) {
        if (!(f_vec[f] & 16)) {
          for (int i = 0; i < 4; ++i) {
            map_idx.back().push_back(p_data + 1 - p_map);
            p_data += 1 + data[p_data];
          }
        } else {
          p_data += (p_data & 0x01);
          for (int i = 0; i < 4; ++i) {
            map_idx.back().push_back((p_data + 2 - p_map) / 2);
            p_data += 2 + 2 * read_uint16(p_data);
          }
        }
      }
    }
    p_data += (p_data & 0x01);

    for (int f = 0; f < num_files; ++f) {
      files[f].precomp.indextable = p_data;
      p_data += size[3 * f];
    }

    for (int f = 0; f < num_files; ++f) {
      files[f].precomp.sizetable = p_data;
      p_data += size[3 * f + 1];
    }

    for (int f = 0; f < num_files; ++f) {
      p_data = (p_data + 0x3f) & ~0x3f;
      files[f].precomp.data = p_data;
      p_data += size[3 * f + 2];
    }
  }

  initialized = true;
}

std::pair<int, int> DtzTable::probe_dtz_table(const chess::Board &board,
                                              int wdl) {
  {
    std::lock_guard<std::recursive_mutex> lock(write_lock);
    read_count++;
  }

  auto res = _probe_dtz_table(board, wdl);

  {
    std::lock_guard<std::recursive_mutex> lock(write_lock);
    read_count--;
    read_condition.notify_all();
  }

  return res;
}

std::pair<int, int> DtzTable::_probe_dtz_table(const chess::Board &board,
                                               int wdl) {
  init_table_dtz();
  assert(data);

  std::string key_str = calc_key(board);

  int cmirror, mirror, bside;
  if (!symmetric) {
    if (key_str != key) {
      cmirror = 8;
      mirror = 0x38;
      bside = (board.sideToMove() == chess::WHITE) ? 1 : 0;
    } else {
      cmirror = mirror = 0;
      bside = (board.sideToMove() != chess::WHITE) ? 1 : 0;
    }
  } else {
    cmirror = (board.sideToMove() == chess::WHITE) ? 0 : 8;
    mirror = (board.sideToMove() == chess::WHITE) ? 0 : 0x38;
    bside = 0;
  }

  int res = 0;
  if (!has_pawns) {
    int f_val = std::get<int>(flags);

    if ((f_val & 1) != bside && !symmetric) {
      return {0, -1};
    }

    const std::vector<PieceType> &pc = pieces;
    std::vector<chess::Square> p(TBPIECES, chess::SQ_NONE);
    int i = 0;
    while (i < num) {
      PieceType piece_type = static_cast<PieceType>(pc[i] & 0x07);
      int color = (pc[i] ^ cmirror) >> 3;
      chess::Bitboard bb =
          board.pieces(piece_type, (color == 0) ? chess::WHITE : chess::BLACK);

      while (bb) {
        p[i] = (chess::Square)pop_lsb(bb);
        i++;
      }
    }

    size_t idx = encode_piece(norm, p, factor);
    res = decompress_pairs(precomp, idx);

    if (f_val & 2) {
      if (!(f_val & 16)) {
        res = data[p_map + map_idx[0][WDL_TO_MAP[wdl + 2]] + res];
      } else {
        res = read_uint16(p_map + 2 * (map_idx[0][WDL_TO_MAP[wdl + 2]] + res));
      }
    }

    if (!(f_val & PA_FLAGS[wdl + 2]) || (wdl & 1)) {
      res *= 2;
    }
  } else {
    const auto &f_vec = std::get<std::vector<int>>(flags);

    int k = files[0].pieces[0] ^ cmirror;
    PieceType piece_type = static_cast<PieceType>(k & 0x07);
    int color = k >> 3;
    chess::Bitboard bb =
        board.pieces(piece_type, (color == 0) ? chess::WHITE : chess::BLACK);

    int i = 0;
    std::vector<Square> p(TBPIECES, SQ_NONE);
    while (bb) {
      p[i] = (Square)(pop_lsb(bb) ^ mirror);
      i++;
    }
    int f = pawn_file(p);
    if ((f_vec[f] & 1) != bside) {
      return {0, -1};
    }

    const std::vector<int> &pc = files[f].pieces;
    while (i < num) {
      piece_type = static_cast<PieceType>(pc[i] & 0x07);
      color = (pc[i] ^ cmirror) >> 3;
      bb = board.pieces(piece_type, (color == 0) ? chess::WHITE : chess::BLACK);

      while (bb) {
        p[i] = (Square)(pop_lsb(bb) ^ mirror);
        i++;
      }
    }

    size_t idx = encode_pawn(files[f].norm, p, files[f].factor);
    res = decompress_pairs(files[f].precomp, idx);

    if (f_vec[f] & 2) {
      if (!(f_vec[f] & 16)) {
        res = data[p_map + map_idx[f][WDL_TO_MAP[wdl + 2]] + res];
      } else {
        res = read_uint16(p_map + 2 * (map_idx[f][WDL_TO_MAP[wdl + 2]] + res));
      }
    }

    if (!(f_vec[f] & PA_FLAGS[wdl + 2]) || (wdl & 1)) {
      res *= 2;
    }
  }

  return {res, 1};
}

void DtzTable::setup_pieces_piece_dtz(size_t p_data, int p_tb_size) {
  assert(data);

  pieces.resize(num);
  for (int i = 0; i < num; ++i)
    pieces[i] = static_cast<PieceType>(data[p_data + i + 1] & 0x0f);
  int order = data[p_data] & 0x0f;
  set_norm_piece(norm, pieces);
  tb_size[p_tb_size] = calc_factors_piece(factor, order, norm);
}

void DtzTable::setup_pieces_pawn_dtz(size_t p_data, int p_tb_size, int f) {
  assert(data);

  int j = 1 + (int)(pawns[1] > 0);
  int order = data[p_data] & 0x0f;
  int order2 = pawns[1] ? (data[p_data + 1] & 0x0f) : 0x0f;
  files[f].pieces.resize(num);
  for (int i = 0; i < num; ++i)
    files[f].pieces[i] = data[p_data + i + j] & 0x0f;

  files[f].norm.assign(num, 0);
  set_norm_pawn(files[f].norm, files[f].pieces);

  files[f].factor.assign(TBPIECES, 0);
  tb_size[p_tb_size] =
      calc_factors_pawn(files[f].factor, order, order2, files[f].norm, f);
}
/**
 * Adds tables from a directory.
 *
 * By default, all available tables with the correct file names
 * (e.g., WDL files like ``KQvKN.rtbw`` and DTZ files like ``KRBvK.rtbz``)
 * are added.
 *
 * The relevant files are lazily opened when the tablebase is actually
 * probed.
 *
 * Returns the number of table files that were found.
 */
int Tablebase::add_directory(const std::string &directory, bool load_wdl,
                             bool load_dtz) {
  std::filesystem::path abs_path = std::filesystem::absolute(directory);
  int total = 0;
  if (std::filesystem::exists(abs_path) &&
      std::filesystem::is_directory(abs_path)) {
    for (const auto &entry : std::filesystem::directory_iterator(abs_path)) {
      total += add_file(entry.path().string(), load_wdl, load_dtz);
    }
  }
  return total;
}

int Tablebase::add_file(const std::string &path, bool load_wdl, bool load_dtz) {
  std::filesystem::path p(path);
  std::string tablename = p.stem().string();
  std::string ext = p.extension().string();

  if (is_tablename(tablename, /*this->variant.one_king*/ true) &&
      std::filesystem::is_regular_file(p)) {
    if (load_wdl) {
      if (ext == /*this->variant.tbw_suffix*/ ".rtbw") {
        return _open_table<WdlTable>(this->wdl, path);
      } else if (tablename.find('P') == std::string::npos &&
                 ext == /*this->variant.pawnless_tbw_suffix*/ ".rtbw") {
        return _open_table<WdlTable>(this->wdl, path);
      }
    }
    if (load_dtz) {
      if (ext == /*this->variant.tbz_suffix*/ ".rtbz") {
        return _open_table<DtzTable>(this->dtz, path);
      } else if (tablename.find('P') == std::string::npos &&
                 ext == /*this->variant.pawnless_tbz_suffix*/ ".rtbz") {
        return _open_table<DtzTable>(this->dtz, path);
      }
    }
  }
  return 0;
}

int Tablebase::probe_wdl_table(chess::Board &board) {
  // Test for variant end.
  /*if (board.is_variant_win()) {
      return 2;
  } else if (board.is_variant_draw()) {
      return 0;
  } else if (board.is_variant_loss()) {
      return -2;
  }*/

  // Test for KvK.
  if (/*this->variant.one_king*/ true && board.pieces(KING) == board.occ()) {
    return 0;
  }

  std::string key = calc_key(board);
  if (this->wdl.find(key) == this->wdl.end()) {
    if (popcount(board.occ()) > TBPIECES) {
      throw std::runtime_error("syzygy tables support up to " +
                               std::to_string(TBPIECES) + " pieces, not " +
                               std::to_string(popcount(board.occ())) + ": " +
                               board.fen());
    }
    throw std::runtime_error("did not find wdl table " + key);
  }

  WdlTable *table = static_cast<WdlTable *>(this->wdl[key]);
  this->_bump_lru(table);

  return table->probe_wdl_table(board);
}

std::pair<int, int> Tablebase::probe_ab(chess::Board &board, int alpha,
                                        int beta, bool threats) {
  // Check preconditions.
  /*if (board.uci_variant != this->variant.uci_variant) {
      throw std::runtime_error("tablebase has been opened for " +
  this->variant.uci_variant + ", probed with: " + board.uci_variant);
  }*/
  if (board.castlingRights() != 0) {
    throw std::runtime_error(
        "syzygy tables do not contain positions with castling rights: " +
        board.fen());
  }

  // Probing resolves captures, so sometimes we can obtain results for
  // positions that have more pieces than the maximum number of supported
  // pieces. We artificially limit this to one additional level, to
  // make sure search remains somewhat bounded.
  if (popcount(board.occ()) > TBPIECES + 1) {
    throw std::runtime_error("syzygy tables support up to " +
                             std::to_string(TBPIECES) + " pieces, not " +
                             std::to_string(popcount(board.occ())) + ": " +
                             board.fen());
  }

  // Special case: Variant with compulsory captures.
  if (/*this->variant.captures_compulsory*/ false) {
    /*if (board.is_variant_win()) {
        return {2, 2};
    } else if (board.is_variant_loss()) {
        return {-2, 2};
    } else if (board.is_variant_draw()) {
        return {0, 2};
    }*/

    return this->sprobe_ab(board, alpha, beta, threats);
  }

  // Generate non-ep captures.
  Movelist legals;
  board.legals<chess::MoveGenType::CAPTURE>(legals);
  for (const auto &move : legals /*board.generate_legal_moves(0xffffffffffffffff, board.occupied_co[!board.turn])*/)
        {
    if (move.typeOf() == chess::MoveType::EN_PASSANT) {
      continue;
    }
    board.doMove(move);
    int v;
    try {
      auto res = this->probe_ab(board, -beta, -alpha);
      v = -res.first;
    } catch (...) {
      board.undoMove();
      throw;
    }
    board.undoMove();

    if (v > alpha) {
      if (v >= beta) {
        return {v, 2};
      }
      alpha = v;
    }
  }

  int v = this->probe_wdl_table(board);

  if (alpha >= v) {
    return {alpha, 1 + static_cast<int>(alpha > 0)};
  } else {
    return {v, 1};
  }
}

std::pair<int, int> Tablebase::sprobe_ab(chess::Board &board, int alpha,
                                         int beta, bool threats) {
  if (popcount(board.occ(~board.sideToMove())) > 1) {
    auto res = this->sprobe_capts(board, alpha, beta);
    int v = res.first;
    bool captures_found = static_cast<bool>(res.second);
    if (captures_found) {
      return {v, 2};
    }
  } else {
    Movelist moves;
    board.legals<MoveGenType::CAPTURE>(moves);
    if (moves.size()) {
      return {-2, 2};
    }
  }

  bool threats_found = false;

  if (threats || popcount(board.occ()) >= 6) {
    Movelist legals;
    board.legals<chess::MoveGenType::ALL &static_cast<MoveGenType>(
        ~(int)chess::MoveGenType::PAWN)>(legals);
    for (const auto &threat : legals) {
      board.doMove(threat);
      int v;
      bool captures_found;
      try {
        auto res = this->sprobe_capts(board, -beta, -alpha);
        v = -res.first;
        captures_found = static_cast<bool>(res.second);
      } catch (...) {
        board.undoMove();
        throw;
      }
      board.undoMove();

      if (captures_found && v > alpha) {
        threats_found = true;
        alpha = v;
        if (alpha >= beta) {
          return {v, 3};
        }
      }
    }
  }

  int v = this->probe_wdl_table(board);
  if (v > alpha) {
    return {v, 1};
  } else {
    return {alpha, threats_found ? 3 : 1};
  }
}

std::pair<int, int> Tablebase::sprobe_capts(chess::Board &board, int alpha,
                                            int beta) {
  bool captures_found = false;
  Movelist captures;
  board.legals<chess::MoveGenType::CAPTURE>(captures);
  for (const auto &move : captures) {
    captures_found = true;

    board.doMove(move);
    int v;
    try {
      auto res = this->sprobe_ab(board, -beta, -alpha);
      v = -res.first;
    } catch (...) {
      board.undoMove();
      throw;
    }
    board.undoMove();

    alpha = std::max(v, alpha);

    if (alpha >= beta) {
      break;
    }
  }

  return {alpha, static_cast<int>(captures_found)};
}

int Tablebase::probe_wdl(chess::Board &board) {
  /**
   * Probes WDL tables for win/draw/loss information under the 50-move rule,
   * assuming the position has been reached directly after a capture or
   * pawn move.
   * ...
   */
  // Probe.
  auto res = this->probe_ab(board, -2, 2);
  int v = res.first;

  // If en passant is not possible, we are done.
  if (!board.ep_square() || /*this->variant.captures_compulsory*/ false) {
    return v;
  }

  // Now handle en passant.
  int v1 = -3;

  // Look at all legal en passant captures.
  Movelist legals;
  board.legals<chess::MoveGenType::PAWN | chess::MoveGenType::CAPTURE>(legals);
  for (const auto &move : legals) {
    if (move.typeOf() != chess::MoveType::EN_PASSANT) {
      continue;
    }
    board.doMove(move);
    int v0;
    try {
      auto res_ep = this->probe_ab(board, -2, 2);
      v0 = -res_ep.first;
    } catch (...) {
      board.undoMove();
      throw;
    }
    board.undoMove();

    if (v0 > v1) {
      v1 = v0;
    }
  }

  if (v1 > -3) {
    if (v1 >= v) {
      v = v1;
    } else if (v == 0) {
      // If there is not at least one legal non-en-passant move we are
      // forced to play the losing en passant capture.
      /*auto legal_moves = board.generate_legal_moves();
      bool all_ep = true;
      for (const auto& m : legal_moves) {
          if (!board.is_en_passant(m)) {
              all_ep = false;
              break;
          }
      }*/
      // Optimized:
      bool all_ep = board.enpassantSq() != SQ_NONE;
      if (all_ep) {
        Movelist legals;
        board.legals(legals);
        for (const auto &m : legals) {
          if (m.type_of() == EN_PASSANT) {
            all_ep = false;
            break;
          }
        }
      }
      if (all_ep) {
        v = v1;
      }
    }
  }

  return v;
}

std::optional<int> Tablebase::get_wdl(chess::Board &board,
                                      std::optional<int> default_val) {
  try {
    return this->probe_wdl(board);
  } catch (const std::runtime_error &) {
    return default_val;
  }
}

std::pair<int, int> Tablebase::probe_dtz_table(chess::Board &board,
                                               int wdl_val) {
  std::string key = calc_key(board);
  if (this->dtz.find(key) == this->dtz.end()) {
    throw std::runtime_error("did not find dtz table " + key);
  }

  DtzTable *table = static_cast<DtzTable *>(this->dtz[key]);
  this->_bump_lru(table);

  return table->probe_dtz_table(board, wdl_val);
}

int Tablebase::probe_dtz_no_ep(chess::Board &board) {
  auto res_ab = this->probe_ab(board, -2, 2, true);
  int wdl_val = res_ab.first;
  int success = res_ab.second;

  if (wdl_val == 0) {
    return 0;
  }

  if (success == 2 || !(board.occ(board.sideToMove()) & ~board.pieces(PAWN))) {
    return dtz_before_zeroing(wdl_val);
  }

  if (wdl_val > 0) {
    // The position is a win or a cursed win by a threat move.
    if (success == 3) {
      return (wdl_val == 2) ? 2 : 102;
    }

    // Generate all legal non-capturing pawn moves.
    Movelist moves;
    board.legals<chess::MoveGenType::PAWN | chess::MoveGenType::QUIET>(moves);
    for (const auto &move : moves) {
      if (board.is_capture(move)) {
        // En passant.
        continue;
      }

      board.doMove(move);
      int v;
      try {
        v = -this->probe_wdl(board);
      } catch (...) {
        board.undoMove();
        throw;
      }
      board.undoMove();

      if (v == wdl_val) {
        return (v == 2) ? 1 : 101;
      }
    }
  }

  auto res_dtz = this->probe_dtz_table(board, wdl_val);
  int dtz_val = res_dtz.first;
  int dtz_success = res_dtz.second;

  if (dtz_success >= 0) {
    return dtz_before_zeroing(wdl_val) + (wdl_val > 0 ? dtz_val : -dtz_val);
  }

  if (wdl_val > 0) {
    int best = 0xffff;
    Movelist moves;
    board.legals<MoveGenType::ALL &static_cast<MoveGenType>(
        ~(int)(chess::MoveGenType::PAWN | chess::MoveGenType::CAPTURE))>(moves);
    for (const auto &move : moves) {
      board.doMove(move);
      try {
        int v = -this->probe_dtz(board);

        if (v == 1 && board.is_checkmate()) {
          best = 1;
        } else if (v > 0 && v + 1 < best) {
          best = v + 1;
        }
      } catch (...) {
        board.undoMove();
        throw;
      }
      board.undoMove();
    }

    return best;
  } else {
    int best = -1;
    Movelist moves;
    board.legals(moves);
    for (const auto &move : moves) {
      board.doMove(move);

      try {
        int v;
        if (board.halfmoveClock() == 0) {
          if (wdl_val == -2) {
            v = -1;
          } else {
            auto res_v = this->probe_ab(board, 1, 2, true);
            v = (res_v.first == 2) ? 0 : -101;
          }
        } else {
          v = -this->probe_dtz(board) - 1;
        }
        if (v < best) {
          best = v;
        }
      } catch (...) {
        board.undoMove();
        throw;
      }
      board.undoMove();
    }

    return best;
  }
}

int Tablebase::probe_dtz(chess::Board &board) {
  /*
  Probes DTZ tables for
  `DTZ50'' information with rounding <https://syzygy-tables.info/metrics#dtz>`_.

  Minmaxing the DTZ50'' values guarantees winning a won position
  (and drawing a drawn position), because it makes progress keeping the
  win in hand.
  However, the lines are not always the most straightforward ways to win.
  Engines like Stockfish calculate themselves, checking with DTZ, but
  only play according to DTZ if they can not manage on their own.

  Returns a positive value if the side to move is winning, ``0`` if the
  position is a draw, and a negative value if the side to move is losing.
  More precisely:

  +-----+------------------+--------------------------------------------+
  | WDL | DTZ              |                                            |
  +=====+==================+============================================+
  |  -2 | -100 <= n <= -1  | Unconditional loss (assuming 50-move       |
  |     |                  | counter is zero), where a zeroing move can |
  |     |                  | be forced in -n plies.                     |
  +-----+------------------+--------------------------------------------+
  |  -1 |         n < -100 | Loss, but draw under the 50-move rule.     |
  |     |                  | A zeroing move can be forced in -n plies   |
  |     |                  | or -n - 100 plies (if a later phase is     |
  |     |                  | responsible for the blessed loss).         |
  +-----+------------------+--------------------------------------------+
  |   0 |         0        | Draw.                                      |
  +-----+------------------+--------------------------------------------+
  |   1 |   100 < n        | Win, but draw under the 50-move rule.      |
  |     |                  | A zeroing move can be forced in n plies or |
  |     |                  | n - 100 plies (if a later phase is         |
  |     |                  | responsible for the cursed win).           |
  +-----+------------------+--------------------------------------------+
  |   2 |    1 <= n <= 100 | Unconditional win (assuming 50-move        |
  |     |                  | counter is zero), where a zeroing move can |
  |     |                  | be forced in n plies.                      |
  +-----+------------------+--------------------------------------------+

  The return value can be off by one: a return value -n can mean a
  losing zeroing move in n + 1 plies and a return value +n can mean a
  winning zeroing move in n + 1 plies.
  This implies some primary tablebase lines may waste up to 1 ply.
  Rounding is never used for endgame phases where it would change the
  game theoretical outcome.

  This means users need to be careful in positions that are nearly drawn
  under the 50-move rule! Carelessly wasting 1 more ply by not following
  the tablebase recommendation, for a total of 2 wasted plies, may
  change the outcome of the game.

  >>> import chess
  >>> import chess.syzygy
  >>>
  >>> with chess.syzygy.open_tablebase("data/syzygy/regular") as tablebase:
  ...     board = chess.Board("8/2K5/4B3/3N4/8/8/4k3/8 b - - 0 1")
  ...     print(tablebase.probe_dtz(board))
  ...
  -53

  C++:
  #include <tbprobe.h>
  auto tablebase = tbprobe::syzygy::open_tablebase("data/syzygy/regular");
  chess::Board board("8/2K5/4B3/3N4/8/8/4k3/8 b - - 0 1");
  int dtz = tablebase.probe_dtz(board); // -53

  Probing is thread-safe when done with different *board* objects and
  if *board* objects are not modified during probing.

  Both DTZ and WDL tables are required in order to probe for DTZ.

  :raises: :exc:`KeyError` (or specifically
      :exc:`chess.syzygy.MissingTableError`) if the position could not
      be found in the tablebase. Use
      :func:`~chess.syzygy.Tablebase.get_dtz()` if you prefer to get
      ``None`` instead of an exception.

      Note that probing corrupted table files is undefined behavior.

  C++: raises std::runtime_error if the position could not be found in the
  tablebase.
   */
  int v = this->probe_dtz_no_ep(board);

  if (!board.ep_square() || /*this->variant.captures_compulsory*/ false) {
    return v;
  }

  int v1 = -3;

  // Generate all en passant moves.
  Movelist moves;
  board.legals(moves);
  for (const auto &move : moves) {
    if (move.typeOf() != chess::MoveType::EN_PASSANT) {
      continue;
    }
    board.doMove(move);
    try {
      auto res = this->probe_ab(board, -2, 2);
      int v0 = -res.first;
      if (v0 > v1) {
        v1 = v0;
      }
    } catch (...) {
      board.undoMove();
      throw;
    }
    board.undoMove();
  }

  if (v1 > -3) {
    v1 = WDL_TO_DTZ[v1 + 2];
    if (v < -100) {
      if (v1 >= 0)
        v = v1;
    } else if (v < 0) {
      if (v1 >= 0 || v1 < -100)
        v = v1;
    } else if (v > 100) {
      if (v1 > 0)
        v = v1;
    } else if (v > 0) {
      if (v1 == 1)
        v = v1;
    } else if (v1 >= 0) {
      v = v1;
    } else {
      Movelist moves;
      board.legals(moves);
      /*bool all_ep = true;
      for (const auto& m : moves) {
          if (m.type_of() != EN_PASSANT) {
              all_ep = false;
              break;
          }
      }*/
      // micro-optimized:
      bool all_ep = board.enpassantSq() != SQ_NONE;
      if (all_ep)
        for (const auto &m : moves) {
          if (m.type_of() != EN_PASSANT) {
            all_ep = false;
            break;
          }
        }
      if (all_ep) {
        v = v1;
      }
    }
  }

  return v;
}

std::optional<int> Tablebase::get_dtz(chess::Board &board,
                                      std::optional<int> default_val) {
  try {
    return this->probe_dtz(board);
  } catch (const std::runtime_error &) {
    return default_val;
  }
}

void Tablebase::_bump_lru(Table *table) {

  if (!max_fds.has_value()) {
    return;
  }

  std::lock_guard<std::mutex> lock(lru_lock);

  auto it = std::find(lru.begin(), lru.end(), table);
  if (it != lru.end()) {
    lru.erase(it);
  }
  lru.push_front(table);

  if (static_cast<int>(lru.size()) > max_fds.value()) {
    Table *last = lru.back();
    lru.pop_back();
    last->close();
  }
}

void Tablebase::close() {
  /** Closes all loaded tables. */
  for (auto &pair : wdl) {
    if (pair.second)
      pair.second->close();
  }
  wdl.clear();

  for (auto &pair : dtz) {
    if (pair.second)
      pair.second->close();
  }
  dtz.clear();

  lru.clear();
}

std::unique_ptr<Tablebase> open_tablebase(const std::string &directory,
                                          bool load_wdl, bool load_dtz,
                                          std::optional<int> max_fds) {
  /**
   * Opens a collection of tables for probing. See
   * :class:`~chess.syzygy.Tablebase`.
   * ...
   */
  std::unique_ptr<Tablebase> tables = std::make_unique<Tablebase>(max_fds);
  tables->add_directory(directory, load_wdl, load_dtz);
  return tables;
}

} // namespace tbprobe::syzygy
