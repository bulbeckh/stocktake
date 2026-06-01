#include "stocktake_orchestration2/websocket_orchestration_node.hpp"

namespace stocktake_orchestration2
{

void WebsocketOrchestrationNode::on_enter_mapping_from_idle()
{
  RCLCPP_INFO(
    get_logger(),
    "Starting MAPPING workflow with Explore action server");
  if (!prepare_mapping_lifecycle()) {
    RCLCPP_ERROR(get_logger(), "Failed to prepare lifecycle nodes for mapping");
    return_mapping_to_idle();
    return;
  }

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
  if (!prepare_navigation_lifecycle()) {
    RCLCPP_ERROR(get_logger(), "Failed to prepare lifecycle nodes for navigation");
    mark_navigation_complete();
    return;
  }

  run_navigation_workflow();
}


}  // namespace stocktake_orchestration2
