#ifndef STOCKTAKE_ORCHESTRATION2__WEBSOCKET_ORCHESTRATION_NODE_HPP_
#define STOCKTAKE_ORCHESTRATION2__WEBSOCKET_ORCHESTRATION_NODE_HPP_

#include <atomic>
#include <deque>
#include <memory>
#include <string>
#include <thread>
#include <unordered_set>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <explore_lite_msgs/msg/explore_status.hpp>
#include <nav2_msgs/srv/save_map.hpp>
#include <rclcpp/rclcpp.hpp>
#include <stocktake_nvidia_swagger_msgs/srv/generate_waypoint_graph.hpp>
#include <std_msgs/msg/bool.hpp>

namespace stocktake_orchestration2
{

class WebsocketOrchestrationNode;

enum class WorkflowState
{
  IDLE,
  MAPPING,
  CONSTRUCTING_ROUTE
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
  using tcp = boost::asio::ip::tcp;
  using request_type = boost::beast::http::request<boost::beast::http::string_body>;
  using response_type = boost::beast::http::response<boost::beast::http::string_body>;

  WebsocketOrchestrationNode();
  ~WebsocketOrchestrationNode() override;

  // Call from future ROS callbacks once mapping work has completed.
  void mark_mapping_complete();

  // Call from future ROS callbacks once route construction has completed.
  void mark_route_construction_complete();

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
  void pause_workflow();
  void resume_workflow();
  void transition_to(WorkflowState new_state, bool paused);
  void handle_mapping_complete_on_io_thread();
  void handle_route_construction_complete_on_io_thread();

  // Placeholder transition hooks implemented in a separate translation unit.
  void on_enter_mapping_from_idle();
  void on_enter_constructing_route_from_mapping();
  void handle_explore_status(
    const explore_lite_msgs::msg::ExploreStatus::SharedPtr message);
  void request_map_save();
  void handle_map_save_response(rclcpp::Client<nav2_msgs::srv::SaveMap>::SharedFuture future);
  void request_waypoint_graph_generation(const std::string & map_image_path);
  void handle_generate_waypoint_graph_response(
    rclcpp::Client<stocktake_nvidia_swagger_msgs::srv::GenerateWaypointGraph>::SharedFuture future);
  void publish_explore_resume_state();
  void publish_explore_resume_once(bool enabled);

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
  std::unordered_set<std::shared_ptr<WebSocketSession>> sessions_;
  rclcpp::Client<nav2_msgs::srv::SaveMap>::SharedPtr map_saver_client_;
  rclcpp::Client<stocktake_nvidia_swagger_msgs::srv::GenerateWaypointGraph>::SharedPtr
    generate_waypoint_graph_client_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr explore_resume_publisher_;
  rclcpp::Subscription<explore_lite_msgs::msg::ExploreStatus>::SharedPtr explore_status_subscription_;
  rclcpp::TimerBase::SharedPtr explore_resume_timer_;

  WorkflowState state_;
  bool paused_;
  std::atomic<bool> explore_resume_enabled_;
  std::atomic<bool> explore_resume_true_sent_;
  std::string saved_map_base_path_;
  std::string saved_map_image_path_;
};

}  // namespace stocktake_orchestration2

#endif  // STOCKTAKE_ORCHESTRATION2__WEBSOCKET_ORCHESTRATION_NODE_HPP_
