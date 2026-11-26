#ifndef TORRENT_PARSER_H_
#define TORRENT_PARSER_H_

#include <iostream>
#include <string>
#include <string_view>
#include <charconv>
#include <cstdint>
#include <variant>
#include <vector>
#include <cmath>
#include <type_traits>
#include <utility>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <format>
#include <array>
#include <optional>
#include <openssl/evp.h>

#include "../include/bencoder.h"

using BencodeValue = BencodeParser::BencodeValue;
using PieceHash = std::array<char, 20>;
using InfoHash = std::array<unsigned char, 20>;

class TorrentFileParser {
public:
    struct FileLayout {
        std::int64_t file_size;
        std::int64_t piece_length;
    };
    
    TorrentFileParser(const std::filesystem::path&);

    std::string read_binary_file();

    std::string extract_torrent_name_from_info(const BencodeValue&);
    FileLayout extract_file_layout_from_info(const BencodeValue& document);
    std::vector<PieceHash> extract_piece_hashes(const BencodeValue& document, const FileLayout& layout);

    std::optional<ByteRange> locate_info(std::string_view);
    InfoHash compute_info_hash(std::string_view);
    std::string to_hex(const InfoHash&);
    
private:
    std::filesystem::path path;
    const static std::size_t hash_size_in_bytes = 20;

    const BencodeValue& require_key_value(const BencodeValue::Dict&, std::string_view);
    const BencodeValue& get_field_from_dict(const BencodeValue&, std::string_view);
    
    std::int64_t count_pieces(const FileLayout& layout);
};


#endif
