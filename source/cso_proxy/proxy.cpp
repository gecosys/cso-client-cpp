#include <nlohmann/json.hpp>
#include "cso_proxy/proxy.h"
#include "entity/bigint.h"
#include "entity/http_client.h"
#include "encryption/base64.h"
#include "message/ticket.h"
#include "utils/utils_dh.h"
#include "utils/utils_aes.h"
#include "utils/utils_rsa.h"
#include "utils/utils_edian.h"
#include "utils/utils_general.hpp"

using json = nlohmann::json;

std::unique_ptr<IProxy> Proxy::build(std::unique_ptr<IConfig>&& config) {
    return std::unique_ptr<IProxy>(new Proxy{ std::forward<std::unique_ptr<IConfig>>(config) });
}

Proxy::Proxy(std::unique_ptr<IConfig>&& config) noexcept
    : config{ std::forward<std::unique_ptr<IConfig>>(config) } {}

std::tuple<Error, ServerKey> Proxy::exchangeKey() {
    std::string str_gKey;
    std::string str_nKey;
    std::string str_pubKey;
    std::string signature;
    {
        // Invoke API
        json j;
        {
            std::string body;
            {
                json js;
                js["project_id"] = this->config->getProjectID();
                js["unique_name"] = this->config->getConnectionName();
                body = js.dump();
            }

            // Build URL
            std::string url{ format("%s/exchange-key", this->config->getCSOAddress().c_str()) };

            // Send request
            HttpClient client;
            Error err;
            std::string resp;

            client.setHeader("Content-Type", "application/json");
            std::tie(err, resp) = client.post(url, body);
            if (!err.nil()) {
                return { std::move(err), ServerKey{} };
            }

            // Parse response
            j = json::parse(resp, nullptr, false);
            if (j.is_discarded()) {
                return { Error{ GET_FUNC_NAME(), "Parsing JSON failed" }, ServerKey{} };
            }
        }

        // Parse data
        try {
            int32_t serverCode = j["returncode"].get<int32_t>();
            if (serverCode != 1) {
                return {
                    Error{ GET_FUNC_NAME(), format("[Server] (%d)%s", serverCode, j["data"].get<std::string>().c_str()) },
                    ServerKey{}
                };
            }

            str_gKey = j["data"]["g_key"].get<std::string>();
            str_nKey = j["data"]["n_key"].get<std::string>();
            str_pubKey = j["data"]["pub_key"].get<std::string>();
            signature = j["data"]["sign"].get<std::string>();
        }
        catch (json::exception& e) {
            return {
                Error{ GET_FUNC_NAME(), format("Invalid JSON format: %s", e.what()) },
                ServerKey{}
            };
        }
    }

    // Verify DH keys
    Error err;
    bool valid;

    std::tie(err, valid) = verifyDHKeys(str_gKey, str_nKey, str_pubKey, signature);
    if (!err.nil()) {
        return { std::move(err), ServerKey{} };
    }

    if (!valid) {
        return { Error{ GET_FUNC_NAME(), "DH keys authentication failed" }, ServerKey{} };
    }

    // Build server-keys object
    BigInt gKey;
    BigInt nKey;
    BigInt pubKey;

    err = gKey.setString(str_gKey);
    if (!err.nil()) {
        return { std::move(err), ServerKey{} };
    }

    err = nKey.setString(str_nKey);
    if (!err.nil()) {
        return { std::move(err), ServerKey{} };
    }

    err = pubKey.setString(str_pubKey);
    if (!err.nil()) {
        return { std::move(err), ServerKey{} };
    }

    return { Error{}, ServerKey{ std::move(gKey), std::move(nKey), std::move(pubKey) } };
}

