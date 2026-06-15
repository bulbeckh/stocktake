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

namespace fs = std::filesystem;

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
  // Setup parameters
  const auto host = declare_parameter<std::string>("host", "127.0.0.1");
  const auto port = declare_parameter<int>("port", 9002);

  maps_directory_ = declare_parameter<std::string>("maps_directory", "/maps");
  map_server_load_service_name_ = declare_parameter<std::string>("map_server_load_service", "/map_server/load_map");
  slam_toolbox_lifecycle_node_name_ = declare_parameter<std::string>("slam_toolbox_lifecycle_node", "/slam_toolbox");
  amcl_lifecycle_node_name_ = declare_parameter<std::string>("amcl_lifecycle_node", "/amcl");

  // Setup TCP endpoint for websocket
  const auto address = boost::asio::ip::make_address(host);
  const tcp::endpoint endpoint(address, static_cast<unsigned short>(port));

  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
  acceptor_.bind(endpoint);
  acceptor_.listen(boost::asio::socket_base::max_listen_connections);

  // Setup map parameters
  active_map_id_.clear();
  active_map_directory_.clear();
  saved_map_base_path_ = "/tmp/stocktake_map";
  saved_map_image_path_ = saved_map_base_path_ + ".png";
  saved_map_metadata_path_ = saved_map_base_path_ + ".yaml";

  // Create ROS2 service and action clients

  // Explore action (frontier)
  explore_client_ = rclcpp_action::create_client<Explore>(this, "/explore");

  // Navigation to set pose
  navigate_to_pose_client_ = rclcpp_action::create_client<NavigateToPose>(this, "/navigate_to_pose");

  // Map save
  map_saver_client_ = create_client<nav2_msgs::srv::SaveMap>("/map_saver/save_map");

  // Map load
  map_loader_client_ = create_client<nav2_msgs::srv::LoadMap>(map_server_load_service_name_);

  // Swagger node service
  generate_waypoint_graph_client_ = create_client<stocktake_nvidia_swagger_msgs::srv::GenerateWaypointGraph>("/generate_waypoint_graph");

  // RFID scan service
  rfid_scan_client_ = create_client<RFIDScan>("/rfid_scanner/scan_request");

  initialize_managed_lifecycle_nodes();

  RCLCPP_INFO(get_logger(), "Starting websocket server on ws://%s:%d/ws", host.c_str(), static_cast<int>(port));

  // Start accepting HTTP connections
  do_accept();

  // Start IO thread
  io_thread_ = std::thread([this]() {io_context_.run();});
}

WebsocketOrchestrationNode::~WebsocketOrchestrationNode()
{
  if (lifecycle_startup_thread_.joinable()) {
    lifecycle_startup_thread_.join();
  }

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

bool WebsocketOrchestrationNode::has_stored_graph() const
{
  return has_stored_waypoint_graph_;
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




}  // namespace stocktake_orchestration2
