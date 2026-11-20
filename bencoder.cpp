#include <iostream>
#include <string>
#include <string_view>
#include <charconv>
#include <cstdint>
#include <variant>
#include <vector>
#include <cmath>
#include <type_traits>

struct BencodeValue {
    using List = std::vector<BencodeValue>;
    std::variant<std::int64_t, std::string, List> value;
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

BencodeValue parse_value(std::string_view, std::size_t&);

BencodeValue parse_value(std::string_view input, std::size_t& pos) {
    BencodeValue result;
    std::size_t input_size = input.size();
    
    if (pos >= input_size)
        throw std::range_error("Position is out of bound");

    else if (input[pos] == 'i')
        return BencodeValue{parse_integer(input, pos)};   

    else if (input[pos] == 'l') 
        return BencodeValue{parse_list(input, pos)};

    else if (input[pos] >= '0' && input[pos] <= '9') 
        return BencodeValue{parse_byte_string(input, pos)}; 

    else 
        throw std::invalid_argument("Invalid format of bencode data type");
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

        try {
        result.emplace_back(parse_value(input, cursor));
        } catch(const std::exception& e) {
            throw e;
        }
    }

    if (cursor >= input_size || input[cursor] != 'e') {
        throw std::invalid_argument("Invalid format of bencode list");
    }
    pos = cursor + 1;
    return result;
} 


void print_value(const BencodeValue& item) {
    std::visit([](const auto& value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, BencodeValue::List>) {
            std::cout << "[ ";
            for (const auto& child : value)
                print_value(child);
            std::cout << " ]" << std::endl;
        } else {
            std::cout << value << '\n';
        }
    }, item.value);
}

int main(int argc, char* argv[]) {
    size_t pos = 0;
    // std::cout << parse_byte_string("3:cati7e", pos) << "\tpos: "<< pos <<  std::endl;
    BencodeValue::List res = parse_list("ll3:cati7eeli5eee", pos);
    std::cout << "length: " << res.size() << std::endl;
    for (const auto& a : res) {
        print_value(a);
    }
}