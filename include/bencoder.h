#ifndef BENCODER_H_
#define BENCODER_H_

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
#include <filesystem>
#include <fstream>
#include <format>
#include <optional>
#include <memory>

struct ByteRange {
    std::size_t begin;
    std::size_t end;
};

class BencodeParser {
public:
    struct BencodeValue {
        using List = std::vector<BencodeValue>;
        using Dict = std::vector<std::pair<std::string, BencodeValue>>;
        std::variant<std::int64_t, std::string, List, Dict> value;
    };

    BencodeParser(std::string_view);
    
    BencodeValue parse_document();
    std::optional<ByteRange> find_dict_value_range(std::string_view key);
    void print_value(const BencodeValue&);

private:
    std::string_view input;
    size_t pos;

    std::string parse_byte_string();
    std::int64_t parse_integer();
 
    BencodeValue::List parse_list();
    BencodeValue::Dict parse_dictionary(
        std::optional<ByteRange>* dict_range = nullptr,
        std::string_view* root = nullptr);

    BencodeValue parse_value();
};

#endif