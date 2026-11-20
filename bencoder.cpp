#include <iostream>
#include <string>
#include <string_view>
#include <charconv>
#include <cstdint>

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
    pos = delim_pos + 1 + str_length;

    return result;
}

std::int64_t parse_integer(std::string_view input, std::size_t& pos) {
    size_t start_integer = input.find('i', pos);
    if (start_integer == std::string::npos)
        throw std::invalid_argument("Input sequence does not contain an integer literal");
    
    std::int64_t result;

    auto [ptr, ec] = std::from_chars(input.data() + start_integer + 1, input.data() + input.size(), result);

    if (ec != std::errc{} || *ptr != 'e')
        throw std::invalid_argument("Failed to parse integer");

    if (std::to_string(result).length() != ptr - input.data() - start_integer - 1)
        throw std::invalid_argument("Invalid format of integer");

    pos = (ptr - input.data()) + 1;
    return result;
}

int main(int argc, char* argv[]) {
    size_t pos = 0;
    std::cout << parse_integer("i03e", pos) << "\tpos: "<< pos <<  std::endl;
}