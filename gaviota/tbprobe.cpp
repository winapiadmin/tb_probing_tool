#include "tbprobe.h"
#include <algorithm>
#include <attacks.h>
#include <filesystem>
#include <fstream>
#include <lzma.h>
#include <iostream>
#include <position.h>
#include <printers.h>
#include <stdexcept>
#include <system_error>
namespace tbprobe::gaviota {
std::array<std::array<int, 64>, 64> FLIPT = [] {
  std::array<std::array<int, 64>, 64> ret{};
  for (chess::Square j = chess::SQ_A1; j < chess::SQ_NONE; j++) {
    for (chess::Square i = chess::SQ_A1; i < chess::SQ_NONE; i++) {
      ret[j][i] = flip_type(j, i);
    }
  }
  return ret;
}();

static auto PP_ARRAYS_HOLDER = [] {
  int idx = 0;
  std::array<std::array<int, 48>, 48> pp48_idx{};
  std::array<int, MAX_PP48_INDEX> pp48_sq_x{}, pp48_sq_y{};
  for (int x = 0; x < 48; x++) {
    pp48_idx[x].fill(-1);
  }
  pp48_sq_x.fill(NOSQUARE);
  pp48_sq_y.fill(NOSQUARE);
  for (chess::Square a = chess::SQ_H7; a >= chess::SQ_A2; a--) {
    for (chess::Square b = a + chess::WEST; b >= chess::SQ_A2; b--) {
      int i = flip_we(flip_ns((int)a)) - 8;
      int j = flip_we(flip_ns((int)b)) - 8;

      if (idx_is_empty(pp48_idx[i][j])) {
        pp48_idx[i][j] = idx;
        pp48_idx[j][i] = idx;
        pp48_sq_x[idx] = i;
        pp48_sq_y[idx] = j;
        idx++;
      }
    }
  }
  return std::make_tuple(pp48_idx, pp48_sq_x, pp48_sq_y);
}();
auto &[pp48_idx, pp48_sq_x, pp48_sq_y] = PP_ARRAYS_HOLDER;

std::array<std::array<int, 48>, 48> PP48_IDX = pp48_idx;
std::array<int, MAX_PP48_INDEX> PP48_SQ_X = pp48_sq_x;
std::array<int, MAX_PP48_INDEX> PP48_SQ_Y = pp48_sq_y;
static auto PPP_ARRAYS_HOLDER = [] {
  int idx = 0;
  std::array<std::array<std::array<int, 48>, 48>, 48> ppp48_idx{};
  std::array<int, MAX_PPP48_INDEX> ppp48_sq_x{}, ppp48_sq_y{}, ppp48_sq_z{};
  for (int x = 0; x < 48; x++) {
    for (int y = 0; y < 48; y++) {
      for (int z = 0; z < 48; z++) {
        ppp48_idx[x][y][z] = -1;
      }
    }
  }
  for (int x = 0; x < MAX_PPP48_INDEX; x++) {
    ppp48_sq_x[x] = NOSQUARE;
    ppp48_sq_y[x] = NOSQUARE;
    ppp48_sq_z[x] = NOSQUARE;
  }
  for (int x = 0; x < 48; x++) {
    for (int y = x + 1; y < 48; y++) {
      for (int z = y + 1; z < 48; z++) {
        chess::Square a = ITOSQ[x], b = ITOSQ[y], c = ITOSQ[z];
        if (!in_queenside(b) || !in_queenside(c)) {
          continue;
        }
        int i = a - 8, j = b - 8, k = c - 8;
        if (idx_is_empty(ppp48_idx[i][j][k])) {
          ppp48_idx[i][j][k] = idx;
          ppp48_idx[i][k][j] = idx;
          ppp48_idx[j][i][k] = idx;
          ppp48_idx[j][k][i] = idx;
          ppp48_idx[k][i][j] = idx;
          ppp48_idx[k][j][i] = idx;
          ppp48_sq_x[idx] = i;
          ppp48_sq_y[idx] = j;
          ppp48_sq_z[idx] = k;
          idx++;
        }
      }
    }
  }
  return std::make_tuple(ppp48_idx, ppp48_sq_x, ppp48_sq_y, ppp48_sq_z);
}();

const auto &[ppp48_idx, ppp48_sq_x, ppp48_sq_y, ppp48_sq_z] = PPP_ARRAYS_HOLDER;
std::array<std::array<std::array<int, 48>, 48>, 48> PPP48_IDX = ppp48_idx;
std::array<int, MAX_PPP48_INDEX> PPP48_SQ_X = ppp48_sq_x;
std::array<int, MAX_PPP48_INDEX> PPP48_SQ_Y = ppp48_sq_y;
std::array<int, MAX_PPP48_INDEX> PPP48_SQ_Z = ppp48_sq_z;

static auto AA_ARRAYS_HOLDER = [] {
  std::array<std::array<int, 64>, 64> aaidx{};
  std::array<int, MAX_AAINDEX> aabase{};
  for (int i = 0; i < 64; i++)
    aaidx[i].fill(-1);
  aabase.fill(0);
  int idx = 0;
  for (int x = 0; x < 64; x++)
    for (int y = x + 1; y < 64; y++) {
      if (idx_is_empty(aaidx[x][y])) {
        aaidx[x][y] = idx;
        aaidx[y][x] = idx;
        aabase[idx] = x;
        idx++;
      }
    }
  return std::make_tuple(aabase, aaidx);
}();
const auto &[aabase, aaidx] = AA_ARRAYS_HOLDER;
std::array<std::array<int, 64>, 64> AAIDX = aaidx;
std::array<int, MAX_AAINDEX> AABASE = aabase;

static auto AAA_ARRAYS_HOLDER = [] {
  std::array<std::array<int, 3>, MAX_AAAINDEX> aaa_xyz{};
  std::array<int, 64> aaa_base{};
  for (int i = 0; i < 64; i++) {
    aaa_base[i] = i * (i - 1) * (i - 2) / 6;
  }
  for (int i = 0; i < MAX_AAAINDEX; i++) {
    aaa_xyz[i].fill(-1);
  }
  int idx = 0;
  for (int z = 0; z < 64; z++)
    for (int y = 0; y < z; y++)
      for (int x = 0; x < y; x++) {
        aaa_xyz[idx][0] = x;
        aaa_xyz[idx][1] = y;
        aaa_xyz[idx][2] = z;
        idx++;
      }
  return std::make_tuple(aaa_base, aaa_xyz);
}();
const auto &[aaa_base, aaa_xyz] = AAA_ARRAYS_HOLDER;
std::array<int, 64> AAA_BASE = aaa_base;
std::array<std::array<int, 3>, MAX_AAAINDEX> AAA_XYZ = aaa_xyz;
std::pair<chess::Square, chess::Square> pp_putanchorfirst(int a, int b) {
  int row_b = b & 56;
  int row_a = a & 56;

  // Default.
  int anchor = a;
  int loosen = b;

  if (row_b > row_a) {
    anchor = b;
    loosen = a;
  } else if (row_b == row_a) {
    int x = a;
    int col = x & 7;
    int inv = col ^ 7;
    int val_a = (1 << col) | (1 << inv);
    val_a &= (val_a - 1);
    int hi_a = val_a;

    x = b;
    col = x & 7;
    inv = col ^ 7;
    int val_b = (1 << col) | (1 << inv);
    val_b &= (val_b - 1);
    int hi_b = val_b;

    if (hi_b > hi_a) {
      anchor = b;
      loosen = a;
    } else if (hi_b < hi_a) {
      anchor = a;
      loosen = b;
    } else {
      if (a < b) {
        anchor = a;
        loosen = b;
      } else {
        anchor = b;
        loosen = a;
      }
    }
  }
  return std::make_pair((chess::Square)anchor, (chess::Square)loosen);
}

auto PPIDX_HOLDER = [] {
  std::array<std::array<int, 48>, 24> ppidx{};
  std::array<int, MAX_PPINDEX> pp_hi24{}, pp_lo48{};
  for (int i = 0; i < 24; ++i)
    ppidx[i].fill(-1);
  for (int i = 0; i < MAX_PPINDEX; ++i)
    pp_hi24[i] = pp_lo48[i] = -1;

  int idx = 0;
  for (int a = chess::SQ_H7; a >= chess::SQ_A2; --a) {
    if (in_queenside(a))
      continue;

    for (int b = a - 1; b >= chess::SQ_A2; --b) {
      std::pair<chess::Square, chess::Square> res = pp_putanchorfirst(a, b);
      chess::Square anchor = res.first;
      chess::Square loosen = res.second;

      if ((anchor & 7) > 3) {
        // Square on the kingside.
        anchor = (chess::Square)flip_we(anchor);
        loosen = (chess::Square)flip_we(loosen);
      }

      int i = wsq_to_pidx24(anchor);
      int j = wsq_to_pidx48(loosen);

      if (idx_is_empty(ppidx[i][j])) {
        ppidx[i][j] = idx;
        pp_hi24[idx] = i;
        pp_lo48[idx] = j;
        idx += 1;
      }
    }
  }
  return std::make_tuple(ppidx, pp_hi24, pp_lo48);
}();
const auto &[ppidx, pp_hi24, pp_lo48] = PPIDX_HOLDER;
std::array<std::array<int, 48>, 24> PPIDX = ppidx;
std::array<int, MAX_PPINDEX> PP_HI24 = pp_hi24;
std::array<int, MAX_PPINDEX> PP_LO48 = pp_lo48;

std::pair<chess::Square, chess::Square> norm_kkindex(chess::Square x,
                                                     chess::Square y) {
  if (chess::file_of(x) > chess::FILE_D) {
    x = (chess::Square)flip_we(x);
    y = (chess::Square)flip_we(y);
  }

  if (chess::rank_of(x) > chess::RANK_4) {
    x = (chess::Square)flip_ns(x);
    y = (chess::Square)flip_ns(y);
  }

  int rowx = chess::rank_of(x);
  int colx = chess::file_of(x);

  if (rowx > colx) {
    x = (chess::Square)flip_nw_se(x);
    y = (chess::Square)flip_nw_se(y);
  }

  int rowy = chess::rank_of(y);
  int coly = chess::file_of(y);

  if (rowx == colx && rowy > coly) {
    x = (chess::Square)flip_nw_se(x);
    y = (chess::Square)flip_nw_se(y);
  }

  return std::make_pair(x, y);
}
auto KKIDX_HOLDER = [] {
  std::array<std::array<int, 64>, 64> kkidx{};
  std::array<int, MAX_KKINDEX> bksq{}, wksq{};
  for (int i = 0; i < 64; i++)
    kkidx[i].fill(-1);
  bksq.fill(-1);
  wksq.fill(-1);
  int idx = 0;
  for (chess::Square x = chess::SQ_A1; x < chess::SQ_NONE; x++) {
    for (chess::Square y = chess::SQ_A1; y < chess::SQ_NONE; y++) {
      if (x == y || (chess::attacks::KingAttacks[x] & (1ULL << y)))
        continue;
      auto [i, j] = norm_kkindex(x, y);

      if (idx_is_empty(kkidx[i][j])) {
        kkidx[i][j] = idx;
        kkidx[x][y] = idx;
        bksq[idx] = i;
        wksq[idx] = j;
        idx++;
      }
    }
  }
  return std::make_tuple(kkidx, wksq, bksq);
}();
const auto &[kkidx, wksq, bksq] = KKIDX_HOLDER;
std::array<std::array<int, 64>, 64> KKIDX = kkidx;
std::array<int, MAX_KKINDEX> WKSQ = wksq;
std::array<int, MAX_KKINDEX> BKSQ = bksq;
std::pair<std::vector<int>, std::vector<int>> sortlists(std::vector<int> ws,
                                                        std::vector<int> wp) {
  assert(ws.size() >= wp.size());
  std::vector<std::pair<int, int>> combined;
  for (size_t i = 0; i < wp.size(); ++i) {
    combined.push_back({wp[i], ws[i]});
  }
  std::stable_sort(
      combined.begin(), combined.end(),
      [](const auto &a, const auto &b) { return a.first > b.first; });
  std::vector<int> ws2, wp2;
  for (auto const &p : combined) {
    wp2.push_back(p.first);
    ws2.push_back(p.second);
  }
  return {ws2, wp2};
}

void Request::sortlists(std::vector<chess::Square> &ws,
                        std::vector<chess::PieceType> &wp) {
  std::vector<std::pair<chess::PieceType, chess::Square>> combined;
  for (size_t i = 0; i < wp.size(); ++i) {
    combined.push_back({wp[i], ws[i]});
  }
  std::stable_sort(
      combined.begin(), combined.end(),
      [](const auto &a, const auto &b) { return a.first > b.first; });
  for (size_t i = 0; i < combined.size(); ++i) {
    wp[i] = combined[i].first;
    ws[i] = combined[i].second;
  }
}
int64_t kxk_pctoindex(const Request &c) {
  const int BLOCK_Ax = 64;
  int ft = flip_type(c.black_piece_squares[0], c.white_piece_squares[0]);

  std::vector<chess::Square> ws = c.white_piece_squares;
  std::vector<chess::Square> bs = c.black_piece_squares;

  if ((ft & 1) != 0) {
    for (auto &s : ws)
      s = (chess::Square)flip_we(s);
    for (auto &s : bs)
      s = (chess::Square)flip_we(s);
  }
  if ((ft & 2) != 0) {
    for (auto &s : ws)
      s = (chess::Square)flip_ns(s);
    for (auto &s : bs)
      s = (chess::Square)flip_ns(s);
  }
  if ((ft & 4) != 0) {
    for (auto &s : ws)
      s = (chess::Square)flip_nw_se(s);
    for (auto &s : bs)
      s = (chess::Square)flip_nw_se(s);
  }

  int ki = KKIDX[bs[0]][ws[0]];
  if (ki == -1)
    return NOINDEX;
  return ki * BLOCK_Ax + ws[1];
}

int64_t kapkb_pctoindex(const Request &c) {
  const long long BLOCK_A = 64LL * 64LL * 64LL * 64LL;
  const long long BLOCK_B = 64LL * 64LL * 64LL;
  const long long BLOCK_C = 64LL * 64LL;
  const long long BLOCK_D = 64LL;

  chess::Square pawn = c.white_piece_squares[2];
  chess::Square wa = c.white_piece_squares[1];
  chess::Square wk = c.white_piece_squares[0];
  chess::Square bk = c.black_piece_squares[0];
  chess::Square ba = c.black_piece_squares[1];

  if (!(chess::SQ_A2 <= pawn && pawn < chess::SQ_A8))
    return NOINDEX;

  if ((pawn & 7) > 3) {
    pawn = (chess::Square)flip_we(pawn);
    wk = (chess::Square)flip_we(wk);
    bk = (chess::Square)flip_we(bk);
    wa = (chess::Square)flip_we(wa);
    ba = (chess::Square)flip_we(ba);
  }

  /*int sq = pawn;
  sq ^= 56; // flip_ns
  sq -= 8; // Down one row
  int pslice = (sq + (sq & 3)) >> 1;*/
  int pslice = wsq_to_pidx24(pawn);
  return pslice * BLOCK_A + wk * BLOCK_B + bk * BLOCK_C + wa * BLOCK_D + ba;
}

int64_t kabpk_pctoindex(const Request &c) {
  const long long BLOCK_A = 64LL * 64LL * 64LL * 64LL;
  const long long BLOCK_B = 64LL * 64LL * 64LL;
  const long long BLOCK_C = 64LL * 64LL;
  const long long BLOCK_D = 64LL;

  chess::Square wk = c.white_piece_squares[0];
  chess::Square wa = c.white_piece_squares[1];
  chess::Square wb = c.white_piece_squares[2];
  chess::Square pawn = c.white_piece_squares[3];
  chess::Square bk = c.black_piece_squares[0];

  if ((pawn & 7) > 3) {
    pawn = (chess::Square)flip_we(pawn);
    wk = (chess::Square)flip_we(wk);
    bk = (chess::Square)flip_we(bk);
    wa = (chess::Square)flip_we(wa);
    wb = (chess::Square)flip_we(wb);
  }

  int pslice = wsq_to_pidx24(pawn);
  return pslice * BLOCK_A + wk * BLOCK_B + bk * BLOCK_C + wa * BLOCK_D + wb;
}

int64_t kabkp_pctoindex(const Request &c) {
  const long long BLOCK_A = 64LL * 64LL * 64LL * 64LL;
  const long long BLOCK_B = 64LL * 64LL * 64LL;
  const long long BLOCK_C = 64LL * 64LL;
  const long long BLOCK_D = 64LL;

  chess::Square pawn = c.black_piece_squares[1];
  chess::Square wa = c.white_piece_squares[1];
  chess::Square wk = c.white_piece_squares[0];
  chess::Square bk = c.black_piece_squares[0];
  chess::Square wb = c.white_piece_squares[2];

  if (!(chess::SQ_A2 <= pawn && pawn < chess::SQ_A8))
    return NOINDEX;

  if ((pawn & 7) > 3) {
    pawn = (chess::Square)flip_we(pawn);
    wk = (chess::Square)flip_we(wk);
    bk = (chess::Square)flip_we(bk);
    wa = (chess::Square)flip_we(wa);
    wb = (chess::Square)flip_we(wb);
  }

  /*chess::Square sq = pawn - chess::NORTH;
  int pslice = (sq + (sq & 3)) >> 1;*/
  int pslice = wsq_to_pidx24(flip_ns(pawn));
  return pslice * BLOCK_A + wk * BLOCK_B + bk * BLOCK_C + wa * BLOCK_D + wb;
}

int64_t kaapk_pctoindex(const Request &c) {
  const int BLOCK_C = MAX_AAINDEX;
  const int BLOCK_B = 64 * BLOCK_C;
  const long long BLOCK_A = 64LL * BLOCK_B;

  chess::Square wk = c.white_piece_squares[0];
  chess::Square wa = c.white_piece_squares[1];
  chess::Square wa2 = c.white_piece_squares[2];
  chess::Square pawn = c.white_piece_squares[3];
  chess::Square bk = c.black_piece_squares[0];

  if ((pawn & 7) > 3) {
    pawn = (chess::Square)flip_we(pawn);
    wk = (chess::Square)flip_we(wk);
    bk = (chess::Square)flip_we(bk);
    wa = (chess::Square)flip_we(wa);
    wa2 = (chess::Square)flip_we(wa2);
  }

  int pslice = wsq_to_pidx24(pawn);
  int aa_combo = AAIDX[wa][wa2];
  if (idx_is_empty(aa_combo))
    return NOINDEX;

  return pslice * BLOCK_A + wk * BLOCK_B + bk * BLOCK_C + aa_combo;
}

int64_t kaakp_pctoindex(const Request &c) {
  const int BLOCK_C = MAX_AAINDEX;
  const int BLOCK_B = 64 * BLOCK_C;
  const long long BLOCK_A = 64LL * BLOCK_B;

  chess::Square wk = c.white_piece_squares[0];
  chess::Square wa = c.white_piece_squares[1];
  chess::Square wa2 = c.white_piece_squares[2];
  chess::Square bk = c.black_piece_squares[0];
  chess::Square pawn = c.black_piece_squares[1];

  if ((pawn & 7) > 3) {
    pawn = (chess::Square)flip_we(pawn);
    wk = (chess::Square)flip_we(wk);
    bk = (chess::Square)flip_we(bk);
    wa = (chess::Square)flip_we(wa);
    wa2 = (chess::Square)flip_we(wa2);
  }

  pawn = (chess::Square)flip_ns(pawn);
  int pslice = wsq_to_pidx24(pawn);
  int aa_combo = AAIDX[wa][wa2];
  if (idx_is_empty(aa_combo))
    return NOINDEX;

  return pslice * BLOCK_A + wk * BLOCK_B + bk * BLOCK_C + aa_combo;
}

int64_t kapkp_pctoindex(const Request &c) {
  const int BLOCK_A = 64 * 64 * 64;
  const int BLOCK_B = 64 * 64;
  const int BLOCK_C = 64;

  chess::Square wk = c.white_piece_squares[0];
  chess::Square wa = c.white_piece_squares[1];
  chess::Square pawn_a = c.white_piece_squares[2];
  chess::Square bk = c.black_piece_squares[0];
  chess::Square pawn_b = c.black_piece_squares[1];

  chess::Square anchor = pawn_a;
  chess::Square loosen = pawn_b;

  if ((anchor & 7) > 3) {
    anchor = (chess::Square)flip_we(anchor);
    loosen = (chess::Square)flip_we(loosen);
    wk = (chess::Square)flip_we(wk);
    bk = (chess::Square)flip_we(bk);
    wa = (chess::Square)flip_we(wa);
  }

  int m = wsq_to_pidx24(anchor);
  int n = loosen - 8;
  int pp_slice = m * 48 + n;

  if (idx_is_empty(pp_slice))
    return NOINDEX;

  return pp_slice * BLOCK_A + wk * BLOCK_B + bk * BLOCK_C + wa;
}

int64_t kappk_pctoindex(const Request &c) {
  const int BLOCK_A = 64 * 64 * 64;
  const int BLOCK_B = 64 * 64;
  const int BLOCK_C = 64;

  chess::Square wk = c.white_piece_squares[0];
  chess::Square wa = c.white_piece_squares[1];
  chess::Square pawn_a = c.white_piece_squares[2];
  chess::Square pawn_b = c.white_piece_squares[3];
  chess::Square bk = c.black_piece_squares[0];

  std::pair<chess::Square, chess::Square> res =
      pp_putanchorfirst(pawn_a, pawn_b);
  chess::Square anchor = res.first;
  chess::Square loosen = res.second;

  if ((anchor & 7) > 3) {
    anchor = (chess::Square)flip_we(anchor);
    loosen = (chess::Square)flip_we(loosen);
    wk = (chess::Square)flip_we(wk);
    bk = (chess::Square)flip_we(bk);
    wa = (chess::Square)flip_we(wa);
  }

  int i = wsq_to_pidx24(anchor);
  int j = wsq_to_pidx48(loosen);
  int pp_slice = PPIDX[i][j];

  if (idx_is_empty(pp_slice))
    return NOINDEX;

  return pp_slice * BLOCK_A + wk * BLOCK_B + bk * BLOCK_C + wa;
}

int64_t kppka_pctoindex(const Request &c) {
  const int BLOCK_A = 64 * 64 * 64;
  const int BLOCK_B = 64 * 64;
  const int BLOCK_C = 64;

  chess::Square wk = c.white_piece_squares[0];
  chess::Square pawn_a = c.white_piece_squares[1];
  chess::Square pawn_b = c.white_piece_squares[2];
  chess::Square bk = c.black_piece_squares[0];
  chess::Square ba = c.black_piece_squares[1];

  std::pair<chess::Square, chess::Square> res =
      pp_putanchorfirst(pawn_a, pawn_b);
  chess::Square anchor = res.first;
  chess::Square loosen = res.second;

  if ((anchor & 7) > 3) {
    anchor = (chess::Square)flip_we(anchor);
    loosen = (chess::Square)flip_we(loosen);
    wk = (chess::Square)flip_we(wk);
    bk = (chess::Square)flip_we(bk);
    ba = (chess::Square)flip_we(ba);
  }

  int i = wsq_to_pidx24(anchor);
  int j = wsq_to_pidx48(loosen);
  int pp_slice = PPIDX[i][j];

  if (idx_is_empty(pp_slice))
    return NOINDEX;

  return pp_slice * BLOCK_A + wk * BLOCK_B + bk * BLOCK_C + ba;
}

int64_t kabck_pctoindex(const Request &c) {
  const int N_WHITE = 4;
  const int BLOCK_A = 64 * 64 * 64;
  const int BLOCK_B = 64 * 64;
  const int BLOCK_C = 64;

  int ft = FLIPT[c.black_piece_squares[0]][c.white_piece_squares[0]];

  std::vector<chess::Square> ws = c.white_piece_squares;
  std::vector<chess::Square> bs = c.black_piece_squares;

  if ((ft & WE_FLAG) != 0) {
    for (int i = 0; i < N_WHITE; ++i)
      ws[i] = (chess::Square)flip_we(ws[i]);
    bs[0] = (chess::Square)flip_we(bs[0]);
  }
  if ((ft & NS_FLAG) != 0) {
    for (int i = 0; i < N_WHITE; ++i)
      ws[i] = (chess::Square)flip_ns(ws[i]);
    bs[0] = (chess::Square)flip_ns(bs[0]);
  }
  if ((ft & NW_SE_FLAG) != 0) {
    for (int i = 0; i < N_WHITE; ++i)
      ws[i] = (chess::Square)flip_nw_se(ws[i]);
    bs[0] = (chess::Square)flip_nw_se(bs[0]);
  }

  int ki = KKIDX[bs[0]][ws[0]];
  if (idx_is_empty(ki))
    return NOINDEX;

  return ki * BLOCK_A + ws[1] * BLOCK_B + ws[2] * BLOCK_C + ws[3];
}

int64_t kabbk_pctoindex(const Request &c) {
  const int N_WHITE = 4;
  const int BLOCK_Bx = 64;
  const int BLOCK_Ax = BLOCK_Bx * MAX_AAINDEX;

  int ft = FLIPT[c.black_piece_squares[0]][c.white_piece_squares[0]];

  std::vector<chess::Square> ws = c.white_piece_squares;
  std::vector<chess::Square> bs = c.black_piece_squares;

  if ((ft & WE_FLAG) != 0) {
    for (int i = 0; i < N_WHITE; ++i)
      ws[i] = (chess::Square)flip_we(ws[i]);
    bs[0] = (chess::Square)flip_we(bs[0]);
  }
  if ((ft & NS_FLAG) != 0) {
    for (int i = 0; i < N_WHITE; ++i)
      ws[i] = (chess::Square)flip_ns(ws[i]);
    bs[0] = (chess::Square)flip_ns(bs[0]);
  }
  if ((ft & NW_SE_FLAG) != 0) {
    for (int i = 0; i < N_WHITE; ++i)
      ws[i] = (chess::Square)flip_nw_se(ws[i]);
    bs[0] = (chess::Square)flip_nw_se(bs[0]);
  }

  int ki = KKIDX[bs[0]][ws[0]];
  int ai = AAIDX[ws[2]][ws[3]];

  if (idx_is_empty(ki) || idx_is_empty(ai))
    return NOINDEX;

  return ki * BLOCK_Ax + ai * BLOCK_Bx + ws[1];
}

int64_t kaabk_pctoindex(const Request &c) {
  const int N_WHITE = 4;
  const int BLOCK_Bx = 64;
  const int BLOCK_Ax = BLOCK_Bx * MAX_AAINDEX;

  int ft = FLIPT[c.black_piece_squares[0]][c.white_piece_squares[0]];

  std::vector<chess::Square> ws = c.white_piece_squares;
  std::vector<chess::Square> bs = c.black_piece_squares;

  if ((ft & WE_FLAG) != 0) {
    for (int i = 0; i < N_WHITE; ++i)
      ws[i] = (chess::Square)flip_we(ws[i]);
    bs[0] = (chess::Square)flip_we(bs[0]);
  }
  if ((ft & NS_FLAG) != 0) {
    for (int i = 0; i < N_WHITE; ++i)
      ws[i] = (chess::Square)flip_ns(ws[i]);
    bs[0] = (chess::Square)flip_ns(bs[0]);
  }
  if ((ft & NW_SE_FLAG) != 0) {
    for (int i = 0; i < N_WHITE; ++i)
      ws[i] = (chess::Square)flip_nw_se(ws[i]);
    bs[0] = (chess::Square)flip_nw_se(bs[0]);
  }

  int ki = KKIDX[bs[0]][ws[0]];
  int ai = AAIDX[ws[1]][ws[2]];

  if (idx_is_empty(ki) || idx_is_empty(ai))
    return NOINDEX;

  return ki * BLOCK_Ax + ai * BLOCK_Bx + ws[3];
}
int64_t kaaak_pctoindex(const Request &c) {
  const int N_WHITE = 4;
  const int BLOCK_Ax = MAX_AAAINDEX;

  std::vector<chess::Square> ws = c.white_piece_squares;
  std::vector<chess::Square> bs = c.black_piece_squares;

  int ft = FLIPT[bs[0]][ws[0]];

  if ((ft & WE_FLAG) != 0) {
    for (int i = 0; i < N_WHITE; ++i)
      ws[i] = (chess::Square)flip_we(ws[i]);
    bs[0] = (chess::Square)flip_we(bs[0]);
  }
  if ((ft & NS_FLAG) != 0) {
    for (int i = 0; i < N_WHITE; ++i)
      ws[i] = (chess::Square)flip_ns(ws[i]);
    bs[0] = (chess::Square)flip_ns(bs[0]);
  }
  if ((ft & NW_SE_FLAG) != 0) {
    for (int i = 0; i < N_WHITE; ++i)
      ws[i] = (chess::Square)flip_nw_se(ws[i]);
    bs[0] = (chess::Square)flip_nw_se(bs[0]);
  }

  if (ws[2] < ws[1])
    std::swap(ws[1], ws[2]);
  if (ws[3] < ws[2])
    std::swap(ws[2], ws[3]);
  if (ws[2] < ws[1])
    std::swap(ws[1], ws[2]);

  int ki = KKIDX[bs[0]][ws[0]];
  if (ws[1] == ws[2] || ws[1] == ws[3] || ws[2] == ws[3])
    return NOINDEX;

  int ai = aaa_getsubi(ws[1], ws[2], ws[3]);
  if (idx_is_empty(ki) || idx_is_empty(ai))
    return NOINDEX;

  return ki * BLOCK_Ax + ai;
}

int64_t kppkp_pctoindex(const Request &c) {
  const long long BLOCK_Ax = (long long)MAX_PP48_INDEX * 64LL * 64LL;
  const int BLOCK_Bx = 64 * 64;
  const int BLOCK_Cx = 64;

  chess::Square wk = c.white_piece_squares[0];
  chess::Square pawn_a = c.white_piece_squares[1];
  chess::Square pawn_b = c.white_piece_squares[2];
  chess::Square bk = c.black_piece_squares[0];
  chess::Square pawn_c = c.black_piece_squares[1];

  if ((pawn_c & 7) > 3) {
    wk = (chess::Square)flip_we(wk);
    pawn_a = (chess::Square)flip_we(pawn_a);
    pawn_b = (chess::Square)flip_we(pawn_b);
    bk = (chess::Square)flip_we(bk);
    pawn_c = (chess::Square)flip_we(pawn_c);
  }

  int i = flip_we(flip_ns(pawn_a)) - 8;
  int j = flip_we(flip_ns(pawn_b)) - 8;
  int k = map24_b(pawn_c);

  int pp48_slice = PP48_IDX[i][j];
  if (idx_is_empty(pp48_slice))
    return NOINDEX;

  return k * BLOCK_Ax + pp48_slice * BLOCK_Bx + wk * BLOCK_Cx + bk;
}

int64_t kaakb_pctoindex(const Request &c) {
  const int N_WHITE = 3;
  const int N_BLACK = 2;
  const int BLOCK_Bx = 64;
  const int BLOCK_Ax = BLOCK_Bx * MAX_AAINDEX;

  int ft = FLIPT[c.black_piece_squares[0]][c.white_piece_squares[0]];
  std::vector<chess::Square> ws = c.white_piece_squares;
  std::vector<chess::Square> bs = c.black_piece_squares;

  if ((ft & WE_FLAG) != 0) {
    std::transform(ws.begin(), ws.begin() + N_WHITE, ws.begin(),
                   [](chess::Square sq) { return (chess::Square)flip_we(sq); });
    std::transform(bs.begin(), bs.begin() + N_BLACK, bs.begin(),
                   [](chess::Square sq) { return (chess::Square)flip_we(sq); });
  }
  if ((ft & NS_FLAG) != 0) {
    std::transform(ws.begin(), ws.begin() + N_WHITE, ws.begin(),
                   [](chess::Square sq) { return (chess::Square)flip_ns(sq); });
    std::transform(bs.begin(), bs.begin() + N_BLACK, bs.begin(),
                   [](chess::Square sq) { return (chess::Square)flip_ns(sq); });
  }
  if ((ft & NW_SE_FLAG) != 0) {
    std::transform(
        ws.begin(), ws.begin() + N_WHITE, ws.begin(),
        [](chess::Square sq) { return (chess::Square)flip_nw_se(sq); });
    std::transform(
        bs.begin(), bs.begin() + N_BLACK, bs.begin(),
        [](chess::Square sq) { return (chess::Square)flip_nw_se(sq); });
  }

  int ki = KKIDX[bs[0]][ws[0]];
  int ai = AAIDX[ws[1]][ws[2]];

  if (idx_is_empty(ki) || idx_is_empty(ai))
    return NOINDEX;

  return ki * BLOCK_Ax + ai * BLOCK_Bx + bs[1];
}

int64_t kabkc_pctoindex(const Request &c) {
  const int N_WHITE = 3;
  const int BLOCK_Ax = 64 * 64 * 64;
  const int BLOCK_Bx = 64 * 64;
  const int BLOCK_Cx = 64;

  int ft = FLIPT[c.black_piece_squares[0]][c.white_piece_squares[0]];
  std::vector<chess::Square> ws = c.white_piece_squares;
  std::vector<chess::Square> bs = c.black_piece_squares;

  if ((ft & WE_FLAG) != 0) {
    std::transform(ws.begin(), ws.begin() + N_WHITE, ws.begin(),
                   [](chess::Square sq) { return (chess::Square)flip_we(sq); });
    bs[0] = (chess::Square)flip_we(bs[0]);
    bs[1] = (chess::Square)flip_we(bs[1]);
  }
  if ((ft & NS_FLAG) != 0) {
    std::transform(ws.begin(), ws.begin() + N_WHITE, ws.begin(),
                   [](chess::Square sq) { return (chess::Square)flip_ns(sq); });
    bs[0] = (chess::Square)flip_ns(bs[0]);
    bs[1] = (chess::Square)flip_ns(bs[1]);
  }
  if ((ft & NW_SE_FLAG) != 0) {
    std::transform(
        ws.begin(), ws.begin() + N_WHITE, ws.begin(),
        [](chess::Square sq) { return (chess::Square)flip_nw_se(sq); });
    bs[0] = (chess::Square)flip_nw_se(bs[0]);
    bs[1] = (chess::Square)flip_nw_se(bs[1]);
  }

  int ki = KKIDX[bs[0]][ws[0]];
  if (idx_is_empty(ki))
    return NOINDEX;

  return ki * BLOCK_Ax + ws[1] * BLOCK_Bx + ws[2] * BLOCK_Cx + bs[1];
}

int64_t kpkp_pctoindex(const Request &c) {
  const int BLOCK_Ax = 64 * 64;
  const int BLOCK_Bx = 64;

  chess::Square wk = c.white_piece_squares[0];
  chess::Square bk = c.black_piece_squares[0];
  chess::Square pawn_a = c.white_piece_squares[1];
  chess::Square pawn_b = c.black_piece_squares[1];

  chess::Square anchor = pawn_a;
  chess::Square loosen = pawn_b;

  if ((anchor & 7) > 3) {
    anchor = (chess::Square)flip_we(anchor);
    loosen = (chess::Square)flip_we(loosen);
    wk = (chess::Square)flip_we(wk);
    bk = (chess::Square)flip_we(bk);
  }

  int m = wsq_to_pidx24(anchor);
  int n = loosen - 8;
  int pp_slice = m * 48 + n;

  if (idx_is_empty(pp_slice))
    return NOINDEX;
  return pp_slice * BLOCK_Ax + wk * BLOCK_Bx + bk;
}

int64_t kppk_pctoindex(const Request &c) {
  const int BLOCK_Ax = 64 * 64;
  const int BLOCK_Bx = 64;
  chess::Square wk = c.white_piece_squares[0];
  chess::Square pawn_a = c.white_piece_squares[1];
  chess::Square pawn_b = c.white_piece_squares[2];
  chess::Square bk = c.black_piece_squares[0];

  std::pair<chess::Square, chess::Square> res =
      pp_putanchorfirst(pawn_a, pawn_b);
  chess::Square anchor = res.first;
  chess::Square loosen = res.second;

  if ((anchor & 7) > 3) {
    anchor = (chess::Square)flip_we(anchor);
    loosen = (chess::Square)flip_we(loosen);
    wk = (chess::Square)flip_we(wk);
    bk = (chess::Square)flip_we(bk);
  }

  int i = wsq_to_pidx24(anchor);
  int j = wsq_to_pidx48(loosen);
  int pp_slice = PPIDX[i][j];

  if (idx_is_empty(pp_slice))
    return NOINDEX;
  return pp_slice * BLOCK_Ax + wk * BLOCK_Bx + bk;
}

int64_t kapk_pctoindex(const Request &c) {
  const int BLOCK_Ax = 64 * 64 * 64;
  const int BLOCK_Bx = 64 * 64;
  const int BLOCK_Cx = 64;

  chess::Square pawn = c.white_piece_squares[2];
  chess::Square wa = c.white_piece_squares[1];
  chess::Square wk = c.white_piece_squares[0];
  chess::Square bk = c.black_piece_squares[0];

  if (!(chess::SQ_A2 <= pawn && pawn < chess::SQ_A8))
    return NOINDEX;

  if ((pawn & 7) > 3) {
    pawn = (chess::Square)flip_we(pawn);
    wk = (chess::Square)flip_we(wk);
    bk = (chess::Square)flip_we(bk);
    wa = (chess::Square)flip_we(wa);
  }

  /*int sq = pawn;
  sq ^= 56; // flip_ns
  sq -= 8;
  int pslice = (sq + (sq & 3)) >> 1;*/
  int pslice = wsq_to_pidx24(pawn);
  return pslice * BLOCK_Ax + wk * BLOCK_Bx + bk * BLOCK_Cx + wa;
}

int64_t kabk_pctoindex(const Request &c) {
  const int BLOCK_Ax = 64 * 64;
  const int BLOCK_Bx = 64;

  int ft = flip_type(c.black_piece_squares[0], c.white_piece_squares[0]);
  std::vector<chess::Square> ws = c.white_piece_squares;
  std::vector<chess::Square> bs = c.black_piece_squares;

  if ((ft & 1) != 0) {
    std::transform(ws.begin(), ws.end(), ws.begin(),
                   [](chess::Square sq) { return (chess::Square)flip_we(sq); });
    std::transform(bs.begin(), bs.end(), bs.begin(),
                   [](chess::Square sq) { return (chess::Square)flip_we(sq); });
  }
  if ((ft & 2) != 0) {
    std::transform(ws.begin(), ws.end(), ws.begin(),
                   [](chess::Square sq) { return (chess::Square)flip_ns(sq); });
    std::transform(bs.begin(), bs.end(), bs.begin(),
                   [](chess::Square sq) { return (chess::Square)flip_ns(sq); });
  }
  if ((ft & 4) != 0) {
    std::transform(ws.begin(), ws.end(), ws.begin(), [](chess::Square sq) {
      return (chess::Square)flip_nw_se(sq);
    });
    std::transform(bs.begin(), bs.end(), bs.begin(), [](chess::Square sq) {
      return (chess::Square)flip_nw_se(sq);
    });
  }

  int ki = KKIDX[bs[0]][ws[0]];
  if (idx_is_empty(ki))
    return NOINDEX;
  return ki * BLOCK_Ax + ws[1] * BLOCK_Bx + ws[2];
}

int64_t kakp_pctoindex(const Request &c) {
  const int BLOCK_Ax = 64 * 64 * 64;
  const int BLOCK_Bx = 64 * 64;
  const int BLOCK_Cx = 64;

  chess::Square pawn = c.black_piece_squares[1];
  chess::Square wa = c.white_piece_squares[1];
  chess::Square wk = c.white_piece_squares[0];
  chess::Square bk = c.black_piece_squares[0];

  if (!(chess::SQ_A2 <= pawn && pawn < chess::SQ_A8))
    return NOINDEX;

  if ((pawn & 7) > 3) {
    pawn = (chess::Square)flip_we(pawn);
    wk = (chess::Square)flip_we(wk);
    bk = (chess::Square)flip_we(bk);
    wa = (chess::Square)flip_we(wa);
  }

  int sq = pawn - 8;
  int pslice = (sq + (sq & 3)) >> 1;
  return pslice * BLOCK_Ax + wk * BLOCK_Bx + bk * BLOCK_Cx + wa;
}

int64_t kaak_pctoindex(const Request &c) {
  const int N_WHITE = 3;
  const int BLOCK_Ax = MAX_AAINDEX;

  int ft = FLIPT[c.black_piece_squares[0]][c.white_piece_squares[0]];
  std::vector<chess::Square> ws = c.white_piece_squares;
  std::vector<chess::Square> bs = c.black_piece_squares;

  if ((ft & WE_FLAG) != 0) {
    for (int i = 0; i < N_WHITE; ++i)
      ws[i] = (chess::Square)flip_we(ws[i]);
    bs[0] = (chess::Square)flip_we(bs[0]);
  }
  if ((ft & NS_FLAG) != 0) {
    for (int i = 0; i < N_WHITE; ++i)
      ws[i] = (chess::Square)flip_ns(ws[i]);
    bs[0] = (chess::Square)flip_ns(bs[0]);
  }
  if ((ft & NW_SE_FLAG) != 0) {
    for (int i = 0; i < N_WHITE; ++i)
      ws[i] = (chess::Square)flip_nw_se(ws[i]);
    bs[0] = (chess::Square)flip_nw_se(bs[0]);
  }

  int ki = KKIDX[bs[0]][ws[0]];
  int ai = AAIDX[ws[1]][ws[2]];

  if (idx_is_empty(ki) || idx_is_empty(ai))
    return NOINDEX;
  return ki * BLOCK_Ax + ai;
}

int64_t kakb_pctoindex(const Request &c) {
  const int BLOCK_Ax = 64 * 64;
  const int BLOCK_Bx = 64;

  int ft = FLIPT[c.black_piece_squares[0]][c.white_piece_squares[0]];
  std::vector<chess::Square> ws = c.white_piece_squares;
  std::vector<chess::Square> bs = c.black_piece_squares;

  if ((ft & 1) != 0) {
    for (auto &s : ws)
      s = (chess::Square)flip_we(s);
    for (auto &s : bs)
      s = (chess::Square)flip_we(s);
  }
  if ((ft & 2) != 0) {
    for (auto &s : ws)
      s = (chess::Square)flip_ns(s);
    for (auto &s : bs)
      s = (chess::Square)flip_ns(s);
  }
  if ((ft & 4) != 0) {
    for (auto &s : ws)
      s = (chess::Square)flip_nw_se(s);
    for (auto &s : bs)
      s = (chess::Square)flip_nw_se(s);
  }

  int ki = KKIDX[bs[0]][ws[0]];
  if (idx_is_empty(ki))
    return NOINDEX;
  return ki * BLOCK_Ax + ws[1] * BLOCK_Bx + bs[1];
}

int64_t kpk_pctoindex(const Request &c) {
  const int BLOCK_A = 64 * 64;
  const int BLOCK_B = 64;

  chess::Square pawn = c.white_piece_squares[1];
  chess::Square wk = c.white_piece_squares[0];
  chess::Square bk = c.black_piece_squares[0];

  if (!(chess::SQ_A2 <= pawn && pawn < chess::SQ_A8))
    return NOINDEX;

  if ((pawn & 7) > 3) {
    pawn = (chess::Square)flip_we(pawn);
    wk = (chess::Square)flip_we(wk);
    bk = (chess::Square)flip_we(bk);
  }

  /*int sq = pawn;
  sq^=56;
  sq-=chess::NORTH;
  int pslice = (sq + (sq & 3)) >> 1;*/
  int pslice = wsq_to_pidx24(pawn);
  return pslice * BLOCK_A + wk * BLOCK_B + bk;
}

int64_t kpppk_pctoindex(const Request &c) {
  const int BLOCK_A = 64 * 64;
  const int BLOCK_B = 64;

  chess::Square wk = c.white_piece_squares[0];
  chess::Square pawn_a = c.white_piece_squares[1];
  chess::Square pawn_b = c.white_piece_squares[2];
  chess::Square pawn_c = c.white_piece_squares[3];
  chess::Square bk = c.black_piece_squares[0];

  int i = pawn_a - 8;
  int j = pawn_b - 8;
  int k = pawn_c - 8;

  int ppp48_slice = -1;
  ppp48_slice = PPP48_IDX[i][j][k];

  if (idx_is_empty(ppp48_slice)) {
    wk = (chess::Square)flip_we(wk);
    pawn_a = (chess::Square)flip_we(pawn_a);
    pawn_b = (chess::Square)flip_we(pawn_b);
    pawn_c = (chess::Square)flip_we(pawn_c);
    bk = (chess::Square)flip_we(bk);
  }

  i = pawn_a - 8;
  j = pawn_b - 8;
  k = pawn_c - 8;
  ppp48_slice = -1;
  ppp48_slice = PPP48_IDX[i][j][k];
  if (idx_is_empty(ppp48_slice))
    return NOINDEX;
  return ppp48_slice * BLOCK_A + wk * BLOCK_B + bk;
}

std::vector<int> egtb_block_unpack(int side, int n,
                                   const std::vector<uint8_t> &bp) {
  std::vector<int> result;
  for (int i = 0; i < n && i < (int)bp.size(); ++i) {
    result.push_back(dtm_unpack(side, bp[i]));
  }
  return result;
}
std::unordered_map<std::string, EndgameKey> EGKEY = {
    {"kqk", EndgameKey(MAX_KXK, 1, kxk_pctoindex)},
    {"krk", EndgameKey(MAX_KXK, 1, kxk_pctoindex)},
    {"kbk", EndgameKey(MAX_KXK, 1, kxk_pctoindex)},
    {"knk", EndgameKey(MAX_KXK, 1, kxk_pctoindex)},
    {"kpk", EndgameKey(MAX_kpk, 24, kpk_pctoindex)},

    {"kqkq", EndgameKey(MAX_kakb, 1, kakb_pctoindex)},
    {"kqkr", EndgameKey(MAX_kakb, 1, kakb_pctoindex)},
    {"kqkb", EndgameKey(MAX_kakb, 1, kakb_pctoindex)},
    {"kqkn", EndgameKey(MAX_kakb, 1, kakb_pctoindex)},

    {"krkr", EndgameKey(MAX_kakb, 1, kakb_pctoindex)},
    {"krkb", EndgameKey(MAX_kakb, 1, kakb_pctoindex)},
    {"krkn", EndgameKey(MAX_kakb, 1, kakb_pctoindex)},

    {"kbkb", EndgameKey(MAX_kakb, 1, kakb_pctoindex)},
    {"kbkn", EndgameKey(MAX_kakb, 1, kakb_pctoindex)},

    {"knkn", EndgameKey(MAX_kakb, 1, kakb_pctoindex)},

    {"kqqk", EndgameKey(MAX_kaak, 1, kaak_pctoindex)},
    {"kqrk", EndgameKey(MAX_kabk, 1, kabk_pctoindex)},
    {"kqbk", EndgameKey(MAX_kabk, 1, kabk_pctoindex)},
    {"kqnk", EndgameKey(MAX_kabk, 1, kabk_pctoindex)},

    {"krrk", EndgameKey(MAX_kaak, 1, kaak_pctoindex)},
    {"krbk", EndgameKey(MAX_kabk, 1, kabk_pctoindex)},
    {"krnk", EndgameKey(MAX_kabk, 1, kabk_pctoindex)},

    {"kbbk", EndgameKey(MAX_kaak, 1, kaak_pctoindex)},
    {"kbnk", EndgameKey(MAX_kabk, 1, kabk_pctoindex)},

    {"knnk", EndgameKey(MAX_kaak, 1, kaak_pctoindex)},
    {"kqkp", EndgameKey(MAX_kakp, 24, kakp_pctoindex)},
    {"krkp", EndgameKey(MAX_kakp, 24, kakp_pctoindex)},
    {"kbkp", EndgameKey(MAX_kakp, 24, kakp_pctoindex)},
    {"knkp", EndgameKey(MAX_kakp, 24, kakp_pctoindex)},

    {"kqpk", EndgameKey(MAX_kapk, 24, kapk_pctoindex)},
    {"krpk", EndgameKey(MAX_kapk, 24, kapk_pctoindex)},
    {"kbpk", EndgameKey(MAX_kapk, 24, kapk_pctoindex)},
    {"knpk", EndgameKey(MAX_kapk, 24, kapk_pctoindex)},

    {"kppk", EndgameKey(MAX_kppk, MAX_PPINDEX, kppk_pctoindex)},

    {"kpkp", EndgameKey(MAX_kpkp, MAX_PpINDEX, kpkp_pctoindex)},

    {"kppkp", EndgameKey(MAX_kppkp, 24 * MAX_PP48_INDEX, kppkp_pctoindex)},

    {"kbbkr", EndgameKey(MAX_kaakb, 1, kaakb_pctoindex)},
    {"kbbkb", EndgameKey(MAX_kaakb, 1, kaakb_pctoindex)},
    {"knnkb", EndgameKey(MAX_kaakb, 1, kaakb_pctoindex)},
    {"knnkn", EndgameKey(MAX_kaakb, 1, kaakb_pctoindex)},

    {"kqqqk", EndgameKey(MAX_kaaak, 1, kaaak_pctoindex)},
    {"kqqrk", EndgameKey(MAX_kaabk, 1, kaabk_pctoindex)},
    {"kqqbk", EndgameKey(MAX_kaabk, 1, kaabk_pctoindex)},
    {"kqqnk", EndgameKey(MAX_kaabk, 1, kaabk_pctoindex)},
    {"kqrrk", EndgameKey(MAX_kabbk, 1, kabbk_pctoindex)},
    {"kqrbk", EndgameKey(MAX_kabck, 1, kabck_pctoindex)},
    {"kqrnk", EndgameKey(MAX_kabck, 1, kabck_pctoindex)},
    {"kqbbk", EndgameKey(MAX_kabbk, 1, kabbk_pctoindex)},
    {"kqbnk", EndgameKey(MAX_kabck, 1, kabck_pctoindex)},
    {"kqnnk", EndgameKey(MAX_kabbk, 1, kabbk_pctoindex)},
    {"krrrk", EndgameKey(MAX_kaaak, 1, kaaak_pctoindex)},
    {"krrbk", EndgameKey(MAX_kaabk, 1, kaabk_pctoindex)},
    {"krrnk", EndgameKey(MAX_kaabk, 1, kaabk_pctoindex)},
    {"krbbk", EndgameKey(MAX_kabbk, 1, kabbk_pctoindex)},
    {"krbnk", EndgameKey(MAX_kabck, 1, kabck_pctoindex)},
    {"krnnk", EndgameKey(MAX_kabbk, 1, kabbk_pctoindex)},
    {"kbbbk", EndgameKey(MAX_kaaak, 1, kaaak_pctoindex)},
    {"kbbnk", EndgameKey(MAX_kaabk, 1, kaabk_pctoindex)},
    {"kbnnk", EndgameKey(MAX_kabbk, 1, kabbk_pctoindex)},
    {"knnnk", EndgameKey(MAX_kaaak, 1, kaaak_pctoindex)},
    {"kqqkq", EndgameKey(MAX_kaakb, 1, kaakb_pctoindex)},
    {"kqqkr", EndgameKey(MAX_kaakb, 1, kaakb_pctoindex)},
    {"kqqkb", EndgameKey(MAX_kaakb, 1, kaakb_pctoindex)},
    {"kqqkn", EndgameKey(MAX_kaakb, 1, kaakb_pctoindex)},
    {"kqrkq", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"kqrkr", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"kqrkb", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"kqrkn", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"kqbkq", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"kqbkr", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"kqbkb", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"kqbkn", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"kqnkq", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"kqnkr", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"kqnkb", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"kqnkn", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"krrkq", EndgameKey(MAX_kaakb, 1, kaakb_pctoindex)},
    {"krrkr", EndgameKey(MAX_kaakb, 1, kaakb_pctoindex)},
    {"krrkb", EndgameKey(MAX_kaakb, 1, kaakb_pctoindex)},
    {"krrkn", EndgameKey(MAX_kaakb, 1, kaakb_pctoindex)},
    {"krbkq", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"krbkr", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"krbkb", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"krbkn", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"krnkq", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"krnkr", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"krnkb", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"krnkn", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"kbbkq", EndgameKey(MAX_kaakb, 1, kaakb_pctoindex)},
    {"kbbkn", EndgameKey(MAX_kaakb, 1, kaakb_pctoindex)},
    {"kbnkq", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"kbnkr", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"kbnkb", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"kbnkn", EndgameKey(MAX_kabkc, 1, kabkc_pctoindex)},
    {"knnkq", EndgameKey(MAX_kaakb, 1, kaakb_pctoindex)},
    {"knnkr", EndgameKey(MAX_kaakb, 1, kaakb_pctoindex)},

    {"kqqpk", EndgameKey(MAX_kaapk, 24, kaapk_pctoindex)},
    {"kqrpk", EndgameKey(MAX_kabpk, 24, kabpk_pctoindex)},
    {"kqbpk", EndgameKey(MAX_kabpk, 24, kabpk_pctoindex)},
    {"kqnpk", EndgameKey(MAX_kabpk, 24, kabpk_pctoindex)},
    {"krrpk", EndgameKey(MAX_kaapk, 24, kaapk_pctoindex)},
    {"krbpk", EndgameKey(MAX_kabpk, 24, kabpk_pctoindex)},
    {"krnpk", EndgameKey(MAX_kabpk, 24, kabpk_pctoindex)},
    {"kbbpk", EndgameKey(MAX_kaapk, 24, kaapk_pctoindex)},
    {"kbnpk", EndgameKey(MAX_kabpk, 24, kabpk_pctoindex)},
    {"knnpk", EndgameKey(MAX_kaapk, 24, kaapk_pctoindex)},

    {"kqppk", EndgameKey(MAX_kappk, MAX_PPINDEX, kappk_pctoindex)},
    {"krppk", EndgameKey(MAX_kappk, MAX_PPINDEX, kappk_pctoindex)},
    {"kbppk", EndgameKey(MAX_kappk, MAX_PPINDEX, kappk_pctoindex)},
    {"knppk", EndgameKey(MAX_kappk, MAX_PPINDEX, kappk_pctoindex)},

    {"kqpkq", EndgameKey(MAX_kapkb, 24, kapkb_pctoindex)},
    {"kqpkr", EndgameKey(MAX_kapkb, 24, kapkb_pctoindex)},
    {"kqpkb", EndgameKey(MAX_kapkb, 24, kapkb_pctoindex)},
    {"kqpkn", EndgameKey(MAX_kapkb, 24, kapkb_pctoindex)},
    {"krpkq", EndgameKey(MAX_kapkb, 24, kapkb_pctoindex)},
    {"krpkr", EndgameKey(MAX_kapkb, 24, kapkb_pctoindex)},
    {"krpkb", EndgameKey(MAX_kapkb, 24, kapkb_pctoindex)},
    {"krpkn", EndgameKey(MAX_kapkb, 24, kapkb_pctoindex)},
    {"kbpkq", EndgameKey(MAX_kapkb, 24, kapkb_pctoindex)},
    {"kbpkr", EndgameKey(MAX_kapkb, 24, kapkb_pctoindex)},
    {"kbpkb", EndgameKey(MAX_kapkb, 24, kapkb_pctoindex)},
    {"kbpkn", EndgameKey(MAX_kapkb, 24, kapkb_pctoindex)},
    {"knpkq", EndgameKey(MAX_kapkb, 24, kapkb_pctoindex)},
    {"knpkr", EndgameKey(MAX_kapkb, 24, kapkb_pctoindex)},
    {"knpkb", EndgameKey(MAX_kapkb, 24, kapkb_pctoindex)},
    {"knpkn", EndgameKey(MAX_kapkb, 24, kapkb_pctoindex)},
    {"kppkq", EndgameKey(MAX_kppka, MAX_PPINDEX, kppka_pctoindex)},
    {"kppkr", EndgameKey(MAX_kppka, MAX_PPINDEX, kppka_pctoindex)},
    {"kppkb", EndgameKey(MAX_kppka, MAX_PPINDEX, kppka_pctoindex)},
    {"kppkn", EndgameKey(MAX_kppka, MAX_PPINDEX, kppka_pctoindex)},

    {"kqqkp", EndgameKey(MAX_kaakp, 24, kaakp_pctoindex)},
    {"kqrkp", EndgameKey(MAX_kabkp, 24, kabkp_pctoindex)},
    {"kqbkp", EndgameKey(MAX_kabkp, 24, kabkp_pctoindex)},
    {"kqnkp", EndgameKey(MAX_kabkp, 24, kabkp_pctoindex)},
    {"krrkp", EndgameKey(MAX_kaakp, 24, kaakp_pctoindex)},
    {"krbkp", EndgameKey(MAX_kabkp, 24, kabkp_pctoindex)},
    {"krnkp", EndgameKey(MAX_kabkp, 24, kabkp_pctoindex)},
    {"kbbkp", EndgameKey(MAX_kaakp, 24, kaakp_pctoindex)},
    {"kbnkp", EndgameKey(MAX_kabkp, 24, kabkp_pctoindex)},
    {"knnkp", EndgameKey(MAX_kaakp, 24, kaakp_pctoindex)},

    {"kqpkp", EndgameKey(MAX_kapkp, MAX_PpINDEX, kapkp_pctoindex)},
    {"krpkp", EndgameKey(MAX_kapkp, MAX_PpINDEX, kapkp_pctoindex)},
    {"kbpkp", EndgameKey(MAX_kapkp, MAX_PpINDEX, kapkp_pctoindex)},
    {"knpkp", EndgameKey(MAX_kapkp, MAX_PpINDEX, kapkp_pctoindex)},

    {"kpppk", EndgameKey(MAX_kpppk, MAX_PPP48_INDEX, kpppk_pctoindex)},
};
int dtm_unpack(int stm, int packed) {
  if (packed == iDRAW || packed == iFORBID)
    return packed;
  int info = packed & 3;
  int store = packed >> 2;
  int moves = 0, plies = 0, prefx = 0;
  if (stm == 0) {
    if (info == iWMATE) {
      moves = store + 1;
      plies = moves * 2 - 1;
      prefx = info;
    } else if (info == iBMATE) {
      moves = store;
      plies = moves * 2;
      prefx = info;
    } else if (info == iDRAW) {
      moves = store + 1 + 63;
      plies = moves * 2 - 1;
      prefx = iWMATE;
    } else if (info == iFORBID) {
      moves = store + 63;
      plies = moves * 2;
      prefx = iBMATE;
    } else {
      plies = 0;
      prefx = 0;
    }
  } else {
    if (info == iBMATE) {
      moves = store + 1;
      plies = moves * 2 - 1;
      prefx = info;
    } else if (info == iWMATE) {
      moves = store;
      plies = moves * 2;
      prefx = info;
    } else if (info == iDRAW) {
      if (store == 63) {
        // Exception: no position in the 5-man TBs needs to store 63 for
        // iBMATE. It is then just used to indicate iWMATE.
        store += 1;
        moves = store + 63;
        plies = moves * 2;
        prefx = iWMATE;
      } else {
        moves = store + 1 + 63;
        plies = moves * 2 - 1;
        prefx = iBMATE;
      }
    } else if (info == iFORBID) {
      moves = store + 63;
      plies = moves * 2;
      prefx = iWMATE;
    } else {
      plies = 0;
      prefx = 0;
    }
  }
  return prefx | (static_cast<int>(static_cast<unsigned>(plies) << 3));
}

void PythonTablebase::add_directory(std::string directory) {
  directory = std::filesystem::absolute(directory).string();
  if (!std::filesystem::is_directory(directory))
    throw std::filesystem::filesystem_error(
        "not a directory", std::make_error_code(std::errc::not_a_directory));

  for (const auto &entry : std::filesystem::directory_iterator(directory)) {
    std::string filename = entry.path().filename().string();
    constexpr std::string_view suffix = ".gtb.cp4";
    if (filename.size() > suffix.size() &&
        filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) == 0) {
      std::string key = filename.substr(0, filename.size() - suffix.size());
      available_tables[key] = entry.path().string();
    }
  }
}

