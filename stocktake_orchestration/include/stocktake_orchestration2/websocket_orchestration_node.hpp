#ifndef STOCKTAKE_ORCHESTRATION2__WEBSOCKET_ORCHESTRATION_NODE_HPP_
#define STOCKTAKE_ORCHESTRATION2__WEBSOCKET_ORCHESTRATION_NODE_HPP_

#include <deque>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <explore_lite_msgs/action/explore.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <nav2_msgs/action/navigate_to_pose.hpp>
#include <nav2_msgs/srv/save_map.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <stocktake_nvidia_swagger_msgs/srv/generate_waypoint_graph.hpp>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>

namespace stocktake_orchestration2
{

class WebsocketOrchestrationNode;

enum class WorkflowState
{
  IDLE,
  MAPPING,
  CONSTRUCTING_ROUTE,
  NAVIGATING
};

struct StoredWaypointNode
{
  uint32_t id;
  double world_x;
  double world_y;
  int32_t pixel_x;
  int32_t pixel_y;
  std::string node_type;
};

struct TraversalGraphEdge
{
  uint32_t target_id;
  double weight;
  std::string edge_type;
};

struct TraversalGraph
{
  std::unordered_map<uint32_t, StoredWaypointNode> nodes_by_id;
  std::unordered_map<uint32_t, std::vector<TraversalGraphEdge>> adjacency_list;
};

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession>
{
public:
  using tcp = boost::asio::ip::tcp;
  using request_type = boost::beast::http::request<boost::beast::http::string_body>;

  WebSocketSession(
    tcp::socket socket,
    WebsocketOrchestrationNode & server);

  void run(request_type request);
  void send_text(const std::string & message);

private:
  void on_accept(boost::beast::error_code ec);
  void do_read();
  void on_read(boost::beast::error_code ec, std::size_t bytes_transferred);
  void do_write();
  void on_write(boost::beast::error_code ec, std::size_t bytes_transferred);
  void close();

  boost::beast::websocket::stream<tcp::socket> websocket_;
  boost::beast::flat_buffer buffer_;
  std::deque<std::string> write_queue_;
  WebsocketOrchestrationNode & server_;
};

class HttpSession : public std::enable_shared_from_this<HttpSession>
{
public:
  using tcp = boost::asio::ip::tcp;

  HttpSession(
    tcp::socket socket,
    WebsocketOrchestrationNode & server);

  void run();

private:
  using request_type = boost::beast::http::request<boost::beast::http::string_body>;
  using response_type = boost::beast::http::response<boost::beast::http::string_body>;

  void do_read();
  void on_read(boost::beast::error_code ec, std::size_t bytes_transferred);
  void on_write(
    bool close,
    boost::beast::error_code ec,
    std::size_t bytes_transferred);
  void send_response(response_type response);

  tcp::socket socket_;
  boost::beast::flat_buffer buffer_;
  request_type request_;
  std::shared_ptr<response_type> response_;
  WebsocketOrchestrationNode & server_;
};

class WebsocketOrchestrationNode : public rclcpp::Node
{
public:
  using Explore = explore_lite_msgs::action::Explore;
  using ExploreGoalHandle = rclcpp_action::ClientGoalHandle<Explore>;
  using NavigateToPose = nav2_msgs::action::NavigateToPose;
  using NavigateToPoseGoalHandle = rclcpp_action::ClientGoalHandle<NavigateToPose>;
  using tcp = boost::asio::ip::tcp;
  using request_type = boost::beast::http::request<boost::beast::http::string_body>;
  using response_type = boost::beast::http::response<boost::beast::http::string_body>;

  WebsocketOrchestrationNode();
  ~WebsocketOrchestrationNode() override;

  // Call from future ROS callbacks once mapping work has completed.
  void mark_mapping_complete();

  // Call from future ROS callbacks once route construction has completed.
  void mark_route_construction_complete();

  // Call from future ROS callbacks once stocktake navigation has completed.
  void mark_navigation_complete();

