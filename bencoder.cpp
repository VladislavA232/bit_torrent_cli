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
struct BencodeValue {
    using List = std::vector<BencodeValue>;
    using Dict = std::vector<std::pair<std::string, BencodeValue>>;
    std::variant<std::int64_t, std::string, List, Dict> value;
};

std::string parse_byte_string(std::string_view input, std::size_t& pos) {
    size_t delim_pos = input.find(':', pos);
    if (delim_pos == std::string::npos)
        throw std::invalid_argument("Input sequence does not contain a string literal");
    
    std::int64_t str_length = 0;
    auto [ptr, ec] = std::from_chars(input.data() + pos, input.data() + delim_pos, str_length);
    if (ec != std::errc{} || ptr != input.data() + delim_pos)
        throw std::invalid_argument("Failed to parse string length");

    if (str_length < 0 || delim_pos + 1 + str_length > input.size())
        throw std::out_of_range("String length is out of range");
    
    std::string result{input.substr(delim_pos + 1, str_length)};
    pos = delim_pos + str_length + std::min(1, 
        static_cast<int>(input.data() + input.size() - ptr));

    return result;
}

std::int64_t parse_integer(std::string_view input, std::size_t& pos) {
    std::size_t start_integer = input.find('i', pos);
    if (start_integer == std::string::npos)
        throw std::invalid_argument("Input sequence does not contain an integer literal");
    
    std::int64_t result;

    auto [ptr, ec] = std::from_chars(input.data() + start_integer + 1, input.data() + input.size(), result);

    if (ec != std::errc{} || *ptr != 'e')
        throw std::invalid_argument("Failed to parse integer");

    if (std::to_string(result).length() != ptr - input.data() - start_integer - 1)
        throw std::invalid_argument("Invalid format of bencode integer");

    
    pos = (ptr - input.data()) + std::min(1, 
        static_cast<int>(input.data() + input.size() - ptr));
    return result;
}

BencodeValue::List parse_list(std::string_view, std::size_t&);
BencodeValue::Dict parse_dictionary(std::string_view, std::size_t&);
BencodeValue parse_value(std::string_view, std::size_t&);


BencodeValue parse_value(std::string_view input, std::size_t& pos) {
    std::size_t input_size = input.size();
    
    if (pos >= input_size)
        throw std::range_error("Position is out of bound");

    else if (input[pos] == 'i')
        return BencodeValue{parse_integer(input, pos)};   

    else if (input[pos] == 'l') 
        return BencodeValue{parse_list(input, pos)};

    else if (input[pos] >= '0' && input[pos] <= '9') 
        return BencodeValue{parse_byte_string(input, pos)}; 

    else if (input[pos] == 'd') 
        return BencodeValue{parse_dictionary(input, pos)};

    else 
        throw std::invalid_argument("Invalid format of bencode data type");
} 

BencodeValue::Dict parse_dictionary(std::string_view input, std::size_t& pos) {
    BencodeValue::Dict result;
    std::string key;
    std::size_t input_size = input.size();
    size_t cursor = pos + 1;


    while (cursor < input_size) {
        if (input[cursor] == 'e')
            break;
        
        try {
            key = parse_byte_string(input, cursor);
        } catch (std::invalid_argument& e) {
            throw std::invalid_argument("Dictionary key must be a string");
        } catch (std::range_error& e) {
            throw std::invalid_argument("Invalid format of key");
        }

        BencodeValue value = parse_value(input, cursor);
        
        if (!result.empty() && result.back().first == key) 
            throw std::invalid_argument("This key already exists");

        if (!result.empty() && result.back().first > key)
            throw std::range_error("Keys must follow in lexicographical order");

        result.emplace_back(std::make_pair(std::move(key), std::move(value)));
    }

    if (cursor >= input_size || input[cursor] != 'e') {
        throw std::invalid_argument("Invalid format of bencode list");
    }

    pos = cursor + 1;
    return result;
}


BencodeValue::List parse_list(std::string_view input, std::size_t& pos) {
    BencodeValue::List result;
    std::size_t input_size = input.size();
    
    if (pos >= input_size || input[pos] != 'l')
        throw std::invalid_argument("Input sequence does not contain a bencode format list");

    size_t cursor = pos + 1;

    while (cursor < input_size) {
        if (input[cursor] == 'e')
            break;

        result.emplace_back(parse_value(input, cursor));
    }

    if (cursor >= input_size || input[cursor] != 'e') {
        throw std::invalid_argument("Invalid format of bencode list");
    }
    pos = cursor + 1;
    return result;
} 

BencodeValue parse_document(std::string_view input) {
    BencodeValue result;
    std::size_t pos = 0;
    std::size_t input_size = input.size();

    result = parse_value(input, pos);

    if (pos != input_size) 
        throw std::invalid_argument("Invalid format of bencode file");

    return result;
}

std::string read_binary_file(const std::filesystem::path& path) {
    std::ifstream fin(path, std::ios::binary | std::ios::ate);
    if (!fin.is_open()) {
        throw std::runtime_error(
         "Failed to open file " + std::string(path)
        );
    }

    auto file_size = fin.tellg();
    if (file_size == std::streampos(-1))
        throw std::runtime_error("Error tellg in a file " + std::string(path));

    std::string input(file_size, '\0');

    fin.seekg(0);
    if (!fin.good())
        throw std::runtime_error("Error seekg in a file " + std::string(path));


    fin.read(input.data(), file_size);
    
    fin.close();
    return input;
}

void print_value(const BencodeValue& item) {
    std::visit([](const auto& value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, BencodeValue::List>) {
            std::cout << "[ ";
            for (const auto& child : value)
                print_value(child);
            std::cout << " ]" << std::endl;
        }
        else if constexpr (std::is_same_v<Value, BencodeValue::Dict>) {
            std::cout << "{ ";
            for (const auto& pair : value) {
                std::cout << pair.first << ": ";
                print_value(pair.second);
            }
            std::cout << " }" << std::endl;
        }
        else {
            std::cout << value << '\n';
        }
    }, item.value);
}

const BencodeValue& require_key_value(const BencodeValue::Dict& dict, std::string_view key) {
    for (const auto& [name, value] : dict) {
        if (name == key) return value;
    }

    throw std::invalid_argument("Key " + std::string(key) + " does not exist");
}

const BencodeValue& get_field_from_dict(const BencodeValue& document, std::string_view key) {
    const auto* dict = std::get_if<BencodeValue::Dict>(&document.value);
    if (dict == nullptr)
        throw std::invalid_argument("Expected a dictionary while looking up '" + std::string(key) + "'"); 
    const BencodeValue& value = require_key_value(*dict, key);
    return value;
}

std::string extract_torrent_name(const BencodeValue& document) {
    const BencodeValue& info_dict = get_field_from_dict(document, "info");
    const BencodeValue& name = get_field_from_dict(info_dict, "name");

    const auto* name_str = std::get_if<std::string>(&name.value);
    if (name_str == nullptr)
        throw std::invalid_argument("Field 'info.name' must be a string"); 
    return *name_str;
}



int main(int argc, char* argv[]) {
    size_t pos = 0;
    // std::cout << parse_byte_string("3:cati7e", pos) << "\tpos: "<< pos <<  std::endl;
    std::string input = read_binary_file("example.bin");
    // auto res = parse_document(std::string_view(input));
    std::cout << extract_torrent_name(parse_document(input)) << std::endl;
    // print_value(res);
}