int PythonTablebase::probe_dtm(chess::Board &board) {
  if (board.castlingRights() != chess::NO_CASTLING)
    throw std::runtime_error(
        "gaviota tables do not contain positions with castling rights");

  if (chess::popcount(board.occ()) > 5)
    throw std::runtime_error("gaviota tables support up to 5 pieces");

  if (board.occ() == board.pieces(chess::KING))
    return 0;

  int dtm = _probe_dtm_no_ep(board);
  chess::Movelist moves;
  board.legals<chess::MoveGenType::PAWN | chess::MoveGenType::CAPTURE>(moves);
  for (auto move : moves) {
    if (move.typeOf() != chess::MoveType::EN_PASSANT)
      continue;
    board.doMove(move);
    int child_dtm;
    if (board.is_checkmate())
      child_dtm = 1;
    else {
      child_dtm = -_probe_dtm_no_ep(board);
      if (child_dtm > 0)
        child_dtm++;
      else if (child_dtm < 0)
        child_dtm--;
    }
    if (dtm * child_dtm > 0)
      dtm = std::min(dtm, child_dtm);
    else
      dtm = std::max(dtm, child_dtm);
    board.undoMove();
  }
  return dtm;
}

int PythonTablebase::_probe_dtm_no_ep(chess::Board &board) {
  std::vector<chess::Square> white_squares;
  std::vector<chess::PieceType> white_types;
  chess::Bitboard wbb = board.occ(chess::WHITE);
  while (wbb) {
    chess::Square sq = static_cast<chess::Square>(chess::pop_lsb(wbb));
    white_squares.push_back(sq);
    white_types.push_back(board.at<chess::PieceType>(sq));
  }
  std::vector<chess::Square> black_squares;
  std::vector<chess::PieceType> black_types;
  chess::Bitboard bbb = board.occ(chess::BLACK);
  while (bbb) {
    chess::Square sq = static_cast<chess::Square>(chess::pop_lsb(bbb));
    black_squares.push_back(sq);
    black_types.push_back(board.at<chess::PieceType>(sq));
  }
  int side = (board.sideToMove() == chess::WHITE) ? 0 : 1;
  Request req(white_squares, white_types, black_squares, black_types, side);
  int dtm = _tb_probe(req);
  auto [ply, res] = unpackdist(dtm);

  if (res == iWMATE) {
    if (req.realside == 1)
      return req.is_reversed ? ply : -ply;
    else
      return req.is_reversed ? -ply : ply;
  } else if (res == iBMATE) {
    if (req.realside == 0)
      return req.is_reversed ? ply : -ply;
    else
      return req.is_reversed ? -ply : ply;
  }
  return 0;
}

