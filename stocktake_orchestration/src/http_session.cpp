#include "stocktake_orchestration2/websocket_orchestration_node.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iterator>
#include <optional>
#include <regex>
#include <sstream>
#include <type_traits>
#include <utility>

#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/system/error_code.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = boost::asio::ip::tcp;

namespace stocktake_orchestration2
{
HttpSession::HttpSession(
  tcp::socket socket,
  WebsocketOrchestrationNode & server)
: socket_(std::move(socket)),
  server_(server)
{
}

void HttpSession::run()
{
  do_read();
}

void HttpSession::do_read()
{
  request_ = {};
  http::async_read(
    socket_,
    buffer_,
    request_,
    beast::bind_front_handler(&HttpSession::on_read, shared_from_this()));
}

void HttpSession::on_read(beast::error_code ec, std::size_t)
{
  if (ec == http::error::end_of_stream) {
    socket_.shutdown(tcp::socket::shutdown_send, ec);
    return;
  }

  if (ec) {
    server_.log_disconnect(ec);
    return;
  }

  if (websocket::is_upgrade(request_) && request_.target() == "/ws") {
    std::make_shared<WebSocketSession>(std::move(socket_), server_)->run(std::move(request_));
    return;
  }

  send_response(server_.make_http_response(request_));
}

void HttpSession::send_response(response_type response)
{
  const bool close = response.need_eof();
  response_ = std::make_shared<response_type>(std::move(response));
  http::async_write(
    socket_,
    *response_,
    beast::bind_front_handler(&HttpSession::on_write, shared_from_this(), close));
}

void HttpSession::on_write(bool close, beast::error_code ec, std::size_t)
{
  if (ec) {
    server_.log_disconnect(ec);
    return;
  }

  if (close) {
    socket_.shutdown(tcp::socket::shutdown_send, ec);
    return;
  }

  response_.reset();
  do_read();
}

}

