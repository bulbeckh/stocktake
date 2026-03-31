#include "stocktake_orchestration2/websocket_orchestration_node.hpp"

#include <functional>
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

WebsocketOrchestrationNode::WebsocketOrchestrationNode()
: Node("stocktake_orchestration"),
  io_context_(1),
  acceptor_(io_context_),
  state_(WorkflowState::IDLE),
  paused_(false),
  explore_resume_enabled_(false),
  explore_resume_true_sent_(false)
{
  const auto host = declare_parameter<std::string>("host", "127.0.0.1");
  const auto port = declare_parameter<int>("port", 9002);

  const auto address = boost::asio::ip::make_address(host);
  const tcp::endpoint endpoint(address, static_cast<unsigned short>(port));

  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
  acceptor_.bind(endpoint);
  acceptor_.listen(boost::asio::socket_base::max_listen_connections);

  saved_map_base_path_ = "/tmp/stocktake_map";
  saved_map_image_path_ = saved_map_base_path_ + ".png";

  map_saver_client_ = create_client<nav2_msgs::srv::SaveMap>("/map_saver/save_map");
  generate_waypoint_graph_client_ =
    create_client<stocktake_nvidia_swagger_msgs::srv::GenerateWaypointGraph>(
    "/generate_waypoint_graph");
  explore_resume_publisher_ = create_publisher<std_msgs::msg::Bool>("/explore/resume", 10);
  explore_status_subscription_ = create_subscription<explore_lite_msgs::msg::ExploreStatus>(
    "/explore/status", 10,
    std::bind(&WebsocketOrchestrationNode::handle_explore_status, this, std::placeholders::_1));
  explore_resume_timer_ = create_wall_timer(
    std::chrono::seconds(1),
    std::bind(&WebsocketOrchestrationNode::publish_explore_resume_state, this));

  RCLCPP_INFO(
    get_logger(), "Starting websocket server on ws://%s:%d/ws", host.c_str(),
    static_cast<int>(port));
  do_accept();
  io_thread_ = std::thread([this]() {io_context_.run();});
}

WebsocketOrchestrationNode::~WebsocketOrchestrationNode()
{
  boost::asio::post(
    io_context_,
    [this]() {
      beast::error_code ec;
      acceptor_.cancel(ec);
      acceptor_.close(ec);
      for (const auto & session : sessions_) {
        session->send_text(make_error("Server shutting down."));
      }
      sessions_.clear();
    });

  io_context_.stop();
  if (io_thread_.joinable()) {
    io_thread_.join();
  }
}

void WebsocketOrchestrationNode::mark_mapping_complete()
{
  boost::asio::post(
    io_context_,
    [this]() {
      handle_mapping_complete_on_io_thread();
    });
}

void WebsocketOrchestrationNode::mark_route_construction_complete()
{
  boost::asio::post(
    io_context_,
    [this]() {
      handle_route_construction_complete_on_io_thread();
    });
}

void WebsocketOrchestrationNode::register_session(const std::shared_ptr<WebSocketSession> & session)
{
  sessions_.insert(session);
  RCLCPP_INFO(get_logger(), "New websocket connection");
  session->send_text(make_state_update_message());
}

void WebsocketOrchestrationNode::unregister_session(const WebSocketSession * session)
{
  for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
    if (it->get() == session) {
      sessions_.erase(it);
      RCLCPP_INFO(get_logger(), "Client disconnected");
      break;
    }
  }
}