std::optional<int> PythonTablebase::get_dtm(chess::Board &board,
                                            std::optional<int> default_val) {
  try {
    return probe_dtm(board);
  } catch (...) {
    return default_val;
  }
}

int PythonTablebase::probe_wdl(chess::Board &board) {
  int dtm = probe_dtm(board);
  if (dtm == 0)
    return board.is_checkmate() ? -1 : 0;
  return (dtm > 0) ? 1 : -1;
}

std::optional<int> PythonTablebase::get_wdl(chess::Board &board,
                                            std::optional<int> default_val) {
  try {
    return probe_wdl(board);
  } catch (...) {
    return default_val;
  }
}

std::unique_ptr<std::ifstream> &
PythonTablebase::_setup_tablebase(Request &req) {
  auto piece_symbol = [](chess::PieceType t) {
    switch (t) {
    case chess::PAWN:
      return 'p';
    case chess::KNIGHT:
      return 'n';
    case chess::BISHOP:
      return 'b';
    case chess::ROOK:
      return 'r';
    case chess::QUEEN:
      return 'q';
    case chess::KING:
      return 'k';
    default:
      throw std::runtime_error("invalid piece type");
    }
  };
  std::string white_letters = "";
  for (auto t : req.white_types)
    white_letters += piece_symbol(t);
  std::string black_letters = "";
  for (auto t : req.black_types)
    black_letters += piece_symbol(t);

  if (available_tables.count(white_letters + black_letters)) {
    req.is_reversed = false;
    req.egkey = white_letters + black_letters;
    req.white_piece_squares = req.white_squares;
    req.white_piece_types = req.white_types;
    req.black_piece_squares = req.black_squares;
    req.black_piece_types = req.black_types;
  } else if (available_tables.count(black_letters + white_letters)) {
    req.is_reversed = true;
    req.egkey = black_letters + white_letters;
    req.white_piece_squares.clear();
    req.black_piece_squares.clear();
    for (chess::Square s : req.black_squares)
      req.white_piece_squares.push_back((chess::Square)flip_ns(s));
    req.white_piece_types = req.black_types;
    for (chess::Square s : req.white_squares)
      req.black_piece_squares.push_back((chess::Square)flip_ns(s));
    req.black_piece_types = req.white_types;
    req.side = opp(req.side);
  } else
    throw std::runtime_error("no gaviota table available");

  return _open_tablebase(req);
}

