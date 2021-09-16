#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include "cso_config/config.h"

using json = nlohmann::json;

std::unique_ptr<IConfig> Config::build(const std::string& filePath) {
    json j;
    {
        // Rad all file
        std::ifstream file(filePath);
        std::stringstream buffer;
        buffer << file.rdbuf();

        // Parse json
        json::parse(buffer, nullptr, false);
        if (j.is_discarded()) {
            return std::unique_ptr<IConfig>(new Config{ "", "", "", "", "" });
        }
    }

    return std::unique_ptr<IConfig>(new Config{
        std::move(j["pid"].get<std::string>()),
        std::move(j["ptoken"].get<std::string>()),
        std::move(j["cname"].get<std::string>()),
        std::move(j["csopubkey"].get<std::string>()),
        std::move(j["csoaddr"].get<std::string>())
    });
}

std::unique_ptr<IConfig> Config::build(std::string&& projectID, std::string&& projectToken, std::string&& connectionName, std::string&& csoPubKey, std::string&& csoAddress) noexcept {
    return std::unique_ptr<IConfig>(new Config(
        std::forward<std::string>(projectID),
        std::forward<std::string>(projectToken),
        std::forward<std::string>(connectionName),
        std::forward<std::string>(csoPubKey),
        std::forward<std::string>(csoAddress)
    ));
}

Config::Config(
    std::string&& projectID,
    std::string&& projectToken,
    std::string&& connectionName,
    std::string&& csoPubKey,
    std::string&& csoAddress
) noexcept
    : projectID{ projectID },
      projectToken{ projectToken },
      connectionName{ connectionName },
      csoPubKey{ csoPubKey },
      csoAddress{ csoAddress } {}

const std::string& Config::getProjectID() noexcept {
    return this->projectID;
}

const std::string& Config::getProjectToken() noexcept {
    return this->projectToken;
}

const std::string& Config::getConnectionName() noexcept {
    return this->connectionName;
}

const std::string& Config::getCSOPublicKey() noexcept {
    return this->csoPubKey;
}

const std::string& Config::getCSOAddress() noexcept {
    return this->csoAddress;
}