#pragma once
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <types.h>
#include <unordered_map>
#include <utility>
#include <vector>
namespace tbprobe::gaviota {
constexpr chess::Square NOSQUARE = chess::SQ_NONE;
constexpr int NOINDEX = -1;

constexpr int MAX_KKINDEX = 462;
constexpr int MAX_PPINDEX = 576;
constexpr int MAX_PpINDEX = 24 * 48;
constexpr int MAX_AAINDEX = (63 - 62) + (62 / 2 * (127 - 62)) - 1 + 1;
constexpr int MAX_AAAINDEX = 64 * 21 * 31;
constexpr int MAX_PPP48_INDEX = 8648;
constexpr int MAX_PP48_INDEX = 1128;

constexpr int MAX_KXK = MAX_KKINDEX * 64;
constexpr int MAX_kabk = MAX_KKINDEX * 64 * 64;
constexpr int MAX_kakb = MAX_KKINDEX * 64 * 64;
constexpr int MAX_kpk = 24 * 64 * 64;
constexpr int MAX_kakp = 24 * 64 * 64 * 64;
constexpr int MAX_kapk = 24 * 64 * 64 * 64;
constexpr int MAX_kppk = MAX_PPINDEX * 64 * 64;
constexpr int MAX_kpkp = MAX_PpINDEX * 64 * 64;
constexpr int MAX_kaak = MAX_KKINDEX * MAX_AAINDEX;
constexpr int MAX_kabkc = MAX_KKINDEX * 64 * 64 * 64;
constexpr int MAX_kabck = MAX_KKINDEX * 64 * 64 * 64;
constexpr int MAX_kaakb = MAX_KKINDEX * MAX_AAINDEX * 64;
constexpr int MAX_kaabk = MAX_KKINDEX * MAX_AAINDEX * 64;
constexpr int MAX_kabbk = MAX_KKINDEX * MAX_AAINDEX * 64;
constexpr int MAX_kaaak = MAX_KKINDEX * MAX_AAAINDEX;
constexpr int MAX_kapkb = 24 * 64 * 64 * 64 * 64;
constexpr int MAX_kabkp = 24 * 64 * 64 * 64 * 64;
constexpr int MAX_kabpk = 24 * 64 * 64 * 64 * 64;
constexpr int MAX_kppka = MAX_kppk * 64;
constexpr int MAX_kappk = MAX_kppk * 64;
constexpr int MAX_kapkp = MAX_kpkp * 64;
constexpr int MAX_kaapk = 24 * MAX_AAINDEX * 64 * 64;
constexpr int MAX_kaakp = 24 * MAX_AAINDEX * 64 * 64;
constexpr int MAX_kppkp = 24 * MAX_PP48_INDEX * 64 * 64;
constexpr int MAX_kpppk = MAX_PPP48_INDEX * 64 * 64;

constexpr int PLYSHIFT = 3;
constexpr int INFOMASK = 7;

constexpr int WE_FLAG = 1;
constexpr int NS_FLAG = 2;
constexpr int NW_SE_FLAG = 4;

constexpr std::array<chess::Square, 64> ITOSQ = {
    chess::SQ_H7, chess::SQ_G7, chess::SQ_F7, chess::SQ_E7, chess::SQ_H6,
    chess::SQ_G6, chess::SQ_F6, chess::SQ_E6, chess::SQ_H5, chess::SQ_G5,
    chess::SQ_F5, chess::SQ_E5, chess::SQ_H4, chess::SQ_G4, chess::SQ_F4,
    chess::SQ_E4, chess::SQ_H3, chess::SQ_G3, chess::SQ_F3, chess::SQ_E3,
    chess::SQ_H2, chess::SQ_G2, chess::SQ_F2, chess::SQ_E2, chess::SQ_D7,
    chess::SQ_C7, chess::SQ_B7, chess::SQ_A7, chess::SQ_D6, chess::SQ_C6,
    chess::SQ_B6, chess::SQ_A6, chess::SQ_D5, chess::SQ_C5, chess::SQ_B5,
    chess::SQ_A5, chess::SQ_D4, chess::SQ_C4, chess::SQ_B4, chess::SQ_A4,
    chess::SQ_D3, chess::SQ_C3, chess::SQ_B3, chess::SQ_A3, chess::SQ_D2,
    chess::SQ_C2, chess::SQ_B2, chess::SQ_A2,
};

constexpr int ENTRIES_PER_BLOCK = 16 * 1024;

constexpr int EGTB_MAXBLOCKSIZE = 65536;
constexpr int map24_b(int s) {
  s -= 8;
  return ((s & 3) + s) >> 1;
}
constexpr int map88(int x) { return x + (x & 56); }

constexpr int in_queenside(int x) { return (x & (1 << 2)) == 0; }

constexpr int flip_we(int x) { return x ^ 7; }

constexpr int flip_ns(int x) { return x ^ 56; }

constexpr int flip_nw_se(int x) { return ((x & 7) << 3) | (x >> 3); }
constexpr int idx_is_empty(int x) { return x == -1; }

constexpr int flip_type(chess::Square x, chess::Square y) {
  int ret = 0;

  if (chess::file_of(x) > chess::FILE_D) {
    x = (chess::Square)flip_we(x);
    y = (chess::Square)flip_we(y);
    ret |= WE_FLAG;
  }

  if (chess::rank_of(x) > chess::RANK_4) {
    x = (chess::Square)flip_ns(x);
    y = (chess::Square)flip_ns(y);
    ret |= NS_FLAG;
  }

  int rowx = chess::rank_of(x);
  int colx = chess::file_of(x);

  if (rowx > colx) {
    x = (chess::Square)flip_nw_se(x);
    y = (chess::Square)flip_nw_se(y);
    ret |= NW_SE_FLAG;
  }

  int rowy = chess::rank_of(y);
  int coly = chess::file_of(y);
  if (rowx == colx && rowy > coly) {
    x = (chess::Square)flip_nw_se(x);
    y = (chess::Square)flip_nw_se(y);
    ret |= NW_SE_FLAG;
  }

  return ret;
}
constexpr int wsq_to_pidx24(int pawn) {
  int sq = pawn;
  sq = flip_ns(sq);
  // sq -= 8; // Down one row
  // int idx24 = (sq + (sq & 3)) >> 1;
  int idx24 = map24_b(sq);
  return idx24;
}

constexpr int wsq_to_pidx48(int pawn) {
  int sq = pawn;
  sq = flip_ns(sq);
  sq -= 8; // Down one row
  int idx48 = sq;
  return idx48;
}
extern std::array<std::array<int, 64>, 64> FLIPT;
extern std::array<std::array<int, 48>, 48> PP48_IDX;
extern std::array<int, MAX_PP48_INDEX> PP48_SQ_X, PP48_SQ_Y;
extern std::array<std::array<std::array<int, 48>, 48>, 48> PPP48_IDX;
extern std::array<int, MAX_PPP48_INDEX> PPP48_SQ_X, PPP48_SQ_Y, PPP48_SQ_Z;
extern std::array<std::array<int, 64>, 64> AAIDX;
extern std::array<int, MAX_AAINDEX> AABASE;
extern std::array<int, 64> AAA_BASE;
extern std::array<std::array<int, 3>, MAX_AAAINDEX> AAA_XYZ;
std::pair<chess::Square, chess::Square> pp_putanchorfirst(int a, int b);
extern std::array<std::array<int, 48>, 24> PPIDX;
extern std::array<int, MAX_PPINDEX> PP_HI24;
extern std::array<int, MAX_PPINDEX> PP_LO48;
std::pair<chess::Square, chess::Square> norm_kkindex(chess::Square x,
                                                     chess::Square y);
extern std::array<std::array<int, 64>, 64> KKIDX;
extern std::array<int, MAX_KKINDEX> WKSQ, BKSQ;
std::pair<std::vector<int>, std::vector<int>> sortlists(std::vector<int> ws,
                                                        std::vector<int> wp);
struct Request {
  std::string egkey;
  std::vector<chess::Square> white_squares, black_squares;
  std::vector<chess::PieceType> white_types, black_types;
  std::vector<chess::Square> white_piece_squares, black_piece_squares;
  std::vector<chess::PieceType> white_piece_types, black_piece_types;
  bool is_reversed;
  int side, realside;
  Request(const std::vector<chess::Square> &_white_squares,
          const std::vector<chess::PieceType> &_white_types,
          const std::vector<chess::Square> &_black_squares,
          const std::vector<chess::PieceType> &_black_types, int _side)
      : white_squares(_white_squares), black_squares(_black_squares),
        white_types(_white_types), black_types(_black_types),
        white_piece_squares(white_squares), black_piece_squares(black_squares),
        white_piece_types(white_types), black_piece_types(black_types),
        side(_side), realside(_side) {
    this->sortlists(white_squares, white_types);
    this->sortlists(black_squares, black_types);
  }
  void sortlists(std::vector<chess::Square> &ws,
                 std::vector<chess::PieceType> &wp);
};

inline int aaa_getsubi(int x, int y, int z) {
  int bse = AAA_BASE[z];
  int calc_idx = x + (y - 1) * y / 2 + bse;
  return calc_idx;
}
struct EndgameKey {
  int maxindex;
  int slice_n;
  int64_t (*pctoi)(const Request &);

