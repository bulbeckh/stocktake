#include "stocktake_orchestration2/websocket_orchestration_node.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>

#include <boost/asio/post.hpp>

namespace stocktake_orchestration2
{

void WebsocketOrchestrationNode::run_navigation_workflow()
{
  geometry_msgs::msg::TransformStamped current_transform;
  if (!lookup_robot_transform_in_map(current_transform)) {
    return;
  }

  RCLCPP_INFO(
    get_logger(),
    "Current robot pose in map frame: x=%.3f y=%.3f z=%.3f qx=%.3f qy=%.3f qz=%.3f qw=%.3f",
    current_transform.transform.translation.x,
    current_transform.transform.translation.y,
    current_transform.transform.translation.z,
    current_transform.transform.rotation.x,
    current_transform.transform.rotation.y,
    current_transform.transform.rotation.z,
    current_transform.transform.rotation.w);

  const auto * closest_node = find_closest_node(
    current_transform.transform.translation.x,
    current_transform.transform.translation.y);

  if (closest_node == nullptr) {
    RCLCPP_WARN(get_logger(), "No waypoint nodes are available in the stored graph");
    return;
  }

  RCLCPP_INFO(
    get_logger(),
    "Closest graph node to current position: id=%u world_x=%.3f world_y=%.3f pixel_x=%d pixel_y=%d node_type=%s",
    closest_node->id,
    closest_node->world_x,
    closest_node->world_y,
    closest_node->pixel_x,
    closest_node->pixel_y,
    closest_node->node_type.c_str());

  visited_navigation_node_ids_.clear();
  navigation_current_world_x_ = current_transform.transform.translation.x;
  navigation_current_world_y_ = current_transform.transform.translation.y;
  continue_navigation_workflow();
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

}
