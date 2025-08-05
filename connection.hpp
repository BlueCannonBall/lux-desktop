#pragma once

#include "json_fwd.hpp"
#include <string>
#include <utility>

class ConnectionInfo {
public:
    std::string address;
    std::string password;
    unsigned int bitrate = 4000;
    bool client_side_mouse = true;
    bool view_only = false;
    bool verify_certs = true;

    ConnectionInfo() = default;
    ConnectionInfo(std::string address, std::string password, unsigned int bitrate = 4000, bool client_side_mouse = true, bool view_only = false, bool verify_certs = true):
        address(std::move(address)),
        password(std::move(password)),
        bitrate(bitrate),
        client_side_mouse(client_side_mouse),
        view_only(view_only),
        verify_certs(verify_certs) {}
    ConnectionInfo(const nlohmann::json& conn_json);

    nlohmann::json to_json() const;
};
