#include "stocktake_orchestration2/websocket_orchestration_node.hpp"

#include <chrono>
#include <functional>
#include <memory>

#include <boost/asio/post.hpp>

namespace stocktake_orchestration2
{

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


}
