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
    
WebSocketSession::WebSocketSession(
  tcp::socket socket,
  WebsocketOrchestrationNode & server)
: websocket_(std::move(socket)),
  server_(server)
{
}

void WebSocketSession::run(request_type request)
{
  websocket_.set_option(
    websocket::stream_base::timeout::suggested(beast::role_type::server));
  websocket_.set_option(
    websocket::stream_base::decorator(
      [](websocket::response_type & response) {
        response.set(http::field::server, "stocktake_orchestration2");
      }));

  websocket_.async_accept(
    request,
    beast::bind_front_handler(&WebSocketSession::on_accept, shared_from_this()));
}

void WebSocketSession::send_text(const std::string & message)
{
  auto self = shared_from_this();
  boost::asio::post(
    websocket_.get_executor(),
    [self, message]() {
      const bool write_in_progress = !self->write_queue_.empty();
      self->write_queue_.push_back(message);
      if (!write_in_progress) {
        self->do_write();
      }
    });
}

void WebSocketSession::on_accept(beast::error_code ec)
{
  if (ec) {
    server_.log_disconnect(ec);
    return;
  }

  server_.register_session(shared_from_this());
  do_read();
}

void WebSocketSession::do_read()
{
  websocket_.async_read(
    buffer_,
    beast::bind_front_handler(&WebSocketSession::on_read, shared_from_this()));
}

void WebSocketSession::on_read(beast::error_code ec, std::size_t)
{
  if (ec == websocket::error::closed) {
    close();
    return;
  }

  if (ec) {
    server_.log_disconnect(ec);
    close();
    return;
  }

  const std::string payload = beast::buffers_to_string(buffer_.data());
  buffer_.consume(buffer_.size());
  server_.handle_client_message(shared_from_this(), payload);
  do_read();
}

void WebSocketSession::do_write()
{
  websocket_.text(true);
  websocket_.async_write(
    boost::asio::buffer(write_queue_.front()),
    beast::bind_front_handler(&WebSocketSession::on_write, shared_from_this()));
}

void WebSocketSession::on_write(beast::error_code ec, std::size_t)
{
  if (ec) {
    server_.log_disconnect(ec);
    close();
    return;
  }

  write_queue_.pop_front();
  if (!write_queue_.empty()) {
    do_write();
  }
}

void WebSocketSession::close()
{
  server_.unregister_session(this);
}


}

