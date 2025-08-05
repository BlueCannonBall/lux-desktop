#include "connection.hpp"
#include "json.hpp"

using nlohmann::json;

ConnectionInfo::ConnectionInfo(const json& conn_json) {
    if (auto address_it = conn_json.find("address"); address_it != conn_json.end() && address_it->is_string()) {
        address = *address_it;
    }
    if (auto password_it = conn_json.find("password"); password_it != conn_json.end() && password_it->is_string()) {
        password = *password_it;
    }
    if (auto bitrate_it = conn_json.find("bitrate"); bitrate_it != conn_json.end() && bitrate_it->is_number_unsigned()) {
        bitrate = *bitrate_it;
    }
    if (auto client_side_mouse_it = conn_json.find("client_side_mouse"); client_side_mouse_it != conn_json.end() && client_side_mouse_it->is_boolean()) {
        client_side_mouse = *client_side_mouse_it;
    }
    if (auto view_only_it = conn_json.find("view_only"); view_only_it != conn_json.end() && view_only_it->is_boolean()) {
        view_only = *view_only_it;
    }
    if (auto verify_certs_it = conn_json.find("verify_certs"); verify_certs_it != conn_json.end() && verify_certs_it->is_boolean()) {
        verify_certs = *verify_certs_it;
    }
}

json ConnectionInfo::to_json() const {
    return {
        {"address", address},
        {"password", password},
        {"bitrate", bitrate},
        {"client_side_mouse", client_side_mouse},
        {"view_only", view_only},
        {"verify_certs", verify_certs},
    };
}