  EndgameKey(int m, int s, int64_t (*p)(const Request &))
      : maxindex(m), slice_n(s), pctoi(p) {}
};
extern std::unordered_map<std::string, EndgameKey> EGKEY;
int64_t kxk_pctoindex(const Request &);
int64_t kpk_pctoindex(const Request &);
int64_t kakb_pctoindex(const Request &);
int64_t kaak_pctoindex(const Request &);
int64_t kabk_pctoindex(const Request &);
int64_t kakp_pctoindex(const Request &);
int64_t kapk_pctoindex(const Request &);
int64_t kppk_pctoindex(const Request &);
int64_t kpkp_pctoindex(const Request &);
int64_t kppkp_pctoindex(const Request &);
int64_t kaakb_pctoindex(const Request &);
int64_t kaaak_pctoindex(const Request &);
int64_t kaabk_pctoindex(const Request &);
int64_t kabbk_pctoindex(const Request &);
int64_t kabck_pctoindex(const Request &);
int64_t kabkc_pctoindex(const Request &);
int64_t kaapk_pctoindex(const Request &);
int64_t kabpk_pctoindex(const Request &);
int64_t kappk_pctoindex(const Request &);
int64_t kapkb_pctoindex(const Request &);
int64_t kppka_pctoindex(const Request &);
int64_t kaakp_pctoindex(const Request &);
int64_t kabkp_pctoindex(const Request &);
int64_t kapkp_pctoindex(const Request &);
int64_t kpppk_pctoindex(const Request &);

std::vector<int> egtb_block_unpack(int side, int n,
                                   const std::vector<uint8_t> &bp);
inline std::pair<int, int> split_index(int i) {
  // python modulo AND DIVISION behaviors:(
  int q = i / ENTRIES_PER_BLOCK;
  int r = i % ENTRIES_PER_BLOCK;

  if ((i ^ ENTRIES_PER_BLOCK) < 0 && r != 0) {
    q -= 1;
    r += ENTRIES_PER_BLOCK;
  }

  return {q, r};
}

inline int opp(int side) { return side == 0 ? 1 : 0; }

inline std::pair<int, int> unpackdist(int d) {
  return {d >> PLYSHIFT, d & INFOMASK};
}

constexpr int tb_DRAW = 0;
constexpr int tb_WMATE = 1;
constexpr int tb_BMATE = 2;
constexpr int tb_FORBID = 3;
constexpr int tb_UNKNOWN = 7;
constexpr int iDRAW = tb_DRAW;
constexpr int iWMATE = tb_WMATE;
constexpr int iBMATE = tb_BMATE;
constexpr int iFORBID = tb_FORBID;

constexpr int iDRAWt = tb_DRAW | 4;
constexpr int iWMATEt = tb_WMATE | 4;
constexpr int iBMATEt = tb_BMATE | 4;
int dtm_unpack(int stm, int packed);
class TableBlock {
public:
  std::string egkey;
  int side;
  int offset;
  int age;
  std::vector<int> pcache;

