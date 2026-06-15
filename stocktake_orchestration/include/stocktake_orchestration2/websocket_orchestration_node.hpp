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
#include <gazebo_rfid_plugin/srv/rfid_scan.hpp>
#include <lifecycle_msgs/msg/state.hpp>
#include <lifecycle_msgs/srv/change_state.hpp>
#include <lifecycle_msgs/srv/get_state.hpp>
#include <nav2_msgs/action/navigate_to_pose.hpp>
#include <nav2_msgs/srv/load_map.hpp>
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

struct RFIDTagObservation
{
  uint32_t waypoint_node_id;
  std::string uid;
  std::string data;
  double rssi;
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
  using ChangeState = lifecycle_msgs::srv::ChangeState;
  using GetState = lifecycle_msgs::srv::GetState;
  using NavigateToPose = nav2_msgs::action::NavigateToPose;
  using NavigateToPoseGoalHandle = rclcpp_action::ClientGoalHandle<NavigateToPose>;
  using RFIDScan = gazebo_rfid_plugin::srv::RFIDScan;
  using tcp = boost::asio::ip::tcp;
  using request_type = boost::beast::http::request<boost::beast::http::string_body>;
  using response_type = boost::beast::http::response<boost::beast::http::string_body>;

  WebsocketOrchestrationNode();
  ~WebsocketOrchestrationNode() override;

  // Register websocket sessions
  void register_session(const std::shared_ptr<WebSocketSession> & session);
  void unregister_session(const WebSocketSession * session);

  // Primary method for receiving websocket messages
  void handle_client_message(
    const std::shared_ptr<WebSocketSession> & session,
    const std::string & payload);

  response_type make_http_response(const request_type & request) const;

  // Logs to console on connection close
  void log_disconnect(const boost::beast::error_code & ec) const;

private:
  // HTTP acceptance methods (http is upgraded to websocket)
  void do_accept();
  void on_accept(boost::beast::error_code ec, tcp::socket socket);

  // State transition methods
  void start_mapping();
  void start_navigation();
  void pause_workflow();
  void resume_workflow();
  void return_mapping_to_idle();
  void return_constructing_route_to_idle();
  void transition_to(WorkflowState new_state, bool paused);
  void handle_mapping_complete_on_io_thread();
  void handle_route_construction_complete_on_io_thread();
  void handle_navigation_complete_on_io_thread();
  void on_enter_mapping_from_idle();
  void on_enter_constructing_route_from_mapping();
  void on_enter_navigating_from_idle();
  void mark_mapping_complete();
  void mark_route_construction_complete();
  void mark_navigation_complete();

  // Mapping
  
  /* @brief Start mapping phase by sending explore action to explore node */
  void send_explore_goal();

  /* @brief Handle explore action response - either accepts action or rejects */
  void handle_explore_goal_response(const ExploreGoalHandle::SharedPtr & goal_handle);

  /* @brief Handle explore action feedback - logs explore updates */
  void handle_explore_feedback(ExploreGoalHandle::SharedPtr, const std::shared_ptr<const Explore::Feedback> feedback);

  /* @brief Handle explore action result - transitions based on outcome */
  void handle_explore_result(const ExploreGoalHandle::WrappedResult & result);

  // Route construction

  /* @brief Start route construction phase by calling map save service */
  void request_map_save();

  /* @brief Creates map directory and artifacts */
  bool prepare_new_map_directory();

  /* @brief Handle response from map_saver node - either success or failure */
  void handle_map_save_response(rclcpp::Client<nav2_msgs::srv::SaveMap>::SharedFuture future);

  /* @brief Call swagger node service */
  void request_waypoint_graph_generation(const std::string & map_image_path);

  /* @brief Handle response from swagger node */
  void handle_generate_waypoint_graph_response(rclcpp::Client<stocktake_nvidia_swagger_msgs::srv::GenerateWaypointGraph>::SharedFuture future);

  /* @brief Write route graph (waypoints) to map directory. Note, this is in the transformed ROS2 map coordinates and NOT the swagger graph coordinates */
  bool persist_current_map_artifacts(std::size_t node_count, std::size_t edge_count) const;

  // Navigation
  
  /* @brief Start navigation phase by navigating to first waypoint */
  void run_navigation_workflow();

  /* @brief Called once for each waypoint. Finds closest unvisited waypoint and calls NavigateToPose service */
  void continue_navigation_workflow();

  /* @brief Retrieves robot pose in map frame */
  bool lookup_robot_transform_in_map(geometry_msgs::msg::TransformStamped & transform) const;

  /* @brief Find closest node (euclidean distance) based on world position. Called at start of navigation phase */
  const StoredWaypointNode * find_closest_node(double world_x, double world_y) const;

  /* @brief Like 'find_closest_node' but checking visited_navigation_node_ids_ for unvisited nodes */
  const StoredWaypointNode * find_closest_unvisited_node(double world_x, double world_y, const std::unordered_set<uint32_t> & visited_node_ids) const;

  /* @brief Call NavigateToPose service */
  void send_navigation_goal_to_node(const StoredWaypointNode & node);

