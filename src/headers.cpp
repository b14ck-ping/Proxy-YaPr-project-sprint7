#include "headers.h"

#include <algorithm>
#include <ranges>
#include <string_view>

using namespace std::string_view_literals;
namespace rv = std::ranges::views;
namespace rs = std::ranges;

using Callback = std::function<void(std::string_view, std::string_view)>;

void iterHeaders(std::string_view req, Callback &&callback) {
    std::string_view delim{"\r\n"};
    auto lines = req | rv::split(delim) | rv::transform([](const auto &item) { return std::string_view(item); }) |
                 rs::to<std::vector<std::string_view>>();

    rs::for_each(lines, [&callback](std::string_view line) {
        std::string filtered = line | rv::filter([](char c) { return !std::isspace(c); }) | rs::to<std::string>();
        std::string_view filtered_view = filtered;
        auto pos = filtered_view.find(":");
        if (pos != std::string_view::npos) {
            std::string_view name = filtered_view.substr(0, pos);
            std::string_view val = filtered_view.substr(pos + 1);
            callback(name, val);
        }
    });
}

std::pair<std::string, std::string> findHostPort(std::string_view req) {
    std::string host{};
    std::string port{};

    iterHeaders(req, [&](std::string_view name, std::string_view val) {
        if (name == "Host") {
            auto pos = val.find(":");
            if (pos != std::string_view::npos) {
                host = val.substr(0, pos);
                port = val.substr(pos + 1);
            } else {
                host = val;
                port = "80";
            }
        }
    });
    return std::pair(host, port);
}

std::optional<size_t> findContentLength(std::string_view rsp) {
    std::optional<size_t> res{};

    iterHeaders(rsp, [&](std::string_view name, std::string_view val) {
        if (name == "Content-Length") {
            std::string v(val);
            try {
                size_t val = std::stoull(std::string(v));
                res = val;
            } catch (...) {
                // ignore parse errors
            }
        }
    });
    return res;
}