  inline TableBlock(std::string egk, int s, int off, int a)
      : egkey(egk), side(s), offset(off), age(a) {}
};

struct ZipInfo {
  int extraoffset;
  int totalblocks;
  std::vector<uint32_t> blockindex;
};
class PythonTablebase {
private:
  struct TupleHash {
    template <typename T>
    static void hash_combine(std::size_t &seed, const T &v) {
      seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    std::size_t operator()(const std::tuple<std::string, int, int> &t) const {
      std::size_t seed = 0;

      hash_combine(seed, std::get<0>(t));
      hash_combine(seed, std::get<1>(t));
      hash_combine(seed, std::get<2>(t));

      return seed;
    }
  };

public:
  std::unordered_map<std::string, std::string> available_tables;
  std::unordered_map<std::string, std::unique_ptr<std::ifstream>> streams;
  std::unordered_map<std::string, ZipInfo> zipinfo;
  std::unordered_map<std::tuple<std::string, int, int>, TableBlock, TupleHash>
      block_cache;
  int block_age;
  void close();
  PythonTablebase() : block_age(0){};

  PythonTablebase(const PythonTablebase &) = delete;
  PythonTablebase &operator=(const PythonTablebase &) = delete;

  PythonTablebase(PythonTablebase &&) = default;
  PythonTablebase &operator=(PythonTablebase &&) = default;
  inline ~PythonTablebase() { close(); }
  void add_directory(std::string directory);
  int probe_dtm(chess::Board &board);
  int _probe_dtm_no_ep(chess::Board &board);
  std::optional<int> get_dtm(chess::Board &board,
                             std::optional<int> default_val = std::nullopt);
  int probe_wdl(chess::Board &board);
  std::optional<int> get_wdl(chess::Board &board,
                             std::optional<int> default_val = std::nullopt);
  std::unique_ptr<std::ifstream> &_setup_tablebase(Request &req);
  std::unique_ptr<std::ifstream> &_open_tablebase(Request &req);
  int egtb_block_getnumber(Request &req, int idx);
  int egtb_block_getsize(Request &req, int idx);
  int _tb_probe(Request &req);
  ZipInfo &egtb_loadindexes(std::string egkey,
                            std::unique_ptr<std::ifstream> &stream);
  int egtb_block_getsize_zipped(std::string egkey, int block);
  int egtb_block_park(std::string egkey, int block,
                      std::unique_ptr<std::ifstream> &stream);
};
PythonTablebase open_tablebase(std::string directory);
} // namespace tbprobe::gaviota
