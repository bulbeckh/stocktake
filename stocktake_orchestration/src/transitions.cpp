#include "stocktake_orchestration2/websocket_orchestration_node.hpp"

#include <boost/asio/post.hpp>

namespace stocktake_orchestration2
{

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

void WebsocketOrchestrationNode::return_constructing_route_to_idle()
{
  boost::asio::post(
    io_context_,
    [this]() {
      if (state_ == WorkflowState::CONSTRUCTING_ROUTE) {
        transition_to(WorkflowState::IDLE, false);
        RCLCPP_INFO(get_logger(), "State change: CONSTRUCTING_ROUTE -> IDLE");
      }
    });
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


}  // namespace stocktake_orchestration2
