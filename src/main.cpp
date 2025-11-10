#include "headers.h"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <cstddef>
#include <iostream>
#include <string_view>

using boost::asio::async_read_until;
using boost::asio::awaitable;
using boost::asio::buffer;
using boost::asio::co_spawn;
using boost::asio::dynamic_buffer;
using boost::asio::io_service;
using boost::asio::transfer_at_least;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;
using boost::system::error_code;

constexpr std::string_view delimiter = "\r\n\r\n";

awaitable<void> session(tcp::socket client_socket, io_service &io_service) {
    try {
        std::string client_request;

        std::size_t n =
            co_await async_read_until(client_socket, dynamic_buffer(client_request), delimiter, use_awaitable);

        auto [host, port] = findHostPort(client_request);
        if (host.empty()) {
            const std::string bad =
                "HTTP/1.1 400 Bad Request\r\nContent-Length: 11\r\nConnection: close\r\n\r\nBad Request";
            co_await boost::asio::async_write(client_socket, buffer(bad), use_awaitable);
            client_socket.close();
            co_return;
        }

        tcp::resolver resolver(io_service);
        auto endpoints = co_await resolver.async_resolve(host, port, use_awaitable);

        tcp::socket server_socket(io_service);
        co_await boost::asio::async_connect(server_socket, endpoints, use_awaitable);

        co_await boost::asio::async_write(server_socket, buffer(client_request.data(), client_request.size()),
                                          use_awaitable);

        std::string server_response;
        std::size_t resp_header_bytes = co_await async_read_until(
            server_socket, boost::asio::dynamic_buffer(server_response), delimiter, use_awaitable);

        std::optional<size_t> content_length = findContentLength(server_response);

        co_await boost::asio::async_write(client_socket, buffer(server_response.data(), server_response.size()),
                                          use_awaitable);
        if (content_length) {
            size_t total = *content_length;
            size_t already = resp_header_bytes;
            size_t remaining = 0;
            if (total > already)
                remaining = total - already;
            else
                remaining = 0;

            std::array<char, 8192> buf;
            while (remaining > 0) {
                std::size_t to_read = static_cast<std::size_t>(std::min<size_t>(buf.size(), remaining));
                std::size_t n = co_await server_socket.async_read_some(buffer(buf.data(), to_read), use_awaitable);
                if (n == 0)
                    break;
                co_await boost::asio::async_write(client_socket, buffer(buf.data(), n), use_awaitable);
                remaining -= n;
            }
        }
        error_code ec;
        server_socket.shutdown(tcp::socket::shutdown_both, ec);
        server_socket.close(ec);

        client_socket.shutdown(tcp::socket::shutdown_both, ec);
        client_socket.close(ec);
    } catch (const std::exception &e) {
        std::cerr << "Session exception: " << e.what() << std::endl;
        error_code ec;
        client_socket.close(ec);
    }
}

class Server {
public:
    Server(io_service &io_service, short port)
        : io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port)), socket_(io_service) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(socket_, [this](error_code ec) {
            if (!ec) {
                tcp::socket client_socket = std::move(socket_);

                socket_ = tcp::socket(io_service_);

                co_spawn(io_service_, session(std::move(client_socket), io_service_), boost::asio::detached);
            } else {
                std::cerr << "Accept error: " << ec.message() << std::endl;
            }

            do_accept();
        });
    }

    io_service &io_service_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
};

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: proxy_server";
            std::cerr << " <listen_port>\n";
            return 1;
        }
        io_service io_service(1);
        Server server(io_service, std::atoi(argv[1]));
        io_service.run();

    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}
