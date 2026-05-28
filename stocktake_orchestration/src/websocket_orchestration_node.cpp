#include "stocktake_orchestration2/websocket_orchestration_node.hpp"

#include <cmath>
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

  const auto address = boost::asio::ip::make_address(host);
  const tcp::endpoint endpoint(address, static_cast<unsigned short>(port));

  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
  acceptor_.bind(endpoint);
  acceptor_.listen(boost::asio::socket_base::max_listen_connections);

  saved_map_base_path_ = "/tmp/stocktake_map";
  saved_map_image_path_ = saved_map_base_path_ + ".png";

  explore_client_ = rclcpp_action::create_client<Explore>(this, "/explore");
  navigate_to_pose_client_ = rclcpp_action::create_client<NavigateToPose>(
    this, "/navigate_to_pose");
  map_saver_client_ = create_client<nav2_msgs::srv::SaveMap>("/map_saver/save_map");
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

  stored_waypoint_graph_.nodes_by_id.clear();
  stored_waypoint_graph_.adjacency_list.clear();

  for (const auto & node : response->graph.nodes) {
    stored_waypoint_graph_.nodes_by_id.emplace(
      node.id,
      StoredWaypointNode{
        node.id,
        node.world_x,
        node.world_y,
        node.pixel_x,
        node.pixel_y,
        node.node_type});
    stored_waypoint_graph_.adjacency_list.try_emplace(node.id);
  }

  for (const auto & edge : response->graph.edges) {
    stored_waypoint_graph_.adjacency_list[edge.source_id].push_back(
      TraversalGraphEdge{
        edge.target_id,
        edge.weight,
        edge.edge_type});
  }
  has_stored_waypoint_graph_ = true;

  mark_route_construction_complete();
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
    const double dx = (node.world_x-map_offset_x) - world_x;
    const double dy = (node.world_y-map_offset_y) - world_y;
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
  goal.pose.pose.position.x = node.world_x - static_cast<double>(map_offset_x);
  goal.pose.pose.position.y = node.world_y - static_cast<double>(map_offset_y);
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
