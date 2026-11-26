#include "../include/bencoder.h"

BencodeParser::BencodeParser(std::string_view input) : 
    input(input), pos(0) {}

std::string BencodeParser::parse_byte_string() {
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

std::int64_t BencodeParser::parse_integer() {
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


BencodeParser::BencodeValue BencodeParser::parse_value() {
    std::size_t input_size = input.size();
    
    if (pos >= input_size)
        throw std::range_error("Position is out of bound");

    else if (input[pos] == 'i')
        return BencodeParser::BencodeValue{parse_integer()};   

    else if (input[pos] == 'l') 
        return BencodeParser::BencodeValue{parse_list()};

    else if (input[pos] >= '0' && input[pos] <= '9') 
        return BencodeParser::BencodeValue{parse_byte_string()}; 

    else if (input[pos] == 'd') 
        return BencodeParser::BencodeValue{parse_dictionary()};

    else 
        throw std::invalid_argument("Invalid format of bencode data type");
} 

BencodeParser::BencodeValue::Dict BencodeParser::parse_dictionary(
    std::optional<ByteRange>* dict_range,
    std::string_view* root)
{   
    BencodeValue::Dict result;
    std::string key;
    std::size_t input_size = input.size();
    ++pos;

    size_t start;

    while (pos < input_size) {
        if (input[pos] == 'e')
            break;
        
        try {
            key = parse_byte_string();
        } catch (std::invalid_argument& e) {
            throw std::invalid_argument("Dictionary key must be a string");
        } catch (std::range_error& e) {
            throw std::invalid_argument("Invalid format of key");
        }

        start = pos;
        
        BencodeValue value = parse_value();

        if (dict_range && root && key == *root) {
                *dict_range = ByteRange{start, pos};
            }
        
        if (!result.empty() && result.back().first == key) 
            throw std::invalid_argument("This key already exists");

        if (!result.empty() && result.back().first > key)
            throw std::range_error("Keys must follow in lexicographical order");

        result.emplace_back(std::make_pair(std::move(key), std::move(value)));
    }

    if (pos >= input_size || input[pos] != 'e') {
        throw std::invalid_argument("Invalid format of bencode list");
    }

    ++pos;


    return result;
}


BencodeParser::BencodeValue::List BencodeParser::parse_list() {
    BencodeParser::BencodeValue::List result;
    std::size_t input_size = input.size();
    
    if (pos >= input_size || input[pos] != 'l')
        throw std::invalid_argument("Input sequence does not contain a bencode format list");

    ++pos;

    while (pos < input_size) {
        if (input[pos] == 'e')
            break;

        result.emplace_back(parse_value());
    }

    if (pos >= input_size || input[pos] != 'e') {
        throw std::invalid_argument("Invalid format of bencode list");
    }
    ++pos;
    return result;
} 

BencodeParser::BencodeValue BencodeParser::parse_document() {
    BencodeParser::BencodeValue result;
    pos = 0;
    std::size_t input_size = input.size();

    result = parse_value();

    if (pos != input_size) 
        throw std::invalid_argument("Invalid format of bencode file");

    return result;
}

void BencodeParser::print_value(const BencodeParser::BencodeValue& item) {
    std::visit([this](const auto& value) {
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


std::optional<ByteRange> BencodeParser::find_dict_value_range(std::string_view key) {
    std::optional<ByteRange> range;
    pos = 0;
    

    BencodeValue::Dict dict = parse_dictionary(&range, &key);

    return range;
}
