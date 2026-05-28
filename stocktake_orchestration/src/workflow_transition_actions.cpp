#include "stocktake_orchestration2/websocket_orchestration_node.hpp"

namespace stocktake_orchestration2
{

void WebsocketOrchestrationNode::on_enter_mapping_from_idle()
{
  RCLCPP_INFO(
    get_logger(),
    "Starting MAPPING workflow with Explore action server");
  send_explore_goal();
}

void WebsocketOrchestrationNode::on_enter_constructing_route_from_mapping()
{
  RCLCPP_INFO(
    get_logger(),
    "Starting CONSTRUCTING_ROUTE workflow");
  request_map_save();
}

void WebsocketOrchestrationNode::on_enter_navigating_from_idle()
{
  RCLCPP_INFO(
    get_logger(),
    "Starting NAVIGATING workflow with stored waypoint graph");
  run_navigation_workflow();
}

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

}  // namespace stocktake_orchestration2