std::unique_ptr<std::ifstream> &PythonTablebase::_open_tablebase(Request &req) {
  if (streams.find(req.egkey) == streams.end()) {
    std::string path = available_tables.at(req.egkey);
    streams[req.egkey] =
        std::make_unique<std::ifstream>(std::ifstream(path, std::ios::binary));
    egtb_loadindexes(req.egkey, streams[req.egkey]);
  }
  return streams[req.egkey];
}

void PythonTablebase::close() {
  available_tables.clear();
  zipinfo.clear();
  block_age = 0;
  block_cache.clear();
  for (auto &pair : streams)
    if (pair.second)
      pair.second->close();

  streams.clear();
}

int PythonTablebase::egtb_block_getnumber(Request &req, int idx) {
  int maxindex = EGKEY.at(req.egkey).maxindex;
  int blocks_per_side = 1 + (maxindex - 1) / ENTRIES_PER_BLOCK;
  return req.side * blocks_per_side + (idx / ENTRIES_PER_BLOCK);
}

int PythonTablebase::egtb_block_getsize(Request &req, int idx) {
  int maxindex = EGKEY.at(req.egkey).maxindex;
  int block = idx / ENTRIES_PER_BLOCK;
  int offset = block * ENTRIES_PER_BLOCK;
  if (offset + ENTRIES_PER_BLOCK > maxindex)
    return maxindex - offset;
  return ENTRIES_PER_BLOCK;
}

