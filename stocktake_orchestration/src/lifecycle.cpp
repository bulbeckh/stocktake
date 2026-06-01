#include "stocktake_orchestration2/websocket_orchestration_node.hpp"

#include <chrono>
#include <future>
#include <string>

#include <lifecycle_msgs/msg/transition.hpp>

namespace stocktake_orchestration2
{
namespace
{

std::string lifecycle_service_name(const std::string & node_name, const std::string & service_name)
{
  if (node_name.empty()) {
    return {};
  }

  if (node_name.back() == '/') {
    return node_name + service_name;
  }

  return node_name + "/" + service_name;
}

const char * lifecycle_state_name(uint8_t state_id)
{
  switch (state_id) {
    case lifecycle_msgs::msg::State::PRIMARY_STATE_UNKNOWN:
      return "unknown";
    case lifecycle_msgs::msg::State::PRIMARY_STATE_UNCONFIGURED:
      return "unconfigured";
    case lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE:
      return "inactive";
    case lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE:
      return "active";
    case lifecycle_msgs::msg::State::PRIMARY_STATE_FINALIZED:
      return "finalized";
    default:
      return "transitional";
  }
}

const char * lifecycle_transition_name(uint8_t transition_id)
{
  switch (transition_id) {
    case lifecycle_msgs::msg::Transition::TRANSITION_CONFIGURE:
      return "configure";
    case lifecycle_msgs::msg::Transition::TRANSITION_CLEANUP:
      return "cleanup";
    case lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE:
      return "activate";
    case lifecycle_msgs::msg::Transition::TRANSITION_DEACTIVATE:
      return "deactivate";
    default:
      return "transition";
  }
}

}  // namespace

void WebsocketOrchestrationNode::initialize_managed_lifecycle_nodes()
{
  if (!slam_toolbox_lifecycle_node_name_.empty()) {
    lifecycle_change_clients_.emplace(
      slam_toolbox_lifecycle_node_name_,
      create_client<ChangeState>(
        lifecycle_service_name(slam_toolbox_lifecycle_node_name_, "change_state")));
    lifecycle_get_clients_.emplace(
      slam_toolbox_lifecycle_node_name_,
      create_client<GetState>(
        lifecycle_service_name(slam_toolbox_lifecycle_node_name_, "get_state")));
  }

  if (!amcl_lifecycle_node_name_.empty()) {
    lifecycle_change_clients_.emplace(
      amcl_lifecycle_node_name_,
      create_client<ChangeState>(lifecycle_service_name(amcl_lifecycle_node_name_, "change_state")));
    lifecycle_get_clients_.emplace(
      amcl_lifecycle_node_name_,
      create_client<GetState>(lifecycle_service_name(amcl_lifecycle_node_name_, "get_state")));
  }

  lifecycle_startup_thread_ = std::thread(
    [this]() {
      RCLCPP_INFO(get_logger(), "Configuring lifecycle-managed localization nodes");
      if (!slam_toolbox_lifecycle_node_name_.empty()) {
        (void)ensure_lifecycle_node_active(slam_toolbox_lifecycle_node_name_);
      }
      if (!amcl_lifecycle_node_name_.empty()) {
        (void)ensure_lifecycle_node_inactive(amcl_lifecycle_node_name_);
      }
    });
}

bool WebsocketOrchestrationNode::prepare_mapping_lifecycle()
{
  if (lifecycle_startup_thread_.joinable()) {
    lifecycle_startup_thread_.join();
  }

  if (!amcl_lifecycle_node_name_.empty() &&
    !ensure_lifecycle_node_inactive(amcl_lifecycle_node_name_))
  {
    return false;
  }

  if (!slam_toolbox_lifecycle_node_name_.empty() &&
    !ensure_lifecycle_node_active(slam_toolbox_lifecycle_node_name_))
  {
    return false;
  }

  return true;
}

bool WebsocketOrchestrationNode::prepare_navigation_lifecycle()
{
  if (lifecycle_startup_thread_.joinable()) {
    lifecycle_startup_thread_.join();
  }

  if (!slam_toolbox_lifecycle_node_name_.empty() &&
    !ensure_lifecycle_node_inactive(slam_toolbox_lifecycle_node_name_))
  {
    return false;
  }

  if (!amcl_lifecycle_node_name_.empty() &&
    !ensure_lifecycle_node_active(amcl_lifecycle_node_name_))
  {
    return false;
  }

  return true;
}

bool WebsocketOrchestrationNode::ensure_lifecycle_node_inactive(const std::string & node_name)
{
  uint8_t state_id = lifecycle_msgs::msg::State::PRIMARY_STATE_UNKNOWN;
  if (!get_lifecycle_state(node_name, state_id)) {
    return false;
  }

  if (state_id == lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE) {
    return true;
  }

  if (state_id == lifecycle_msgs::msg::State::PRIMARY_STATE_UNCONFIGURED) {
    return change_lifecycle_state(
      node_name, lifecycle_msgs::msg::Transition::TRANSITION_CONFIGURE);
  }

  if (state_id == lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
    return change_lifecycle_state(
      node_name, lifecycle_msgs::msg::Transition::TRANSITION_DEACTIVATE);
  }

  RCLCPP_ERROR(
    get_logger(),
    "Cannot make lifecycle node %s inactive from state %s",
    node_name.c_str(),
    lifecycle_state_name(state_id));
  return false;
}

bool WebsocketOrchestrationNode::ensure_lifecycle_node_active(const std::string & node_name)
{
  uint8_t state_id = lifecycle_msgs::msg::State::PRIMARY_STATE_UNKNOWN;
  if (!get_lifecycle_state(node_name, state_id)) {
    return false;
  }

  if (state_id == lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
    return true;
  }

  if (state_id == lifecycle_msgs::msg::State::PRIMARY_STATE_UNCONFIGURED) {
    if (!change_lifecycle_state(
        node_name, lifecycle_msgs::msg::Transition::TRANSITION_CONFIGURE))
    {
      return false;
    }
    state_id = lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE;
  }

  if (state_id == lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE) {
    return change_lifecycle_state(
      node_name, lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE);
  }

  RCLCPP_ERROR(
    get_logger(),
    "Cannot make lifecycle node %s active from state %s",
    node_name.c_str(),
    lifecycle_state_name(state_id));
  return false;
}

bool WebsocketOrchestrationNode::get_lifecycle_state(
  const std::string & node_name,
  uint8_t & state_id)
{
  const auto client_it = lifecycle_get_clients_.find(node_name);
  if (client_it == lifecycle_get_clients_.end()) {
    RCLCPP_ERROR(get_logger(), "No lifecycle get_state client exists for %s", node_name.c_str());
    return false;
  }

  const auto & client = client_it->second;
  if (!client->wait_for_service(std::chrono::seconds(2))) {
    RCLCPP_ERROR(
      get_logger(),
      "Lifecycle get_state service is not available for %s",
      node_name.c_str());
    return false;
  }

  auto request = std::make_shared<GetState::Request>();
  auto future = client->async_send_request(request);
  if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
    RCLCPP_ERROR(
      get_logger(),
      "Timed out waiting for lifecycle state from %s",
      node_name.c_str());
    return false;
  }

