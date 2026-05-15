#include "stocktake_orchestration2/websocket_orchestration_node.hpp"

#include <memory>

#include <rclcpp/rclcpp.hpp>

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<stocktake_orchestration2::WebsocketOrchestrationNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
