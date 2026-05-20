#include "tbprobe.h"
#include <algorithm>
#include <fstream>
#include <lzma.h>
#include <stdexcept>

namespace tbprobe::gaviota {

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

  lzma_ret ret = lzma_code(&strm, LZMA_RUN);

  lzma_end(&strm);

  if ((ret != LZMA_OK && ret != LZMA_STREAM_END) || strm.avail_out > 0) {
    throw std::runtime_error(
        "Decompression failed (Code: " + std::to_string(ret) + " - " +
        lzma_ret_name(ret) + ")");
  }

  return decompressed_data;
}
int PythonTablebase::_tb_probe(Request &req) {
  std::unique_ptr<std::ifstream> &stream = _setup_tablebase(req);
  int64_t idx = EGKEY.at(req.egkey).pctoi(req);
  auto [offset, remainder] = split_index(idx);

  auto cache_key = std::make_tuple(req.egkey, offset, req.side);
  if (block_cache.find(cache_key) == block_cache.end()) {
    TableBlock t(req.egkey, req.side, offset, block_age);
    int block = egtb_block_getnumber(req, idx);
    int n = egtb_block_getsize(req, idx);
    int z = egtb_block_getsize_zipped(req.egkey, block);

    egtb_block_park(req.egkey, block, stream);
    std::vector<uint8_t> buffer_zipped(z);
    stream->read((char *)buffer_zipped.data(), z);
    if (buffer_zipped.empty())
      throw std::runtime_error("Compressed buffer is empty");

    std::vector<uint8_t> lzma_input;
    if (buffer_zipped[0] == 0) {
      // If flag is zero, plain LZMA is following.
      if (buffer_zipped.size() < 2)
        throw std::runtime_error("Invalid raw LZMA data");
      lzma_input.assign(buffer_zipped.begin() + 2, buffer_zipped.end());
    } else {
      // Else LZMA86. Build a fake header.
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