void WebsocketOrchestrationNode::handle_client_message(
  const std::shared_ptr<WebSocketSession> & session,
  const std::string & payload)
{
  RCLCPP_INFO(get_logger(), "Incoming websocket message: %s", payload.c_str());

  const auto command_key = std::string{"\"command\""};
  const auto type_key = std::string{"\"type\""};
  const auto command_pos = payload.find(command_key);
  const auto type_pos = payload.find(type_key);

  if (type_pos == std::string::npos || command_pos == std::string::npos) {
    session->send_text(make_error("Invalid JSON payload."));
    return;
  }

  if (payload.find("\"type\":\"command\"") == std::string::npos &&
    payload.find("\"type\": \"command\"") == std::string::npos)
  {
    session->send_text(make_error("Unsupported message type."));
    return;
  }

  const auto colon_pos = payload.find(':', command_pos + command_key.size());
  const auto first_quote = payload.find('"', colon_pos + 1);
  const auto second_quote = payload.find('"', first_quote + 1);
  if (colon_pos == std::string::npos || first_quote == std::string::npos ||
    second_quote == std::string::npos)
  {
    session->send_text(make_error("Invalid JSON payload."));
    return;
  }

  const std::string command = payload.substr(first_quote + 1, second_quote - first_quote - 1);

  if (command == "start_mapping") {
    if (state_ != WorkflowState::IDLE) {
      session->send_text(make_command_ack(
          command, false, "A workflow is already active."));
      return;
    }
    start_mapping();
    session->send_text(make_command_ack(command, true));
    return;
  }

  if (command == "pause") {
    if (state_ == WorkflowState::IDLE || paused_) {
      session->send_text(make_command_ack(
          command, false, "Pause/resume is only available while mapping is active."));
      return;
    }
    pause_workflow();
    session->send_text(make_command_ack(command, true));
    return;
  }

  if (command == "resume") {
    if (state_ == WorkflowState::IDLE || !paused_) {
      session->send_text(make_command_ack(
          command, false, "Pause/resume is only available while mapping is active."));
      return;
    }
    resume_workflow();
    session->send_text(make_command_ack(command, true));
    return;
  }

  session->send_text(make_error("Unsupported command."));
}

WebsocketOrchestrationNode::response_type WebsocketOrchestrationNode::make_http_response(
  const request_type & request) const
{
  response_type response;

  if (request.method() != http::verb::get) {
    response.result(http::status::bad_request);
    response.version(request.version());
    response.set(http::field::content_type, "application/json");
    response.body() = make_error("Only GET is supported.");
    response.prepare_payload();
    return response;
  }

  if (request.target() != "/") {
    response.result(http::status::not_found);
    response.version(request.version());
    response.set(http::field::content_type, "application/json");
    response.body() = make_error("Not found.");
    response.prepare_payload();
    return response;
  }

  response.result(http::status::ok);
  response.version(request.version());
  response.set(http::field::content_type, "application/json");
  response.body() = make_healthcheck_body();
  response.prepare_payload();
  return response;
}

void WebsocketOrchestrationNode::log_disconnect(const beast::error_code & ec) const
{
  if (ec && ec != boost::asio::error::operation_aborted) {
    RCLCPP_INFO(get_logger(), "Connection event: %s", ec.message().c_str());
  }
}

void WebsocketOrchestrationNode::do_accept()
{
  acceptor_.async_accept(
    beast::bind_front_handler(&WebsocketOrchestrationNode::on_accept, this));
}

void WebsocketOrchestrationNode::on_accept(beast::error_code ec, tcp::socket socket)
{
  if (ec) {
    log_disconnect(ec);
  } else {
    std::make_shared<HttpSession>(std::move(socket), *this)->run();
  }

  if (acceptor_.is_open()) {
    do_accept();
  }
}

void WebsocketOrchestrationNode::start_mapping()
{
  transition_to(WorkflowState::MAPPING, false);
  RCLCPP_INFO(get_logger(), "State change: IDLE -> MAPPING");
  on_enter_mapping_from_idle();
}

void WebsocketOrchestrationNode::pause_workflow()
{
  transition_to(state_, true);
  RCLCPP_INFO(get_logger(), "Workflow paused");
}

void WebsocketOrchestrationNode::resume_workflow()
{
  transition_to(state_, false);
  RCLCPP_INFO(get_logger(), "Workflow resumed");
}