  const auto response = future.get();
  state_id = response->current_state.id;
  RCLCPP_INFO(
    get_logger(),
    "Lifecycle node %s is %s",
    node_name.c_str(),
    lifecycle_state_name(state_id));
  return true;
}

bool WebsocketOrchestrationNode::change_lifecycle_state(
  const std::string & node_name,
  uint8_t transition_id)
{
  const auto client_it = lifecycle_change_clients_.find(node_name);
  if (client_it == lifecycle_change_clients_.end()) {
    RCLCPP_ERROR(get_logger(), "No lifecycle change_state client exists for %s", node_name.c_str());
    return false;
  }

  const auto & client = client_it->second;
  if (!client->wait_for_service(std::chrono::seconds(2))) {
    RCLCPP_ERROR(
      get_logger(),
      "Lifecycle change_state service is not available for %s",
      node_name.c_str());
    return false;
  }

  auto request = std::make_shared<ChangeState::Request>();
  request->transition.id = transition_id;

  RCLCPP_INFO(
    get_logger(),
    "Requesting lifecycle %s on %s",
    lifecycle_transition_name(transition_id),
    node_name.c_str());

  auto future = client->async_send_request(request);
  if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
    RCLCPP_ERROR(
      get_logger(),
      "Timed out waiting for lifecycle %s on %s",
      lifecycle_transition_name(transition_id),
      node_name.c_str());
    return false;
  }

  const auto response = future.get();
  if (!response->success) {
    RCLCPP_ERROR(
      get_logger(),
      "Lifecycle %s on %s failed",
      lifecycle_transition_name(transition_id),
      node_name.c_str());
    return false;
  }

  return true;
}

}  // namespace stocktake_orchestration2