  /* @brief Handle response from NavigateToPose service */
  void handle_navigation_goal_result(const StoredWaypointNode & node, const NavigateToPoseGoalHandle::WrappedResult & result);

  /* @brief Request one RFID scan at the current waypoint */
  void request_rfid_scan_at_node(const StoredWaypointNode & node);

  /* @brief Store RFID scan response and continue waypoint navigation */
  void handle_rfid_scan_response(const StoredWaypointNode & node, rclcpp::Client<RFIDScan>::SharedFuture future);

  /* @brief Output aggregated RFID tag observations */
  void log_rfid_scan_summary() const;

  // SLAM and AMCL lifecycle management
  void initialize_managed_lifecycle_nodes();
  bool prepare_mapping_lifecycle();
  bool prepare_navigation_lifecycle();
  bool ensure_lifecycle_node_inactive(const std::string & node_name);
  bool ensure_lifecycle_node_active(const std::string & node_name);
  bool get_lifecycle_state(const std::string & node_name, uint8_t & state_id);
  bool change_lifecycle_state(const std::string & node_name, uint8_t transition_id);

  // Web UI interface (websocket message)

  /* @brief Send current state to all websocket sessions */
  void broadcast_state();

  /* @brief Send RFID scan observations to all websocket sessions */
  void broadcast_rfid_scan_observation(
    const StoredWaypointNode & node,
    const RFIDScan::Response & response);

  /* @brief Create a websocket message with the list of available maps for web UI */
  std::string make_maps_list_message() const;

  /* @brief Create a websocket message of selected map id for web UI */
  std::string make_map_selected_message(const std::string & map_id) const;

  /* @brief Create a state update message for web UI */
  std::string make_state_update_message() const;

  /* @brief Create a websocket message for one completed RFID scan */
  std::string make_rfid_scan_observation_message(
    const StoredWaypointNode & node,
    const RFIDScan::Response & response) const;

  /* @brief Create a health-check message for web UI */
  std::string make_healthcheck_body() const;

  /* @brief Create a command acknowledgemenet message */
  static std::string make_command_ack(const std::string & command, bool accepted, const std::string & reason = "");

  /* @brief Create error message for web UI */
  static std::string make_error(const std::string & message);

  static std::string state_to_string(WorkflowState state);
  static std::string escape_json(const std::string & value);

  // Map selection and loading
  bool load_stored_map(const std::string & map_id);
  bool load_map_into_map_server(const std::string & map_yaml_path);

  // Helper functions
  bool has_stored_graph() const;

  // Primary execution thread
  boost::asio::io_context io_context_;

  // Networking objects
  tcp::acceptor acceptor_;
  std::thread io_thread_;
  std::unordered_set<std::shared_ptr<WebSocketSession>> sessions_;

  std::thread lifecycle_startup_thread_;
  
  // Transform buffers for robot pose in map frame
  tf2_ros::Buffer tf_buffer_;
  tf2_ros::TransformListener tf_listener_;

  // ROS2 Client Handlers
  rclcpp_action::Client<Explore>::SharedPtr explore_client_;
  rclcpp_action::Client<NavigateToPose>::SharedPtr navigate_to_pose_client_;
  rclcpp::Client<nav2_msgs::srv::SaveMap>::SharedPtr map_saver_client_;
  rclcpp::Client<nav2_msgs::srv::LoadMap>::SharedPtr map_loader_client_;
  rclcpp::Client<stocktake_nvidia_swagger_msgs::srv::GenerateWaypointGraph>::SharedPtr generate_waypoint_graph_client_;
  rclcpp::Client<RFIDScan>::SharedPtr rfid_scan_client_;
  std::unordered_map<std::string, rclcpp::Client<ChangeState>::SharedPtr> lifecycle_change_clients_;
  std::unordered_map<std::string, rclcpp::Client<GetState>::SharedPtr> lifecycle_get_clients_;

  // State management

  /* @brief Current orchestration state - one of IDLE, MAPPING, CONSTRUCTING_ROUTE, NAVIGATING */
  WorkflowState state_;
  bool paused_;

  // ROS2 Parameters
  std::string maps_directory_;
  std::string map_server_load_service_name_;
  std::string slam_toolbox_lifecycle_node_name_;
  std::string amcl_lifecycle_node_name_;

  // Selected map id and directory
  std::string active_map_id_;
  std::string active_map_directory_;

  std::string saved_map_base_path_;
  std::string saved_map_image_path_;
  std::string saved_map_metadata_path_;

  // Waypoint Graph representation
  TraversalGraph stored_waypoint_graph_;
  bool has_stored_waypoint_graph_;

  // Navigation phase objects
  std::unordered_set<uint32_t> visited_navigation_node_ids_;
  std::vector<RFIDTagObservation> rfid_scan_observations_;
  double navigation_current_world_x_;
  double navigation_current_world_y_;

};

}  // namespace stocktake_orchestration2

#endif  // STOCKTAKE_ORCHESTRATION2__WEBSOCKET_ORCHESTRATION_NODE_HPP_