ZipInfo &
PythonTablebase::egtb_loadindexes(std::string egkey,
                                  std::unique_ptr<std::ifstream> &stream) {
  if (zipinfo.find(egkey) == zipinfo.end()) {
    stream->seekg(0);
    uint32_t header[10];
    stream->read((char *)header, 40);
    if (stream->gcount() != 40LL) {
      throw std::runtime_error("Short read while loading indexes");
    }
    uint32_t offset = header[8];
    if (offset < 40)
      throw std::runtime_error("Invalid offset in tablebase index header");
    uint64_t n_idx = (offset - 40) / 4;
    if (n_idx == 0 || n_idx > 1024 * 1024)
      throw std::runtime_error("Invalid index count in tablebase header");
    std::vector<uint32_t> p(static_cast<size_t>(n_idx));
    const std::streamsize bytes_to_read =
        static_cast<std::streamsize>(n_idx) * static_cast<std::streamsize>(4);
    stream->read(reinterpret_cast<char *>(p.data()), bytes_to_read);
    if (stream->gcount() != bytes_to_read)
      throw std::runtime_error("Short read while loading block index");
    zipinfo[egkey] = {0, static_cast<int>(n_idx), p};
  }
  return zipinfo[egkey];
}

int PythonTablebase::egtb_block_getsize_zipped(std::string egkey, int block) {
  const auto &idx = zipinfo[egkey].blockindex;
  if (block < 0 || static_cast<size_t>(block + 1) >= idx.size())
    throw std::runtime_error("Block index out of bounds in egtb_block_getsize_zipped");
  return idx[block + 1] - idx[block];
}