void WebsocketOrchestrationNode::handle_mapping_complete_on_io_thread()
{
  if (state_ != WorkflowState::MAPPING) {
    RCLCPP_WARN(
      get_logger(),
      "Ignoring mapping completion request because current state is %s",
      state_to_string(state_).c_str());
    return;
  }

  if (paused_) {
    RCLCPP_WARN(
      get_logger(),
      "Ignoring mapping completion request because the workflow is currently paused");
    return;
  }

  transition_to(WorkflowState::CONSTRUCTING_ROUTE, false);
  RCLCPP_INFO(get_logger(), "State change: MAPPING -> CONSTRUCTING_ROUTE");
  on_enter_constructing_route_from_mapping();
}

void WebsocketOrchestrationNode::handle_route_construction_complete_on_io_thread()
{
  if (state_ != WorkflowState::CONSTRUCTING_ROUTE) {
    RCLCPP_WARN(
      get_logger(),
      "Ignoring route construction completion request because current state is %s",
      state_to_string(state_).c_str());
    return;
  }

  if (paused_) {
    RCLCPP_WARN(
      get_logger(),
      "Ignoring route construction completion request because the workflow is currently paused");
    return;
  }

  transition_to(WorkflowState::IDLE, false);
  RCLCPP_INFO(get_logger(), "State change: CONSTRUCTING_ROUTE -> IDLE");
}

void WebsocketOrchestrationNode::transition_to(WorkflowState new_state, bool paused)
{
  const bool next_explore_resume_enabled = (new_state == WorkflowState::MAPPING && !paused);

  state_ = new_state;
  paused_ = (new_state == WorkflowState::IDLE) ? false : paused;
  explore_resume_enabled_.store(next_explore_resume_enabled);

  if (next_explore_resume_enabled) {
    if (!explore_resume_true_sent_.exchange(true)) {
      publish_explore_resume_once(true);
    }
  } else {
    explore_resume_true_sent_.store(false);
  }

  broadcast_state();
}

void WebsocketOrchestrationNode::publish_explore_resume_state()
{
  if (explore_resume_enabled_.load()) {
    return;
  }

  std_msgs::msg::Bool message;
  message.data = false;
  explore_resume_publisher_->publish(message);
}

void WebsocketOrchestrationNode::publish_explore_resume_once(bool enabled)
{
  std_msgs::msg::Bool message;
  message.data = enabled;
  explore_resume_publisher_->publish(message);
}

void WebsocketOrchestrationNode::handle_explore_status(
  const explore_lite_msgs::msg::ExploreStatus::SharedPtr message)
{
  RCLCPP_INFO(get_logger(), "Received /explore/status: %s", message->status.c_str());

  if (message->status == explore_lite_msgs::msg::ExploreStatus::EXPLORATION_COMPLETE) {
    mark_mapping_complete();
  }
}

void WebsocketOrchestrationNode::request_map_save()
{
  if (!map_saver_client_->service_is_ready()) {
    RCLCPP_WARN(get_logger(), "Map saver service /map_saver/save_map is not available");
    return;
  }

  auto request = std::make_shared<nav2_msgs::srv::SaveMap::Request>();
  request->map_topic = "/map";
  request->map_url = saved_map_base_path_;
  request->image_format = "png";
  request->map_mode = "trinary";
  request->free_thresh = 0.25F;
  request->occupied_thresh = 0.65F;

  RCLCPP_INFO(get_logger(), "Requesting map save to %s", saved_map_base_path_.c_str());
  map_saver_client_->async_send_request(
    request,
    std::bind(&WebsocketOrchestrationNode::handle_map_save_response, this, std::placeholders::_1));
}

void WebsocketOrchestrationNode::handle_map_save_response(
  rclcpp::Client<nav2_msgs::srv::SaveMap>::SharedFuture future)
{
  const auto response = future.get();
  if (!response->result) {
    RCLCPP_ERROR(get_logger(), "Map save request failed");
    return;
  }

  RCLCPP_INFO(
    get_logger(), "Map save complete, requesting waypoint graph generation from %s",
    saved_map_image_path_.c_str());
  request_waypoint_graph_generation(saved_map_image_path_);
}

