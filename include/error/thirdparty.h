#ifndef CSO_ERROR_THIRDPARTY_H
#define CSO_ERROR_THIRDPARTY_H

#include <string>
#include <cstdint>

class ThirdParty {
public:
	static std::string getOpensslError(int32_t code);
	static std::string getCurlError(int32_t code);
	static std::string getSocketError(int32_t code);
};

#endif // !CSO_ERROR_THIRDPARTY_H
