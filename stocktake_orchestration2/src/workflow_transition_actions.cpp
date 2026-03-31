#include "stocktake_orchestration2/websocket_orchestration_node.hpp"

namespace stocktake_orchestration2
{

void WebsocketOrchestrationNode::on_enter_mapping_from_idle()
{
  RCLCPP_INFO(
    get_logger(),
    "Temporary test hook: immediately advancing from MAPPING to CONSTRUCTING_ROUTE");
  mark_mapping_complete();
}

void WebsocketOrchestrationNode::on_enter_constructing_route_from_mapping()
{
  RCLCPP_INFO(
    get_logger(),
    "Starting CONSTRUCTING_ROUTE workflow");
  request_map_save();
}

}  // namespace stocktake_orchestration2