  void register_session(const std::shared_ptr<WebSocketSession> & session);
  void unregister_session(const WebSocketSession * session);
  void handle_client_message(
    const std::shared_ptr<WebSocketSession> & session,
    const std::string & payload);

  response_type make_http_response(const request_type & request) const;
  void log_disconnect(const boost::beast::error_code & ec) const;

private:
  void do_accept();
  void on_accept(boost::beast::error_code ec, tcp::socket socket);

  void start_mapping();
  void start_navigation();
  void pause_workflow();
  void resume_workflow();
  void transition_to(WorkflowState new_state, bool paused);
  void handle_mapping_complete_on_io_thread();
  void handle_route_construction_complete_on_io_thread();
  void handle_navigation_complete_on_io_thread();

  // Placeholder transition hooks implemented in a separate translation unit.
  void on_enter_mapping_from_idle();
  void on_enter_constructing_route_from_mapping();
  void on_enter_navigating_from_idle();
  void run_navigation_workflow();
  void send_explore_goal();
  void handle_explore_goal_response(const ExploreGoalHandle::SharedPtr & goal_handle);
  void handle_explore_feedback(
    ExploreGoalHandle::SharedPtr,
    const std::shared_ptr<const Explore::Feedback> feedback);
  void handle_explore_result(const ExploreGoalHandle::WrappedResult & result);
  void return_mapping_to_idle();
  void request_map_save();
  void handle_map_save_response(rclcpp::Client<nav2_msgs::srv::SaveMap>::SharedFuture future);
  void request_waypoint_graph_generation(const std::string & map_image_path);
  void handle_generate_waypoint_graph_response(
    rclcpp::Client<stocktake_nvidia_swagger_msgs::srv::GenerateWaypointGraph>::SharedFuture future);
  bool lookup_robot_transform_in_map(geometry_msgs::msg::TransformStamped & transform) const;
  const StoredWaypointNode * find_closest_node(double world_x, double world_y) const;
  const StoredWaypointNode * find_closest_unvisited_node(
    double world_x, double world_y, const std::unordered_set<uint32_t> & visited_node_ids) const;
  void continue_navigation_workflow();
  void send_navigation_goal_to_node(const StoredWaypointNode & node);
  void handle_navigation_goal_result(
    const StoredWaypointNode & node,
    const NavigateToPoseGoalHandle::WrappedResult & result);
  bool has_stored_graph() const;

  void broadcast_state();
  std::string make_state_update_message() const;
  std::string make_healthcheck_body() const;
  static std::string make_command_ack(
    const std::string & command,
    bool accepted,
    const std::string & reason = "");
  static std::string make_error(const std::string & message);
  static std::string state_to_string(WorkflowState state);
  static std::string escape_json(const std::string & value);

  boost::asio::io_context io_context_;
  tcp::acceptor acceptor_;
  std::thread io_thread_;
  tf2_ros::Buffer tf_buffer_;
  tf2_ros::TransformListener tf_listener_;
  std::unordered_set<std::shared_ptr<WebSocketSession>> sessions_;
  rclcpp_action::Client<Explore>::SharedPtr explore_client_;
  rclcpp_action::Client<NavigateToPose>::SharedPtr navigate_to_pose_client_;
  rclcpp::Client<nav2_msgs::srv::SaveMap>::SharedPtr map_saver_client_;
  rclcpp::Client<stocktake_nvidia_swagger_msgs::srv::GenerateWaypointGraph>::SharedPtr
    generate_waypoint_graph_client_;

  WorkflowState state_;
  bool paused_;
  std::string saved_map_base_path_;
  std::string saved_map_image_path_;
  std::string saved_map_metadata_path_;
  TraversalGraph stored_waypoint_graph_;
  bool has_stored_waypoint_graph_;
  std::unordered_set<uint32_t> visited_navigation_node_ids_;
  double navigation_current_world_x_;
  double navigation_current_world_y_;

};

}  // namespace stocktake_orchestration2

#endif  // STOCKTAKE_ORCHESTRATION2__WEBSOCKET_ORCHESTRATION_NODE_HPP_