std::tuple<Error, ServerTicket> Proxy::registerConnection(const ServerKey& serverKey) {
    // Generate client private key
    BigInt clientPrivKey{ UtilsDH::generatePrivateKey() };

    // Calculate client public key
    std::string clientPubKey;
    {
        Error err;
        BigInt pubKey;

        std::tie(err, pubKey) = UtilsDH::calcPublicKey(serverKey.gKey, serverKey.nKey, clientPrivKey);
        if (!err.nil()) {
            return { std::move(err), ServerTicket{} };
        }

        std::tie(err, clientPubKey) = pubKey.toString();
        if (!err.nil()) {
            return { std::move(err), ServerTicket{} };
        }
    }

    Error err;
    Array<uint8_t> iv;
    Array<uint8_t> tag;
    Array<uint8_t> token;
    {
        // Calculate client secret key
        Array<uint8_t> secretKey;

        std::tie(err, secretKey) = UtilsDH::calcSecretKey(serverKey.nKey, clientPrivKey, serverKey.pubKey);
        if (!err.nil()) {
            return { std::move(err), ServerTicket{} };
        }

        // Encrypt client token
        std::tie(err, iv, tag, token) = buildEncyptToken(clientPubKey, secretKey);
        if (!err.nil()) {
            return { std::move(err), ServerTicket{} };
        }
    }

    // Get data
    Array<uint8_t> ticketToken;
    Array<uint8_t> aad;
    BigInt serverPubKey;
    std::string hubIP;
    uint16_t hubPort;
    uint16_t ticketID;
    {
        std::string strServerPubKey;
        std::string hubAddress;
        {
            // Invoke API
            json j;
            {
                // Build request body
                std::string body;
                {
                    std::string encodeIV = Base64::encode(iv);
                    std::string encodeTag = Base64::encode(tag);
                    std::string encodeToken = Base64::encode(token);

                    json js;
                    js["project_id"] = this->config->getProjectID();
                    js["project_token"] = encodeToken;
                    js["unique_name"] = this->config->getConnectionName();
                    js["public_key"] = clientPubKey;
                    js["iv"] = encodeIV;
                    js["authen_tag"] = encodeTag;
                    body = js.dump();

                    iv.release();
                    tag.release();
                    token.release();
                }

                // Build URL
                std::string url{ format("%s/register-connection", this->config->getCSOAddress().c_str()) };

                // Send request
                HttpClient client;
                std::string resp;

                client.setHeader("Content-Type", "application/json");
                std::tie(err, resp) = client.post(url, body);
                if (!err.nil()) {
                    return { std::move(err), ServerTicket{} };
                }

                // Parse response
                j = json::parse(resp, nullptr, false);
                if (j.is_discarded()) {
                    return { Error{ GET_FUNC_NAME(), "Parsing JSON failed" }, ServerTicket{} };
                }
            }

            // Parse response
            try {
                int32_t serverCode = j["returncode"].get<int32_t>();
                if (serverCode != 1) {
                    return {
                        Error{ GET_FUNC_NAME(), format("[Server] (%d)%s", serverCode, j["data"].get<std::string>().c_str()) },
                        ServerTicket{}
                    };
                }

                strServerPubKey = j["data"]["pub_key"].get<std::string>();
                hubAddress = j["data"]["hub_address"].get<std::string>();

                // Get ticket_id
                ticketID = j["data"]["ticket_id"].get<uint16_t>();

                // Decode base64 data
                iv = Base64::decode(j["data"]["iv"].get<std::string>());
                tag = Base64::decode(j["data"]["auth_tag"].get<std::string>());
                ticketToken = Base64::decode(j["data"]["ticket_token"].get<std::string>());
            }
            catch (json::exception& e) {
                return {
                    Error{ GET_FUNC_NAME(), format("Invalid JSON format: %s", e.what()) },
                    ServerTicket{}
                };
            }
        }

        // Build aad
        {
            auto t = ticketID;
            // Convert to little edian
            if (UtilsEdian::isBigEndian()) {
                t = UtilsEdian::swap2Bytes((int16_t)ticketID);
            }
            size_t lenHubAddress = hubAddress.length();
            size_t lenServerPubKey = strServerPubKey.length();

            aad.reset(2 + lenHubAddress + lenServerPubKey);
            memcpy(aad.get(), &t, 2);
            memcpy(aad.get() + 2, hubAddress.c_str(), lenHubAddress);
            memcpy(aad.get() + (2 + lenHubAddress), strServerPubKey.c_str(), lenServerPubKey);
        }

        // Parse hub_address string to hub_ip + hub_port
        {
            int8_t index = -1;
            for (auto idx = hubAddress.length() - 1; idx >= 0; --idx) {
                if (hubAddress[idx] == ':') {
                    index = idx;
                    break;
                }
            }
            if (index == -1) {
                return { Error{ GET_FUNC_NAME(), "Invalid hub address" }, ServerTicket{} };
            }

            hubIP.assign(hubAddress, 0, index);
            hubPort = atoi(hubAddress.c_str() + index + 1);
        }

        // Build server public key
        err = serverPubKey.setString(strServerPubKey);
        if (!err.nil()) {
            return { std::move(err), ServerTicket{} };
        }
    }

    // Build server secret key
    Array<uint8_t> serverSecretKey;
    std::tie(err, serverSecretKey) = UtilsDH::calcSecretKey(serverKey.nKey, clientPrivKey, serverPubKey);
    if (!err.nil()) {
        return { std::move(err), ServerTicket{} };
    }

    // Decrypt server token
    std::tie(err, token) = UtilsAES::decrypt(serverSecretKey, ticketToken, aad, iv, tag);
    if (!err.nil()) {
        return { std::move(err), ServerTicket{} };
    }

    // Parse server ticket token to bytes
    Array<uint8_t> ticket;
    std::tie(err, ticket) = Ticket::buildBytes(ticketID, token);
    if (!err.nil()) {
        return { std::move(err), ServerTicket{} };
    }

    // Done
    return {
        Error{},
        ServerTicket{
            std::move(hubIP),
            hubPort,
            ticketID,
            std::move(ticket),
            std::move(serverSecretKey)
        }
    };
}

