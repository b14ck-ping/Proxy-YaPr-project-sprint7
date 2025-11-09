#include "headers.h"
#include <gtest/gtest.h>

// ---------- Helper ----------
static std::vector<std::pair<std::string, std::string>> collectHeaders(std::string_view s) {
    std::vector<std::pair<std::string, std::string>> out;
    iterHeaders(s, [&](std::string_view name, std::string_view value) {
        out.emplace_back(std::string(name), std::string(value));
    });
    return out;
}

TEST(iterHeaders, Empty) {
    std::string_view req = "";
    auto v = collectHeaders(req);
    EXPECT_TRUE(v.empty());
}

TEST(iterHeaders, SkipRequestLine) {
    std::string_view req = "GET / HTTP/1.1\r\n\r\n";
    auto v = collectHeaders(req);
    EXPECT_TRUE(v.empty());
}

TEST(iterHeaders, SingleHeader) {
    std::string_view req = "GET / HTTP/1.1\r\n"
                           "Host: example.com\r\n"
                           "\r\n";
    auto v = collectHeaders(req);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_TRUE(v[0].first == "host" || v[0].first == "Host");
    EXPECT_EQ(v[0].second, "example.com");
}

TEST(iterHeaders, MultipleHeaders) {
    std::string_view req = "POST /submit HTTP/1.1\r\n"
                           "Host: example.com\r\n"
                           "Content-Type: text/plain\r\n"
                           "X-Custom: value\r\n"
                           "\r\n";
    auto v = collectHeaders(req);
    ASSERT_EQ(v.size(), 3u);

    std::map<std::string, std::string> m;
    for (auto &p : v) {
        std::string key = p.first;
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
        m[key] = p.second;
    }
    EXPECT_EQ(m["host"], "example.com");
    EXPECT_EQ(m["content-type"], "text/plain");
    EXPECT_EQ(m["x-custom"], "value");
}

TEST(iterHeaders, MultipleSameHeaders) {
    std::string_view req = "GET / HTTP/1.1\r\n"
                           "Set-Cookie: a=1\r\n"
                           "Set-Cookie: b=2\r\n"
                           "\r\n";
    auto v = collectHeaders(req);
    ASSERT_EQ(v.size(), 2u);

    std::string n0 = v[0].first;
    std::transform(n0.begin(), n0.end(), n0.begin(), [](unsigned char c) { return std::tolower(c); });
    std::string n1 = v[1].first;
    std::transform(n1.begin(), n1.end(), n1.begin(), [](unsigned char c) { return std::tolower(c); });
    EXPECT_EQ(n0, "set-cookie");
    EXPECT_EQ(n1, "set-cookie");
    EXPECT_EQ(v[0].second, "a=1");
    EXPECT_EQ(v[1].second, "b=2");
}

TEST(findHostPort, Simple) {
    std::string_view req1 = "GET / HTTP/1.1\r\n"
                            "Host: example.com\r\n"
                            "\r\n";
    auto p1 = findHostPort(req1);
    EXPECT_EQ(p1.first, "example.com");
    EXPECT_EQ(p1.second, "80");

    std::string_view req2 = "GET / HTTP/1.1\r\n"
                            "Host: example.com:8080\r\n"
                            "\r\n";
    auto p2 = findHostPort(req2);
    EXPECT_EQ(p2.first, "example.com");
    EXPECT_EQ(p2.second, "8080");
}

TEST(findHostPort, NoHost) {
    std::string_view req = "GET / HTTP/1.1\r\n"
                           "User-Agent: test\r\n"
                           "\r\n";
    auto p = findHostPort(req);
    EXPECT_TRUE(p.first.empty());
    EXPECT_TRUE(p.second.empty());
}

TEST(findContentLength, Simple) {
    std::string_view rsp = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/plain\r\n"
                           "Content-Length: 123\r\n"
                           "\r\n";
    auto cl = findContentLength(rsp);
    ASSERT_TRUE(cl.has_value());
    EXPECT_EQ(cl.value(), 123u);
}

TEST(findContentLength, NoContentLength) {
    std::string_view rsp = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/plain\r\n"
                           "\r\n";
    auto cl = findContentLength(rsp);
    EXPECT_FALSE(cl.has_value());
}