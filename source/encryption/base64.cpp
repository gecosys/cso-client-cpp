extern "C" {
    #include <b64/cencode.h>
    #include <b64/cdecode.h>
}
#include "encryption/base64.h"

std::string Base64::encode(const Array<uint8_t>& data) {
    base64_encodestate state;
    base64_init_encodestate(&state);

    Array<char> output{ base64_encode_length(data.length(), &state) + 1 };
    auto seek = base64_encode_block((const char*)data.get(), data.length(), output.get(), &state);
    base64_encode_blockend(output.get() + seek, &state);
    return std::string{ output.get(), output.length() - 1 };
}

Array<uint8_t> Base64::decode(const std::string& data) {
    uint8_t* output = new uint8_t[base64_decode_maxlength(data.length())];
    base64_decodestate state;

    base64_init_decodestate(&state);
    size_t lenOutput = base64_decode_block(data.c_str(), data.length(), (char*)output, &state);
    return Array<uint8_t>{ output, lenOutput };
}