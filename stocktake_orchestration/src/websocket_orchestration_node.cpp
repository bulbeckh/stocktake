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

namespace
{

namespace fs = std::filesystem;

struct SavedMapMetadata
{
  double resolution;
  double origin_x;
  double origin_y;
  double origin_yaw;
  uint32_t image_width;
  uint32_t image_height;
};

template<typename RequestT, typename = void>
struct has_output_dir : std::false_type {};

template<typename RequestT>
struct has_output_dir<RequestT, std::void_t<decltype(std::declval<RequestT>().output_dir)>>
: std::true_type {};

template<typename RequestT>
void set_output_dir_if_supported(RequestT & request, const std::string & output_dir)
{
  if constexpr (has_output_dir<RequestT>::value) {
    request.output_dir = output_dir;
  } else {
    (void)request;
    (void)output_dir;
  }
}

std::string trim_copy(const std::string & value)
{
  auto first = value.begin();
  while (first != value.end() && std::isspace(static_cast<unsigned char>(*first))) {
    ++first;
  }

  auto last = value.end();
  while (last != first && std::isspace(static_cast<unsigned char>(*(last - 1)))) {
    --last;
  }

  return std::string(first, last);
}

bool parse_yaml_scalar(
  const std::string & line,
  const std::string & key,
  double & value)
{
  const std::string prefix = key + ":";
  if (line.rfind(prefix, 0) != 0) {
    return false;
  }

  value = std::stod(trim_copy(line.substr(prefix.size())));
  return true;
}

bool parse_yaml_origin(
  const std::string & line,
  double & origin_x,
  double & origin_y,
  double & origin_yaw)
{
  const std::string prefix = "origin:";
  if (line.rfind(prefix, 0) != 0) {
    return false;
  }

  auto origin_text = trim_copy(line.substr(prefix.size()));
  if (!origin_text.empty() && origin_text.front() == '[') {
    origin_text.erase(origin_text.begin());
  }
  if (!origin_text.empty() && origin_text.back() == ']') {
    origin_text.pop_back();
  }

  std::replace(origin_text.begin(), origin_text.end(), ',', ' ');
  std::istringstream stream(origin_text);
  stream >> origin_x >> origin_y >> origin_yaw;
  return !stream.fail();
}

std::optional<std::string> extract_json_string(
  const std::string & payload,
  const std::string & key)
{
  const auto key_pos = payload.find("\"" + key + "\"");
  if (key_pos == std::string::npos) {
    return std::nullopt;
  }

  const auto colon_pos = payload.find(':', key_pos);
  if (colon_pos == std::string::npos) {
    return std::nullopt;
  }

  const auto first_quote = payload.find('"', colon_pos + 1);
  if (first_quote == std::string::npos) {
    return std::nullopt;
  }

  std::string value;
  bool escaping = false;
  for (auto index = first_quote + 1; index < payload.size(); ++index) {
    const char ch = payload[index];
    if (escaping) {
      switch (ch) {
        case '"':
        case '\\':
        case '/':
          value += ch;
          break;
        case 'n':
          value += '\n';
          break;
        case 'r':
          value += '\r';
          break;
        case 't':
          value += '\t';
          break;
        default:
          value += ch;
          break;
      }
      escaping = false;
      continue;
    }

    if (ch == '\\') {
      escaping = true;
      continue;
    }

    if (ch == '"') {
      return value;
    }

    value += ch;
  }

  return std::nullopt;
}

std::optional<std::array<uint32_t, 2>> read_png_dimensions(const std::string & image_path)
{
  std::ifstream stream(image_path, std::ios::binary);
  if (!stream) {
    return std::nullopt;
  }

  std::array<unsigned char, 24> header{};
  stream.read(reinterpret_cast<char *>(header.data()), static_cast<std::streamsize>(header.size()));
  if (stream.gcount() != static_cast<std::streamsize>(header.size())) {
    return std::nullopt;
  }

  constexpr std::array<unsigned char, 8> png_signature{
    0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  for (std::size_t i = 0; i < png_signature.size(); ++i) {
    if (header[i] != png_signature[i]) {
      return std::nullopt;
    }
  }

  const auto read_u32_be = [&header](std::size_t offset) {
      return (static_cast<uint32_t>(header[offset]) << 24) |
             (static_cast<uint32_t>(header[offset + 1]) << 16) |
             (static_cast<uint32_t>(header[offset + 2]) << 8) |
             static_cast<uint32_t>(header[offset + 3]);
    };

  return std::array<uint32_t, 2>{read_u32_be(16), read_u32_be(20)};
}

std::optional<SavedMapMetadata> load_saved_map_metadata(
  const std::string & metadata_path,
  const std::string & image_path)
{
  std::ifstream metadata_stream(metadata_path);
  if (!metadata_stream) {
    return std::nullopt;
  }

  SavedMapMetadata metadata{};
  bool has_resolution = false;
  bool has_origin = false;

  try {
    std::string line;
    while (std::getline(metadata_stream, line)) {
      line = trim_copy(line);
      if (line.empty() || line.front() == '#') {
        continue;
      }

      if (parse_yaml_scalar(line, "resolution", metadata.resolution)) {
        has_resolution = true;
        continue;
      }

      if (parse_yaml_origin(line, metadata.origin_x, metadata.origin_y, metadata.origin_yaw)) {
        has_origin = true;
        continue;
      }
    }
  } catch (const std::exception &) {
    return std::nullopt;
  }

  const auto dimensions = read_png_dimensions(image_path);
  if (!has_resolution || !has_origin || !dimensions.has_value() || metadata.resolution <= 0.0) {
    return std::nullopt;
  }

  metadata.image_width = (*dimensions)[0];
  metadata.image_height = (*dimensions)[1];
  return metadata;
}

bool image_pixel_to_nav2_map_coordinates(
  int32_t image_x,
  int32_t image_y,
  const SavedMapMetadata & metadata,
  double & map_x,
  double & map_y)
{
  if (image_x < 0 || image_y < 0 ||
    static_cast<uint32_t>(image_x) >= metadata.image_width ||
    static_cast<uint32_t>(image_y) >= metadata.image_height)
  {
    return false;
  }

  const double local_x = (static_cast<double>(image_x) + 0.5) * metadata.resolution;
  const double local_y =
    (static_cast<double>(metadata.image_height) - static_cast<double>(image_y) - 0.5) *
    metadata.resolution;
  const double cos_yaw = std::cos(metadata.origin_yaw);
  const double sin_yaw = std::sin(metadata.origin_yaw);

  map_x = metadata.origin_x + cos_yaw * local_x - sin_yaw * local_y;
  map_y = metadata.origin_y + sin_yaw * local_x + cos_yaw * local_y;
  return true;
}

}  // namespace

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
  tf_buffer_(get_clock()),
  tf_listener_(tf_buffer_, this, false),
  state_(WorkflowState::IDLE),
  paused_(false),
  has_stored_waypoint_graph_(false),
  navigation_current_world_x_(0.0),
  navigation_current_world_y_(0.0)
{
  const auto host = declare_parameter<std::string>("host", "127.0.0.1");
  const auto port = declare_parameter<int>("port", 9002);
  maps_directory_ = declare_parameter<std::string>("maps_directory", "/maps");
  map_server_load_service_name_ = declare_parameter<std::string>(
    "map_server_load_service", "/map_server/load_map");
  mapping_backend_pause_service_name_ = declare_parameter<std::string>(
    "mapping_backend_pause_service", "/slam_toolbox/pause_new_measurements");

  const auto address = boost::asio::ip::make_address(host);
  const tcp::endpoint endpoint(address, static_cast<unsigned short>(port));

  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
  acceptor_.bind(endpoint);
  acceptor_.listen(boost::asio::socket_base::max_listen_connections);

  active_map_id_.clear();
  active_map_directory_.clear();
  saved_map_base_path_ = "/tmp/stocktake_map";
  saved_map_image_path_ = saved_map_base_path_ + ".png";
  saved_map_metadata_path_ = saved_map_base_path_ + ".yaml";

  explore_client_ = rclcpp_action::create_client<Explore>(this, "/explore");
  navigate_to_pose_client_ = rclcpp_action::create_client<NavigateToPose>(
    this, "/navigate_to_pose");
  map_saver_client_ = create_client<nav2_msgs::srv::SaveMap>("/map_saver/save_map");
  map_loader_client_ = create_client<nav2_msgs::srv::LoadMap>(map_server_load_service_name_);
  if (!mapping_backend_pause_service_name_.empty()) {
    mapping_backend_pause_client_ =
      create_client<slam_toolbox::srv::Pause>(mapping_backend_pause_service_name_);
  }
  generate_waypoint_graph_client_ =
    create_client<stocktake_nvidia_swagger_msgs::srv::GenerateWaypointGraph>(
    "/generate_waypoint_graph");

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

void WebsocketOrchestrationNode::mark_navigation_complete()
{
  boost::asio::post(
    io_context_,
    [this]() {
      handle_navigation_complete_on_io_thread();
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

  if (command == "list_maps") {
    session->send_text(make_maps_list_message());
    return;
  }

  if (command == "select_map") {
    if (state_ != WorkflowState::IDLE) {
      session->send_text(make_command_ack(
          command, false, "Maps can only be selected while the workflow is idle."));
      return;
    }

    const auto map_id = extract_json_string(payload, "map_id");
    if (!map_id.has_value() || map_id->empty()) {
      session->send_text(make_command_ack(command, false, "Missing map_id."));
      return;
    }

    const auto previous_graph = stored_waypoint_graph_;
    const bool previous_has_graph = has_stored_waypoint_graph_;
    const std::string previous_map_id = active_map_id_;
    const std::string previous_map_directory = active_map_directory_;
    const std::string previous_map_base_path = saved_map_base_path_;
    const std::string previous_map_image_path = saved_map_image_path_;
    const std::string previous_map_metadata_path = saved_map_metadata_path_;

    const auto restore_previous_map = [this, previous_graph, previous_has_graph, previous_map_id,
        previous_map_directory, previous_map_base_path, previous_map_image_path,
        previous_map_metadata_path]() {
        stored_waypoint_graph_ = previous_graph;
        has_stored_waypoint_graph_ = previous_has_graph;
        active_map_id_ = previous_map_id;
        active_map_directory_ = previous_map_directory;
        saved_map_base_path_ = previous_map_base_path;
        saved_map_image_path_ = previous_map_image_path;
        saved_map_metadata_path_ = previous_map_metadata_path;
      };

    if (!load_stored_map(*map_id)) {
      session->send_text(make_command_ack(command, false, "Failed to load stored map."));
      return;
    }

    if (!disable_mapping_backend()) {
      restore_previous_map();
      session->send_text(make_command_ack(command, false, "Failed to disable mapping backend."));
      return;
    }

    if (!load_map_into_map_server(saved_map_metadata_path_)) {
      restore_previous_map();
      session->send_text(make_command_ack(command, false, "Failed to load map into map server."));
      return;
    }

    session->send_text(make_command_ack(command, true));
    session->send_text(make_map_selected_message(*map_id));
    return;
  }

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

  if (command == "start_stocktake") {
    if (state_ != WorkflowState::IDLE) {
      session->send_text(make_command_ack(
          command, false, "Stocktake can only start while the workflow is idle."));
      return;
    }
    if (!has_stored_graph()) {
      session->send_text(make_command_ack(
          command, false, "No waypoint graph is available. Construct a route first."));
      return;
    }
    start_navigation();
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

void WebsocketOrchestrationNode::start_navigation()
{
  transition_to(WorkflowState::NAVIGATING, false);
  RCLCPP_INFO(get_logger(), "State change: IDLE -> NAVIGATING");
  on_enter_navigating_from_idle();
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

void WebsocketOrchestrationNode::handle_navigation_complete_on_io_thread()
{
  if (state_ != WorkflowState::NAVIGATING) {
    RCLCPP_WARN(
      get_logger(),
      "Ignoring navigation completion request because current state is %s",
      state_to_string(state_).c_str());
    return;
  }

  if (paused_) {
    RCLCPP_WARN(
      get_logger(),
      "Ignoring navigation completion request because the workflow is currently paused");
    return;
  }

  transition_to(WorkflowState::IDLE, false);
  RCLCPP_INFO(get_logger(), "State change: NAVIGATING -> IDLE");
}

void WebsocketOrchestrationNode::transition_to(WorkflowState new_state, bool paused)
{
  state_ = new_state;
  paused_ = (new_state == WorkflowState::IDLE) ? false : paused;

  broadcast_state();
}

void WebsocketOrchestrationNode::send_explore_goal()
{
  if (!explore_client_->wait_for_action_server(std::chrono::seconds(0))) {
    RCLCPP_ERROR(get_logger(), "Explore action server /explore is not available");
    return_mapping_to_idle();
    return;
  }

  Explore::Goal goal;
  goal.return_to_init = false;

  rclcpp_action::Client<Explore>::SendGoalOptions options;
  options.goal_response_callback =
    std::bind(
    &WebsocketOrchestrationNode::handle_explore_goal_response, this, std::placeholders::_1);
  options.feedback_callback =
    std::bind(
    &WebsocketOrchestrationNode::handle_explore_feedback, this, std::placeholders::_1,
    std::placeholders::_2);
  options.result_callback =
    std::bind(&WebsocketOrchestrationNode::handle_explore_result, this, std::placeholders::_1);

  RCLCPP_INFO(get_logger(), "Sending Explore goal to /explore");
  explore_client_->async_send_goal(goal, options);
}

void WebsocketOrchestrationNode::handle_explore_goal_response(
  const ExploreGoalHandle::SharedPtr & goal_handle)
{
  if (!goal_handle) {
    RCLCPP_ERROR(get_logger(), "Explore goal was rejected");
    return_mapping_to_idle();
    return;
  }

  RCLCPP_INFO(get_logger(), "Explore goal accepted");
}

void WebsocketOrchestrationNode::handle_explore_feedback(
  ExploreGoalHandle::SharedPtr,
  const std::shared_ptr<const Explore::Feedback> feedback)
{
  RCLCPP_INFO(
    get_logger(),
    "Explore feedback: status=%s frontier_count_discovered=%u frontier_count_blacklisted=%u",
    feedback->status.status.c_str(),
    feedback->frontier_count_discovered,
    feedback->frontier_count_blacklisted);
}

void WebsocketOrchestrationNode::handle_explore_result(
  const ExploreGoalHandle::WrappedResult & result)
{
  switch (result.code) {
    case rclcpp_action::ResultCode::SUCCEEDED:
      if (result.result->success) {
        RCLCPP_INFO(
          get_logger(),
          "Explore action succeeded: status=%s message=%s frontier_count_visited=%u",
          result.result->status.status.c_str(),
          result.result->message.c_str(),
          result.result->frontier_count_visited);
        mark_mapping_complete();
        return;
      } else {
        RCLCPP_WARN(
          get_logger(),
          "Explore action completed unsuccessfully: status=%s message=%s frontier_count_visited=%u",
          result.result->status.status.c_str(),
          result.result->message.c_str(),
          result.result->frontier_count_visited);
      }
      break;
    case rclcpp_action::ResultCode::ABORTED:
      RCLCPP_ERROR(get_logger(), "Explore action aborted");
      break;
    case rclcpp_action::ResultCode::CANCELED:
      RCLCPP_ERROR(get_logger(), "Explore action canceled");
      break;
    default:
      RCLCPP_ERROR(get_logger(), "Explore action returned unknown result");
      break;
  }

  return_mapping_to_idle();
}

void WebsocketOrchestrationNode::return_mapping_to_idle()
{
  boost::asio::post(
    io_context_,
    [this]() {
      if (state_ != WorkflowState::MAPPING) {
        return;
      }

      transition_to(WorkflowState::IDLE, false);
      RCLCPP_INFO(get_logger(), "State change: MAPPING -> IDLE");
    });
}

void WebsocketOrchestrationNode::request_map_save()
{
  if (!prepare_new_map_directory()) {
    RCLCPP_ERROR(get_logger(), "Failed to prepare map output directory under %s", maps_directory_.c_str());
    boost::asio::post(
      io_context_,
      [this]() {
        if (state_ == WorkflowState::CONSTRUCTING_ROUTE) {
          transition_to(WorkflowState::IDLE, false);
          RCLCPP_INFO(get_logger(), "State change: CONSTRUCTING_ROUTE -> IDLE");
        }
      });
    return;
  }

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
  set_output_dir_if_supported(*request, active_map_directory_);

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

  const auto map_metadata = load_saved_map_metadata(saved_map_metadata_path_, saved_map_image_path_);
  if (!map_metadata.has_value()) {
    RCLCPP_ERROR(
      get_logger(),
      "Failed to load saved map metadata from %s and image dimensions from %s",
      saved_map_metadata_path_.c_str(),
      saved_map_image_path_.c_str());
    boost::asio::post(
      io_context_,
      [this]() {
        if (state_ == WorkflowState::CONSTRUCTING_ROUTE) {
          transition_to(WorkflowState::IDLE, false);
          RCLCPP_INFO(get_logger(), "State change: CONSTRUCTING_ROUTE -> IDLE");
        }
      });
    return;
  }

  RCLCPP_INFO(
    get_logger(),
    "Converting swagger graph pixels to Nav2 map frame using resolution=%.3f origin=(%.3f, %.3f, %.3f) image=%ux%u",
    map_metadata->resolution,
    map_metadata->origin_x,
    map_metadata->origin_y,
    map_metadata->origin_yaw,
    map_metadata->image_width,
    map_metadata->image_height);

  stored_waypoint_graph_.nodes_by_id.clear();
  stored_waypoint_graph_.adjacency_list.clear();

  for (const auto & node : response->graph.nodes) {
    double nav2_map_x = 0.0;
    double nav2_map_y = 0.0;
    const int32_t image_x = node.pixel_y;
    const int32_t image_y = node.pixel_x;
    if (!image_pixel_to_nav2_map_coordinates(
        image_x, image_y, *map_metadata, nav2_map_x, nav2_map_y))
    {
      RCLCPP_ERROR(
        get_logger(),
        "Rejecting waypoint graph because node id=%u has out-of-bounds swagger pixel row=%d column=%d for image %ux%u",
        node.id,
        node.pixel_x,
        node.pixel_y,
        map_metadata->image_width,
        map_metadata->image_height);
      stored_waypoint_graph_.nodes_by_id.clear();
      stored_waypoint_graph_.adjacency_list.clear();
      boost::asio::post(
        io_context_,
        [this]() {
          if (state_ == WorkflowState::CONSTRUCTING_ROUTE) {
            transition_to(WorkflowState::IDLE, false);
            RCLCPP_INFO(get_logger(), "State change: CONSTRUCTING_ROUTE -> IDLE");
          }
        });
      return;
    }

    RCLCPP_INFO(
      get_logger(),
      "Graph node: id=%u swagger_world=(%.3f, %.3f) nav2_map=(%.3f, %.3f) swagger_pixel=(row=%d, column=%d) image_pixel=(x=%d, y=%d) node_type=%s",
      node.id,
      node.world_x,
      node.world_y,
      nav2_map_x,
      nav2_map_y,
      node.pixel_x,
      node.pixel_y,
      image_x,
      image_y,
      node.node_type.c_str());

    stored_waypoint_graph_.nodes_by_id.emplace(
      node.id,
      StoredWaypointNode{
        node.id,
        nav2_map_x,
        nav2_map_y,
        node.pixel_x,
        node.pixel_y,
        node.node_type});
    stored_waypoint_graph_.adjacency_list.try_emplace(node.id);
  }

  for (const auto & edge : response->graph.edges) {
    RCLCPP_INFO(
      get_logger(),
      "Graph edge: source_id=%u target_id=%u weight=%.3f edge_type=%s",
      edge.source_id, edge.target_id, edge.weight, edge.edge_type.c_str());
    stored_waypoint_graph_.adjacency_list[edge.source_id].push_back(
      TraversalGraphEdge{
        edge.target_id,
        edge.weight,
        edge.edge_type});
  }
  has_stored_waypoint_graph_ = true;

  if (!persist_current_map_artifacts(response->graph.nodes.size(), response->graph.edges.size())) {
    RCLCPP_ERROR(get_logger(), "Failed to persist generated map artifacts");
    has_stored_waypoint_graph_ = false;
    stored_waypoint_graph_.nodes_by_id.clear();
    stored_waypoint_graph_.adjacency_list.clear();
    boost::asio::post(
      io_context_,
      [this]() {
        if (state_ == WorkflowState::CONSTRUCTING_ROUTE) {
          transition_to(WorkflowState::IDLE, false);
          RCLCPP_INFO(get_logger(), "State change: CONSTRUCTING_ROUTE -> IDLE");
        }
      });
    return;
  }

  mark_route_construction_complete();
}

bool WebsocketOrchestrationNode::prepare_new_map_directory()
{
  try {
    fs::create_directories(maps_directory_);

    const auto now = std::chrono::system_clock::now();
    const auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
      now.time_since_epoch()).count();

    for (int attempt = 0; attempt < 100; ++attempt) {
      const std::string candidate_id =
        (attempt == 0) ? "map_" + std::to_string(timestamp) :
        "map_" + std::to_string(timestamp) + "_" + std::to_string(attempt);
      const fs::path candidate_dir = fs::path(maps_directory_) / candidate_id;
      if (fs::exists(candidate_dir)) {
        continue;
      }

      fs::create_directories(candidate_dir);
      active_map_id_ = candidate_id;
      active_map_directory_ = candidate_dir.string();
      saved_map_base_path_ = (candidate_dir / "map").string();
      saved_map_image_path_ = (candidate_dir / "map.png").string();
      saved_map_metadata_path_ = (candidate_dir / "map.yaml").string();

      RCLCPP_INFO(
        get_logger(),
        "Prepared map output directory %s",
        active_map_directory_.c_str());
      return true;
    }
  } catch (const std::exception & ex) {
    RCLCPP_ERROR(get_logger(), "Failed to create map directory: %s", ex.what());
    return false;
  }

  RCLCPP_ERROR(get_logger(), "Failed to choose a unique map directory name");
  return false;
}

bool WebsocketOrchestrationNode::persist_current_map_artifacts(
  std::size_t node_count,
  std::size_t edge_count) const
{
  if (active_map_id_.empty() || active_map_directory_.empty()) {
    return false;
  }

  try {
    const fs::path map_dir(active_map_directory_);
    fs::create_directories(map_dir);

    const fs::path generated_graph_source("/tmp/swagger_generated_graph.png");
    const fs::path generated_graph_destination = map_dir / "swagger_generated_graph.png";
    if (fs::exists(generated_graph_source) && !fs::exists(generated_graph_destination)) {
      fs::copy_file(
        generated_graph_source,
        generated_graph_destination,
        fs::copy_options::overwrite_existing);
    }

    const fs::path graph_path = map_dir / "waypoint_graph.json";
    std::ofstream graph_stream(graph_path);
    if (!graph_stream) {
      return false;
    }

    graph_stream << "{\n";
    graph_stream << "  \"nodes\": [\n";
    bool first_node = true;
    for (const auto & [node_id, node] : stored_waypoint_graph_.nodes_by_id) {
      (void)node_id;
      if (!first_node) {
        graph_stream << ",\n";
      }
      first_node = false;
      graph_stream << "    {\"id\":" << node.id
                   << ",\"world_x\":" << node.world_x
                   << ",\"world_y\":" << node.world_y
                   << ",\"pixel_x\":" << node.pixel_x
                   << ",\"pixel_y\":" << node.pixel_y
                   << ",\"node_type\":\"" << escape_json(node.node_type) << "\"}";
    }
    graph_stream << "\n  ],\n";
    graph_stream << "  \"edges\": [\n";
    bool first_edge = true;
    for (const auto & [source_id, edges] : stored_waypoint_graph_.adjacency_list) {
      for (const auto & edge : edges) {
        if (!first_edge) {
          graph_stream << ",\n";
        }
        first_edge = false;
        graph_stream << "    {\"source_id\":" << source_id
                     << ",\"target_id\":" << edge.target_id
                     << ",\"weight\":" << edge.weight
                     << ",\"edge_type\":\"" << escape_json(edge.edge_type) << "\"}";
      }
    }
    graph_stream << "\n  ]\n";
    graph_stream << "}\n";
    graph_stream.close();
    if (!graph_stream) {
      return false;
    }

    const auto metadata = load_saved_map_metadata(saved_map_metadata_path_, saved_map_image_path_);
    const fs::path manifest_path = map_dir / "manifest.json";
    std::ofstream manifest_stream(manifest_path);
    if (!manifest_stream) {
      return false;
    }

    const std::string timestamp_text = active_map_id_.rfind("map_", 0) == 0 ?
      active_map_id_.substr(4) : "";
    manifest_stream << "{\n";
    manifest_stream << "  \"id\":\"" << escape_json(active_map_id_) << "\",\n";
    manifest_stream << "  \"created_unix\":\"" << escape_json(timestamp_text) << "\",\n";
    manifest_stream << "  \"map_image\":\"map.png\",\n";
    manifest_stream << "  \"map_yaml\":\"map.yaml\",\n";
    manifest_stream << "  \"swagger_graph_image\":\"swagger_generated_graph.png\",\n";
    manifest_stream << "  \"waypoint_graph\":\"waypoint_graph.json\",\n";
    manifest_stream << "  \"node_count\":" << node_count << ",\n";
    manifest_stream << "  \"edge_count\":" << edge_count;
    if (metadata.has_value()) {
      manifest_stream << ",\n";
      manifest_stream << "  \"resolution\":" << metadata->resolution << ",\n";
      manifest_stream << "  \"origin\":[" << metadata->origin_x << "," << metadata->origin_y <<
        "," << metadata->origin_yaw << "]";
    }
    manifest_stream << "\n}\n";
    manifest_stream.close();
    return static_cast<bool>(manifest_stream);
  } catch (const std::exception & ex) {
    RCLCPP_ERROR(get_logger(), "Failed to persist map artifacts: %s", ex.what());
    return false;
  }
}

bool WebsocketOrchestrationNode::load_stored_map(const std::string & map_id)
{
  if (map_id.find('/') != std::string::npos || map_id.find("..") != std::string::npos) {
    RCLCPP_WARN(get_logger(), "Rejecting invalid map id: %s", map_id.c_str());
    return false;
  }

  const fs::path map_dir = fs::path(maps_directory_) / map_id;
  const fs::path graph_path = map_dir / "waypoint_graph.json";
  const fs::path image_path = map_dir / "map.png";
  const fs::path metadata_path = map_dir / "map.yaml";

  if (!fs::is_directory(map_dir) || !fs::exists(graph_path) ||
    !fs::exists(image_path) || !fs::exists(metadata_path))
  {
    RCLCPP_WARN(get_logger(), "Stored map %s is missing required artifacts", map_id.c_str());
    return false;
  }

  std::ifstream graph_stream(graph_path);
  if (!graph_stream) {
    return false;
  }

  const std::string graph_text(
    (std::istreambuf_iterator<char>(graph_stream)),
    std::istreambuf_iterator<char>());

  TraversalGraph loaded_graph;
  try {
    const std::regex node_pattern(
      R"json(\{"id":(\d+),"world_x":([-+0-9.eE]+),"world_y":([-+0-9.eE]+),"pixel_x":(-?\d+),"pixel_y":(-?\d+),"node_type":"([^"]*)"\})json");
    for (std::sregex_iterator it(graph_text.begin(), graph_text.end(), node_pattern), end;
      it != end; ++it)
    {
      const auto & match = *it;
      const auto id = static_cast<uint32_t>(std::stoul(match[1].str()));
      loaded_graph.nodes_by_id.emplace(
        id,
        StoredWaypointNode{
          id,
          std::stod(match[2].str()),
          std::stod(match[3].str()),
          static_cast<int32_t>(std::stoi(match[4].str())),
          static_cast<int32_t>(std::stoi(match[5].str())),
          match[6].str()});
      loaded_graph.adjacency_list.try_emplace(id);
    }

    const std::regex edge_pattern(
      R"json(\{"source_id":(\d+),"target_id":(\d+),"weight":([-+0-9.eE]+),"edge_type":"([^"]*)"\})json");
    for (std::sregex_iterator it(graph_text.begin(), graph_text.end(), edge_pattern), end;
      it != end; ++it)
    {
      const auto & match = *it;
      const auto source_id = static_cast<uint32_t>(std::stoul(match[1].str()));
      const auto target_id = static_cast<uint32_t>(std::stoul(match[2].str()));
      if (loaded_graph.nodes_by_id.find(source_id) == loaded_graph.nodes_by_id.end() ||
        loaded_graph.nodes_by_id.find(target_id) == loaded_graph.nodes_by_id.end())
      {
        RCLCPP_WARN(get_logger(), "Stored map %s contains an edge with unknown node ids", map_id.c_str());
        return false;
      }

      loaded_graph.adjacency_list[source_id].push_back(
        TraversalGraphEdge{
          target_id,
          std::stod(match[3].str()),
          match[4].str()});
    }
  } catch (const std::exception & ex) {
    RCLCPP_ERROR(get_logger(), "Failed to parse stored waypoint graph: %s", ex.what());
    return false;
  }

  if (loaded_graph.nodes_by_id.empty()) {
    RCLCPP_WARN(get_logger(), "Stored map %s has no waypoint nodes", map_id.c_str());
    return false;
  }

  stored_waypoint_graph_ = std::move(loaded_graph);
  has_stored_waypoint_graph_ = true;
  active_map_id_ = map_id;
  active_map_directory_ = map_dir.string();
  saved_map_base_path_ = (map_dir / "map").string();
  saved_map_image_path_ = image_path.string();
  saved_map_metadata_path_ = metadata_path.string();

  RCLCPP_INFO(
    get_logger(),
    "Loaded stored map %s with %zu waypoint nodes",
    active_map_id_.c_str(),
    stored_waypoint_graph_.nodes_by_id.size());
  return true;
}

bool WebsocketOrchestrationNode::disable_mapping_backend()
{
  if (mapping_backend_pause_service_name_.empty()) {
    RCLCPP_INFO(get_logger(), "No mapping backend pause service configured; skipping mapping disable");
    return true;
  }

  if (!mapping_backend_pause_client_) {
    RCLCPP_ERROR(get_logger(), "Mapping backend pause client is not initialized");
    return false;
  }

  if (!mapping_backend_pause_client_->wait_for_service(std::chrono::seconds(2))) {
    RCLCPP_ERROR(
      get_logger(),
      "Mapping backend pause service %s is not available",
      mapping_backend_pause_service_name_.c_str());
    return false;
  }

  auto request = std::make_shared<slam_toolbox::srv::Pause::Request>();
  auto future = mapping_backend_pause_client_->async_send_request(request);
  if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
    RCLCPP_ERROR(
      get_logger(),
      "Timed out waiting for mapping backend pause service %s",
      mapping_backend_pause_service_name_.c_str());
    return false;
  }

  const auto response = future.get();
  if (!response->status) {
    RCLCPP_ERROR(
      get_logger(),
      "Mapping backend pause service %s returned failure",
      mapping_backend_pause_service_name_.c_str());
    return false;
  }

  RCLCPP_INFO(
    get_logger(),
    "Disabled further mapping via %s",
    mapping_backend_pause_service_name_.c_str());
  return true;
}

bool WebsocketOrchestrationNode::load_map_into_map_server(const std::string & map_yaml_path)
{
  if (!map_loader_client_->wait_for_service(std::chrono::seconds(2))) {
    RCLCPP_ERROR(
      get_logger(),
      "Map server load service %s is not available",
      map_server_load_service_name_.c_str());
    return false;
  }

  auto request = std::make_shared<nav2_msgs::srv::LoadMap::Request>();
  request->map_url = map_yaml_path;

  RCLCPP_INFO(
    get_logger(),
    "Requesting map server load of %s via %s",
    request->map_url.c_str(),
    map_server_load_service_name_.c_str());

  auto future = map_loader_client_->async_send_request(request);
  if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
    RCLCPP_ERROR(
      get_logger(),
      "Timed out waiting for map server load service %s",
      map_server_load_service_name_.c_str());
    return false;
  }

  const auto response = future.get();
  if (response->result != nav2_msgs::srv::LoadMap::Response::RESULT_SUCCESS) {
    RCLCPP_ERROR(
      get_logger(),
      "Map server failed to load %s with result code %u",
      map_yaml_path.c_str(),
      static_cast<unsigned int>(response->result));
    return false;
  }

  RCLCPP_INFO(get_logger(), "Map server loaded %s", map_yaml_path.c_str());
  return true;
}

bool WebsocketOrchestrationNode::lookup_robot_transform_in_map(
  geometry_msgs::msg::TransformStamped & transform) const
{
  try {
    transform = tf_buffer_.lookupTransform("map", "robot_base", tf2::TimePointZero);
    return true;
  } catch (const tf2::TransformException & ex) {
    RCLCPP_WARN(
      get_logger(), "Failed to lookup transform from map to robot_base: %s", ex.what());
    return false;
  }
}

const StoredWaypointNode * WebsocketOrchestrationNode::find_closest_node(
  double world_x, double world_y) const
{
  const StoredWaypointNode * closest_node = nullptr;
  double closest_distance = 0.0;

  for (const auto & [node_id, node] : stored_waypoint_graph_.nodes_by_id) {
    (void)node_id;
    const double dx = node.world_x - world_x;
    const double dy = node.world_y - world_y;
    const double distance = std::hypot(dx, dy);

    if (closest_node == nullptr || distance < closest_distance) {
      closest_node = &node;
      closest_distance = distance;
    }

    RCLCPP_INFO(
      get_logger(),
      "Node id=%u world_x=%.3f world_y=%.3f distance=%.3lf",
      node.id,
      node.world_x,
      node.world_y,
      distance);
  }

  return closest_node;
}

const StoredWaypointNode * WebsocketOrchestrationNode::find_closest_unvisited_node(
  double world_x, double world_y, const std::unordered_set<uint32_t> & visited_node_ids) const
{
  const StoredWaypointNode * closest_node = nullptr;
  double closest_distance = 0.0;

  for (const auto & [node_id, node] : stored_waypoint_graph_.nodes_by_id) {
    if (visited_node_ids.find(node_id) != visited_node_ids.end()) {
      continue;
    }

    const double dx = node.world_x - world_x;
    const double dy = node.world_y - world_y;
    const double distance = std::hypot(dx, dy);

    if (closest_node == nullptr || distance < closest_distance) {
      closest_node = &node;
      closest_distance = distance;
    }
  }

  return closest_node;
}

void WebsocketOrchestrationNode::continue_navigation_workflow()
{
  if (state_ != WorkflowState::NAVIGATING) {
    RCLCPP_WARN(
      get_logger(), "Cannot continue navigation workflow while in state %s",
      state_to_string(state_).c_str());
    return;
  }

  if (paused_) {
    RCLCPP_WARN(get_logger(), "Navigation workflow is paused; not sending the next goal");
    return;
  }

  if (visited_navigation_node_ids_.size() >= stored_waypoint_graph_.nodes_by_id.size()) {
    RCLCPP_INFO(get_logger(), "All waypoint nodes have been visited");
    mark_navigation_complete();
    return;
  }

  const auto * next_node = find_closest_unvisited_node(
    navigation_current_world_x_, navigation_current_world_y_, visited_navigation_node_ids_);

  if (next_node == nullptr) {
    RCLCPP_WARN(get_logger(), "Failed to find an unvisited node during greedy navigation");
    mark_navigation_complete();
    return;
  }

  RCLCPP_INFO(
    get_logger(),
    "[%zu/%zu nodes] Greedy navigation selected node: id=%u world_x=%.3f world_y=%.3f pixel_x=%d pixel_y=%d node_type=%s",
    visited_navigation_node_ids_.size(),
    stored_waypoint_graph_.nodes_by_id.size(),
    next_node->id,
    next_node->world_x,
    next_node->world_y,
    next_node->pixel_x,
    next_node->pixel_y,
    next_node->node_type.c_str());

  send_navigation_goal_to_node(*next_node);
}

void WebsocketOrchestrationNode::send_navigation_goal_to_node(const StoredWaypointNode & node)
{
  if (!navigate_to_pose_client_->wait_for_action_server(std::chrono::seconds(0))) {
    RCLCPP_ERROR(get_logger(), "NavigateToPose action server /navigate_to_pose is not available");
    mark_navigation_complete();
    return;
  }

  NavigateToPose::Goal goal;
  goal.pose.header.frame_id = "map";
  goal.pose.header.stamp = now();
  goal.pose.pose.position.x = node.world_x;
  goal.pose.pose.position.y = node.world_y;
  goal.pose.pose.position.z = 0.0;
  goal.pose.pose.orientation.x = 0.0;
  goal.pose.pose.orientation.y = 0.0;
  goal.pose.pose.orientation.z = 0.0;
  goal.pose.pose.orientation.w = 1.0;

  RCLCPP_INFO(
    get_logger(),
    "Sending nav goal: node_id=%u goal_x=%.3f goal_y=%.3f pixel_x=%d pixel_y=%d node_type=%s",
    node.id,
    goal.pose.pose.position.x,
    goal.pose.pose.position.y,
    node.pixel_x,
    node.pixel_y,
    node.node_type.c_str());

  rclcpp_action::Client<NavigateToPose>::SendGoalOptions options;
  options.goal_response_callback =
    [this, node](const NavigateToPoseGoalHandle::SharedPtr & goal_handle) {
      if (!goal_handle) {
        RCLCPP_ERROR(get_logger(), "NavigateToPose goal was rejected for node_id=%u", node.id);
        mark_navigation_complete();
        return;
      }

      RCLCPP_INFO(get_logger(), "NavigateToPose goal accepted for node_id=%u", node.id);
    };
  options.result_callback =
    [this, node](const NavigateToPoseGoalHandle::WrappedResult & result) {
      handle_navigation_goal_result(node, result);
    };

  navigate_to_pose_client_->async_send_goal(goal, options);
}

void WebsocketOrchestrationNode::handle_navigation_goal_result(
  const StoredWaypointNode & node,
  const NavigateToPoseGoalHandle::WrappedResult & result)
{
  switch (result.code) {
    case rclcpp_action::ResultCode::SUCCEEDED: {
        RCLCPP_INFO(get_logger(), "NavigateToPose succeeded for node_id=%u", node.id);
        visited_navigation_node_ids_.insert(node.id);

        geometry_msgs::msg::TransformStamped current_transform;
        if (lookup_robot_transform_in_map(current_transform)) {
          navigation_current_world_x_ = current_transform.transform.translation.x;
          navigation_current_world_y_ = current_transform.transform.translation.y;
        } else {
          navigation_current_world_x_ = node.world_x;
          navigation_current_world_y_ = node.world_y;
        }

        continue_navigation_workflow();
        return;
      }
    case rclcpp_action::ResultCode::ABORTED:
      RCLCPP_ERROR(get_logger(), "NavigateToPose aborted for node_id=%u", node.id);
      break;
    case rclcpp_action::ResultCode::CANCELED:
      RCLCPP_ERROR(get_logger(), "NavigateToPose canceled for node_id=%u", node.id);
      break;
    default:
      RCLCPP_ERROR(get_logger(), "NavigateToPose returned unknown result for node_id=%u", node.id);
      break;
  }

  mark_navigation_complete();
}

bool WebsocketOrchestrationNode::has_stored_graph() const
{
  return has_stored_waypoint_graph_;
}

std::string WebsocketOrchestrationNode::make_maps_list_message() const
{
  std::vector<fs::path> map_directories;
  try {
    if (fs::is_directory(maps_directory_)) {
      for (const auto & entry : fs::directory_iterator(maps_directory_)) {
        if (!entry.is_directory()) {
          continue;
        }

        const auto map_id = entry.path().filename().string();
        if (map_id.rfind("map_", 0) != 0) {
          continue;
        }

        map_directories.push_back(entry.path());
      }
    }
  } catch (const std::exception & ex) {
    RCLCPP_ERROR(get_logger(), "Failed to list maps under %s: %s", maps_directory_.c_str(), ex.what());
  }

  std::sort(map_directories.begin(), map_directories.end());

  std::string body = "{\"type\":\"maps_list\",\"maps\":[";
  bool first = true;
  for (const auto & map_dir : map_directories) {
    const auto map_id = map_dir.filename().string();
    if (!first) {
      body += ",";
    }
    first = false;

    const bool has_graph = fs::exists(map_dir / "waypoint_graph.json");
    const bool has_map = fs::exists(map_dir / "map.yaml") && fs::exists(map_dir / "map.png");
    body += "{\"id\":\"" + escape_json(map_id) + "\",\"path\":\"" +
      escape_json(map_dir.string()) + "\",\"has_graph\":" + (has_graph ? "true" : "false") +
      ",\"has_map\":" + (has_map ? "true" : "false") +
      ",\"selected\":" + (map_id == active_map_id_ ? "true" : "false") + "}";
  }
  body += "]}";
  return body;
}

std::string WebsocketOrchestrationNode::make_map_selected_message(const std::string & map_id) const
{
  return "{\"type\":\"map_selected\",\"map_id\":\"" + escape_json(map_id) +
         "\",\"state\":\"" + state_to_string(state_) +
         "\",\"paused\":" + (paused_ ? "true" : "false") + "}";
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
         "\",\"paused\":" + (paused_ ? "true" : "false") +
         ",\"map_id\":\"" + escape_json(active_map_id_) + "\"}";
}

std::string WebsocketOrchestrationNode::make_healthcheck_body() const
{
  return "{\"status\":\"ok\",\"state\":\"" + state_to_string(state_) +
         "\",\"paused\":" + (paused_ ? "true" : "false") +
         ",\"map_id\":\"" + escape_json(active_map_id_) + "\"}";
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
    case WorkflowState::NAVIGATING:
      return "NAVIGATING";
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
