#include "../include/torrent_parser.h"

TorrentFileParser::TorrentFileParser(const std::filesystem::path& file_path) : path(file_path) {}

const BencodeValue& TorrentFileParser::require_key_value(const BencodeValue::Dict& dict, std::string_view key) {
    for (const auto& [name, value] : dict) {
        if (name == key) return value;
    }

    throw std::invalid_argument("Key " + std::string(key) + " does not exist");
}

std::string TorrentFileParser::read_binary_file() {
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


const BencodeValue& TorrentFileParser::get_field_from_dict(const BencodeValue& document, std::string_view key) {
    const auto* dict = std::get_if<BencodeValue::Dict>(&document.value);
    if (dict == nullptr)
        throw std::invalid_argument("Expected a dictionary while looking up '" + std::string(key) + "'"); 
    const BencodeValue& value = require_key_value(*dict, key);
    return value;
}

std::string TorrentFileParser::extract_torrent_name_from_info(const BencodeValue& document) {
    const BencodeValue& info_dict = get_field_from_dict(document, "info");
    const BencodeValue& name = get_field_from_dict(info_dict, "name");

    const auto* name_str = std::get_if<std::string>(&name.value);
    if (name_str == nullptr)
        throw std::invalid_argument("Field 'info.name' must be a string"); 
    return *name_str;
}


TorrentFileParser::FileLayout TorrentFileParser::extract_file_layout_from_info(const BencodeValue& document) {
    const BencodeValue& info_dict = get_field_from_dict(document, "info");
    
    const BencodeValue& length_bencode = get_field_from_dict(info_dict, "length");
    const auto* length = std::get_if<std::int64_t>(&length_bencode.value);
    if (length == nullptr)
        throw std::invalid_argument("Field 'info.length' must be an integer"); 

    else if (*length < 0) 
        throw std::invalid_argument("Field 'info.length' must be postive or zero");

    const BencodeValue& piece_length_bencode = get_field_from_dict(info_dict, "piece length");
    const auto* piece_length = std::get_if<std::int64_t>(&piece_length_bencode.value);
    if (piece_length == nullptr)
        throw std::invalid_argument("Field 'info.piece length' must be an integer"); 

    else if (*piece_length <= 0)
        throw std::invalid_argument("Field 'info.piece length must be positive");

    return FileLayout{*length, *piece_length};
}

std::int64_t TorrentFileParser::count_pieces(const TorrentFileParser::FileLayout& layout) {
    return layout.file_size / layout.piece_length + (layout.file_size % layout.piece_length > 0 ? 1 : 0);
}

std::vector<PieceHash> TorrentFileParser::extract_piece_hashes(
    const BencodeValue& document, 
    const TorrentFileParser::FileLayout& layout) 
{
    std::vector<PieceHash> pieces_hash;
    std::int64_t pieces_count = count_pieces(layout);

    const BencodeValue& info_dict = get_field_from_dict(document, "info");
    const BencodeValue& pieces_bencode = get_field_from_dict(info_dict, "pieces");
    const auto pieces_bencode_str = std::get_if<std::string>(&pieces_bencode.value);
    
    if (pieces_bencode_str == nullptr) {
        throw std::invalid_argument("Field 'info.pieces' must be a string");
    }

    std::string pieces = *pieces_bencode_str;

    if (pieces.size() % 20 != 0 || pieces.size() / 20 != pieces_count) {
        throw std::invalid_argument("Field 'info.pieces' must be divisible by twenty bytes");
    }

    std::string::iterator start = pieces.begin();
    for (int i = 0; i < pieces_count; ++i) {
        PieceHash arr;
        std::copy_n(start, hash_size_in_bytes, arr.begin());
        pieces_hash.emplace_back(arr);
        start += hash_size_in_bytes;
    }    

    return pieces_hash;
}

std::optional<ByteRange> TorrentFileParser::locate_info(std::string_view input) {
    std::optional<ByteRange> info_location;

    BencodeParser parser(input);
    info_location = parser.find_dict_value_range("info");
    return info_location;
}

InfoHash TorrentFileParser::compute_info_hash(std::string_view encoded_info) {
    InfoHash hash;
    unsigned int hash_len;

    if (EVP_Digest(
        encoded_info.data(), encoded_info.size(), hash.data(), &hash_len, EVP_sha1(), nullptr
    ) != 1) {
        throw std::runtime_error("Failed to hash value: " + std::string(encoded_info));
    }

    if (hash_len != hash_size_in_bytes) {
        throw std::runtime_error("Error in hash length");
    }

    return hash;
}

std::string TorrentFileParser::to_hex(const InfoHash& hash) {
    std::string_view digits = "0123456789abcdef"; 
    std::string hex_hash;
    for (unsigned char byte : hash) { 
        char low = digits[byte >> 4];
        char high = digits[byte & 0x0F];
        hex_hash += low;
        hex_hash += high;
    }
    return hex_hash;
}

int main(int argc, char* argv[]) {
    size_t pos = 0;
    // std::cout << parse_byte_string("3:cati7e", pos) << "\tpos: "<< pos <<  std::endl;
    auto path = "example.bin"; 
    auto torrent_parser = TorrentFileParser(path);
    std::string input = torrent_parser.read_binary_file();
    auto bencode_parser = BencodeParser(input);
    // auto res = parse_document(std::string_view(input));
    auto layout = torrent_parser.extract_file_layout_from_info(
        bencode_parser.parse_document()
    );

    std::cout << layout.file_size << " " << layout.piece_length << std::endl;
    // std::cout << count_pieces(layout) << std::endl;
    // print_value(res);
    std::vector<PieceHash> hashes = torrent_parser.extract_piece_hashes(bencode_parser.parse_document(), layout);
    for (auto arr: hashes) {
        std::string str (arr.data(), 20);
        
        std::cout << str << std::endl;
    }
    auto range = torrent_parser.locate_info(input);
        std::cout << range->begin << " " << range->end << std::endl;

    auto hash = torrent_parser.compute_info_hash(input.substr(range->begin, range->end - range->begin));
    std::cout << torrent_parser.to_hex(hash) << std::endl;
}   