int PythonTablebase::egtb_block_park(std::string egkey, int block,
                                     std::unique_ptr<std::ifstream> &stream) {
  int i = zipinfo[egkey].blockindex[block] + zipinfo[egkey].extraoffset;
  stream->seekg(i);
  return i;
}

PythonTablebase open_tablebase(std::string directory) {
  PythonTablebase tables;
  tables.add_directory(directory);
  return tables;
}
// Decompression
static const char *lzma_ret_name(lzma_ret r) {
  switch (r) {
  case LZMA_OK:
    return "LZMA_OK";
  case LZMA_STREAM_END:
    return "LZMA_STREAM_END";
  case LZMA_NO_CHECK:
    return "LZMA_NO_CHECK";
  case LZMA_UNSUPPORTED_CHECK:
    return "LZMA_UNSUPPORTED_CHECK";
  case LZMA_GET_CHECK:
    return "LZMA_GET_CHECK";
  case LZMA_MEM_ERROR:
    return "LZMA_MEM_ERROR";
  case LZMA_MEMLIMIT_ERROR:
    return "LZMA_MEMLIMIT_ERROR";
  case LZMA_FORMAT_ERROR:
    return "LZMA_FORMAT_ERROR";
  case LZMA_OPTIONS_ERROR:
    return "LZMA_OPTIONS_ERROR";
  case LZMA_DATA_ERROR:
    return "LZMA_DATA_ERROR";
  case LZMA_BUF_ERROR:
    return "LZMA_BUF_ERROR";
  case LZMA_PROG_ERROR:
    return "LZMA_PROG_ERROR";
  default:
    return "UNKNOWN";
  }
}
std::vector<uint8_t> decompress(const std::vector<uint8_t> &compressed_data,
                                size_t uncompressed_size) {
  lzma_stream strm = LZMA_STREAM_INIT;
  if (lzma_alone_decoder(&strm, UINT64_MAX) != LZMA_OK) {
    throw std::runtime_error("Failed to initialize LZMA decoder");
  }

  std::vector<uint8_t> decompressed_data(uncompressed_size);

  strm.next_in = compressed_data.data();
  strm.avail_in = compressed_data.size();
  strm.next_out = decompressed_data.data();
  strm.avail_out = decompressed_data.size();

  while (true) {
    lzma_ret ret = lzma_code(&strm, LZMA_RUN);
    if (ret == LZMA_STREAM_END)
      break;
    if (ret != LZMA_OK) {
      lzma_end(&strm);
      throw std::runtime_error(
          "Decompression failed (Code: " + std::to_string(ret) + " - " +
          lzma_ret_name(ret) + ")");
    }
    if (strm.avail_in == 0 && strm.avail_out > 0) {
      lzma_end(&strm);
      throw std::runtime_error(
          "Decompression stopped before output was filled");
    }
  }

  size_t produced = decompressed_data.size() - strm.avail_out;
  size_t consumed = compressed_data.size() - strm.avail_in;
  lzma_end(&strm);

  if (produced != uncompressed_size) {
    throw std::runtime_error("Wrong decompressed size");
  }
  if (consumed == 0) {
    throw std::runtime_error("No compressed input consumed");
  }

  return decompressed_data;
}
int PythonTablebase::_tb_probe(Request &req) {
  std::unique_ptr<std::ifstream> &stream = _setup_tablebase(req);
  int64_t idx = EGKEY.at(req.egkey).pctoi(req);
  if (idx == NOINDEX) {
    throw std::runtime_error("Position cannot be indexed for this table");
  }
  auto [offset, remainder] = split_index(idx);
  auto cache_key = std::make_tuple(req.egkey, offset, req.side);
  if (block_cache.find(cache_key) == block_cache.end()) {
    TableBlock t(req.egkey, req.side, offset, block_age);
    int block = egtb_block_getnumber(req, idx);
    int n = egtb_block_getsize(req, idx);
    int z = egtb_block_getsize_zipped(req.egkey, block);
    egtb_block_park(req.egkey, block, stream);
    std::vector<uint8_t> buffer_zipped(z);
    stream->read(reinterpret_cast<char *>(buffer_zipped.data()), z);
    const std::streamsize bytes_read = stream->gcount();
    if (bytes_read != static_cast<std::streamsize>(z)) {
      throw std::runtime_error("Compressed buffer read failed: expected " +
                               std::to_string(z) + " bytes, got " +
                               std::to_string(bytes_read));
    }

    if (buffer_zipped.empty())
      throw std::runtime_error("Empty compressed block");

    std::vector<uint8_t> lzma_input;
    if (buffer_zipped[0] == 0) {
      if (buffer_zipped.size() < 2)
        throw std::runtime_error("Invalid raw LZMA data");
      lzma_input.assign(buffer_zipped.begin() + 2, buffer_zipped.end());
    } else {
      if (buffer_zipped.size() < 15)
        throw std::runtime_error("Invalid LZMA86 data");
      const uint32_t DICTIONARY_SIZE = 4096;
      const uint8_t POS_STATE_BITS = 2;
      const uint8_t NUM_LITERAL_POS_STATE_BITS = 0;
      const uint8_t NUM_LITERAL_CONTEXT_BITS = 3;
      std::vector<uint8_t> properties(13);
      properties[0] = (POS_STATE_BITS * 5 + NUM_LITERAL_POS_STATE_BITS) * 9 +
                      NUM_LITERAL_CONTEXT_BITS;
      for (int i = 0; i < 4; ++i)
        properties[1 + i] = (DICTIONARY_SIZE >> (8 * i)) & 0xFF;
      const uint64_t n64 = static_cast<uint64_t>(n);
      for (int i = 0; i < 8; ++i)
        properties[5 + i] = static_cast<uint8_t>((n64 >> (8 * i)) & 0xFF);
      lzma_input.reserve(properties.size() + buffer_zipped.size() - 15);
      lzma_input.insert(lzma_input.end(), properties.begin(), properties.end());
      lzma_input.insert(lzma_input.end(), buffer_zipped.begin() + 15,
                        buffer_zipped.end());
    }

    std::vector<uint8_t> buffer_packed = decompress(lzma_input, n);
    if (buffer_packed.size() != static_cast<size_t>(n)) {
      throw std::runtime_error("Decompressed block size mismatch");
    }
    t.pcache = egtb_block_unpack(req.side, n, buffer_packed);
    block_cache.insert({cache_key, t});

    // LRU cache cleanup
    if (block_cache.size() > 128) {
      auto lru = std::min_element(block_cache.begin(), block_cache.end(),
                                  [](const auto &a, const auto &b) {
                                    return a.second.age < b.second.age;
                                  });
      block_cache.erase(lru);
    }
    (void)block_cache.at(cache_key).pcache.at(remainder);
  } else {
    block_cache.at(cache_key).age = block_age;
  }

  block_age++;
  return block_cache.at(cache_key).pcache.at(remainder);
}

} // namespace tbprobe::gaviota