void WebsocketOrchestrationNode::request_waypoint_graph_generation(const std::string & map_image_path)
{
  if (!generate_waypoint_graph_client_->service_is_ready()) {
    RCLCPP_WARN(get_logger(), "Waypoint graph service /generate_waypoint_graph is not available");
    return;
  }

  auto request =
    std::make_shared<stocktake_nvidia_swagger_msgs::srv::GenerateWaypointGraph::Request>();
  request->map_path = map_image_path;

  generate_waypoint_graph_client_->async_send_request(
    request,
    std::bind(
      &WebsocketOrchestrationNode::handle_generate_waypoint_graph_response, this,
      std::placeholders::_1));
}

void WebsocketOrchestrationNode::handle_generate_waypoint_graph_response(
  rclcpp::Client<stocktake_nvidia_swagger_msgs::srv::GenerateWaypointGraph>::SharedFuture future)
{
  const auto response = future.get();
  if (!response->success) {
    RCLCPP_ERROR(
      get_logger(), "Waypoint graph generation failed: %s", response->message.c_str());
    return;
  }

  RCLCPP_INFO(
    get_logger(), "Waypoint graph generation complete: %zu nodes, %zu edges",
    response->graph.nodes.size(), response->graph.edges.size());

  for (const auto & node : response->graph.nodes) {
    RCLCPP_INFO(
      get_logger(),
      "Graph node: id=%u world_x=%.3f world_y=%.3f pixel_x=%d pixel_y=%d node_type=%s",
      node.id, node.world_x, node.world_y, node.pixel_x, node.pixel_y, node.node_type.c_str());
  }

  for (const auto & edge : response->graph.edges) {
    RCLCPP_INFO(
      get_logger(),
      "Graph edge: source_id=%u target_id=%u weight=%.3f edge_type=%s",
      edge.source_id, edge.target_id, edge.weight, edge.edge_type.c_str());
  }

  mark_route_construction_complete();
}

void WebsocketOrchestrationNode::broadcast_state()
{
  const std::string message = make_state_update_message();
  for (const auto & session : sessions_) {
    session->send_text(message);
  }
}

std::string WebsocketOrchestrationNode::make_state_update_message() const
{
  return "{\"type\":\"state_update\",\"state\":\"" + state_to_string(state_) +
         "\",\"paused\":" + (paused_ ? "true" : "false") + "}";
}

std::string WebsocketOrchestrationNode::make_healthcheck_body() const
{
  return "{\"status\":\"ok\",\"state\":\"" + state_to_string(state_) +
         "\",\"paused\":" + (paused_ ? "true" : "false") + "}";
}

std::string WebsocketOrchestrationNode::make_command_ack(
  const std::string & command,
  bool accepted,
  const std::string & reason)
{
  std::string body = "{\"type\":\"command_ack\",\"command\":\"" + escape_json(command) +
    "\",\"status\":\"" + (accepted ? "accepted" : "rejected") + "\"";
  if (!accepted && !reason.empty()) {
    body += ",\"reason\":\"" + escape_json(reason) + "\"";
  }
  body += "}";
  return body;
}

std::string WebsocketOrchestrationNode::make_error(const std::string & message)
{
  return "{\"type\":\"error\",\"message\":\"" + escape_json(message) + "\"}";
}

std::string WebsocketOrchestrationNode::state_to_string(WorkflowState state)
{
  switch (state) {
    case WorkflowState::IDLE:
      return "IDLE";
    case WorkflowState::MAPPING:
      return "MAPPING";
    case WorkflowState::CONSTRUCTING_ROUTE:
      return "CONSTRUCTING_ROUTE";
    default:
      return "IDLE";
  }
}

std::string WebsocketOrchestrationNode::escape_json(const std::string & value)
{
  std::string escaped;
  escaped.reserve(value.size());
  for (const char ch : value) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped += ch;
        break;
    }
  }
  return escaped;
}

}  // namespace stocktake_orchestration2