std::tuple<Error, bool> Proxy::verifyDHKeys(const std::string& gKey, const std::string& nKey, const std::string& pubKey, const std::string& encodeSign) {
    // Build data
    Array<uint8_t> data;
    {
        size_t lenGKey = gKey.length();
        size_t lenNKey = nKey.length();
        size_t lenPubKey = pubKey.length();

        data.reset(lenGKey + lenNKey + lenPubKey);
        memcpy(data.get(), gKey.c_str(), lenGKey);
        memcpy(data.get() + lenGKey, nKey.c_str(), lenNKey);
        memcpy(data.get() + (lenGKey + lenNKey), pubKey.c_str(), lenPubKey);
    }

    // Verify
    return UtilsRSA::verifySignature(
        this->config->getCSOPublicKey(),
        Base64::decode(encodeSign),
        data
    );
}

std::tuple<Error, Array<uint8_t>, Array<uint8_t>, Array<uint8_t>> Proxy::buildEncyptToken(const std::string& clientPubKey, const Array<uint8_t>& secretKey) {
    // Build client aad
    Array<uint8_t> aad;
    {
        const std::string& pid = this->config->getProjectID();
        const std::string& cname = this->config->getConnectionName();

        size_t lenPid = pid.length();
        size_t lenCname = cname.length();
        size_t lenClientPubKey = clientPubKey.length();

        aad.reset(lenPid + lenCname + lenClientPubKey);
        memcpy(aad.get(), pid.c_str(), lenPid);
        memcpy(aad.get() + lenPid, cname.c_str(), lenCname);
        memcpy(aad.get() + (lenPid + lenCname), clientPubKey.c_str(), lenClientPubKey);
    }

    // AES encrypt token
    return UtilsAES::encrypt(secretKey, Base64::decode(this->config->getProjectToken()), aad);
}