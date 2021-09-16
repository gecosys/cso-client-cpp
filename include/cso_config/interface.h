#ifndef CSO_CONFIG_INTERFACE_H
#define CSO_CONFIG_INTERFACE_H

#include <string>

class IConfig {
public:
    virtual const std::string& getProjectID() noexcept = 0;
    virtual const std::string& getProjectToken() noexcept = 0;
    virtual const std::string& getConnectionName() noexcept = 0;
    virtual const std::string& getCSOPublicKey() noexcept = 0;
    virtual const std::string& getCSOAddress() noexcept = 0;
};

#endif // !CSO_CONFIG_INTERFACE_H