#include "stocktake_orchestration2/websocket_orchestration_node.hpp"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <boost/asio/post.hpp>

namespace stocktake_orchestration2
{
namespace
{

namespace fs = std::filesystem;

std::optional<std::string> extract_json_string(
  const std::string & payload,
  const std::string & key)
{
  const auto key_pos = payload.find("\"" + key + "\"");
  if (key_pos == std::string::npos) {
    return std::nullopt;
  }

  const auto colon_pos = payload.find(':', key_pos);
  if (colon_pos == std::string::npos) {
    return std::nullopt;
  }

  const auto first_quote = payload.find('"', colon_pos + 1);
  if (first_quote == std::string::npos) {
    return std::nullopt;
  }

  std::string value;
  bool escaping = false;
  for (auto index = first_quote + 1; index < payload.size(); ++index) {
    const char ch = payload[index];
    if (escaping) {
      switch (ch) {
        case '"':
        case '\\':
        case '/':
          value += ch;
          break;
        case 'n':
          value += '\n';
          break;
        case 'r':
          value += '\r';
          break;
        case 't':
          value += '\t';
          break;
        default:
          value += ch;
          break;
      }
      escaping = false;
      continue;
    }

    if (ch == '\\') {
      escaping = true;
      continue;
    }

    if (ch == '"') {
      return value;
    }

    value += ch;
  }

  return std::nullopt;
}

}  // namespace

void WebsocketOrchestrationNode::handle_client_message(
  const std::shared_ptr<WebSocketSession> & session,
  const std::string & payload)
{
  RCLCPP_INFO(get_logger(), "Incoming websocket message: %s", payload.c_str());

  const auto command_key = std::string{"\"command\""};
  const auto type_key = std::string{"\"type\""};
  const auto command_pos = payload.find(command_key);
  const auto type_pos = payload.find(type_key);

  if (type_pos == std::string::npos || command_pos == std::string::npos) {
    session->send_text(make_error("Invalid JSON payload."));
    return;
  }

  if (payload.find("\"type\":\"command\"") == std::string::npos &&
    payload.find("\"type\": \"command\"") == std::string::npos)
  {
    session->send_text(make_error("Unsupported message type."));
    return;
  }

  const auto colon_pos = payload.find(':', command_pos + command_key.size());
  const auto first_quote = payload.find('"', colon_pos + 1);
  const auto second_quote = payload.find('"', first_quote + 1);
  if (colon_pos == std::string::npos || first_quote == std::string::npos ||
    second_quote == std::string::npos)
  {
    session->send_text(make_error("Invalid JSON payload."));
    return;
  }

  const std::string command = payload.substr(first_quote + 1, second_quote - first_quote - 1);

  /* Here is where handle each message type. Typically, we will hand-off to another method. */

  if (command == "list_maps") {
    session->send_text(make_maps_list_message());
    return;
  }

  if (command == "select_map") {
    if (state_ != WorkflowState::IDLE) {
      session->send_text(make_command_ack(
          command, false, "Maps can only be selected while the workflow is idle."));
      return;
    }

    const auto map_id = extract_json_string(payload, "map_id");
    if (!map_id.has_value() || map_id->empty()) {
      session->send_text(make_command_ack(command, false, "Missing map_id."));
      return;
    }

    // NOTE Does this not just pass by copy anyway in the lambda? Why do we need to copy here too
    const auto previous_graph = stored_waypoint_graph_;
    const bool previous_has_graph = has_stored_waypoint_graph_;
    const std::string previous_map_id = active_map_id_;
    const std::string previous_map_directory = active_map_directory_;
    const std::string previous_map_base_path = saved_map_base_path_;
    const std::string previous_map_image_path = saved_map_image_path_;
    const std::string previous_map_metadata_path = saved_map_metadata_path_;

    const auto restore_previous_map = [this, previous_graph, previous_has_graph, previous_map_id,
        previous_map_directory, previous_map_base_path, previous_map_image_path,
        previous_map_metadata_path]() {
        stored_waypoint_graph_ = previous_graph;
        has_stored_waypoint_graph_ = previous_has_graph;
        active_map_id_ = previous_map_id;
        active_map_directory_ = previous_map_directory;
        saved_map_base_path_ = previous_map_base_path;
        saved_map_image_path_ = previous_map_image_path;
        saved_map_metadata_path_ = previous_map_metadata_path;
      };

    if (!load_stored_map(*map_id)) {
      session->send_text(make_command_ack(command, false, "Failed to load stored map."));
      return;
    }

    if (!load_map_into_map_server(saved_map_metadata_path_)) {
      restore_previous_map();
      session->send_text(make_command_ack(command, false, "Failed to load map into map server."));
      return;
    }

    if (!prepare_navigation_lifecycle()) {
      restore_previous_map();
      session->send_text(
        make_command_ack(command, false, "Failed to prepare localization lifecycle nodes."));
      return;
    }

    session->send_text(make_command_ack(command, true));
    session->send_text(make_map_selected_message(*map_id));
    return;
  }

  if (command == "start_mapping") {
    if (state_ != WorkflowState::IDLE) {
      session->send_text(make_command_ack(
          command, false, "A workflow is already active."));
      return;
    }
    start_mapping();
    session->send_text(make_command_ack(command, true));
    return;
  }

  if (command == "start_stocktake") {
    if (state_ != WorkflowState::IDLE) {
      session->send_text(make_command_ack(
          command, false, "Stocktake can only start while the workflow is idle."));
      return;
    }
    if (!has_stored_graph()) {
      session->send_text(make_command_ack(
          command, false, "No waypoint graph is available. Construct a route first."));
      return;
    }
    start_navigation();
    session->send_text(make_command_ack(command, true));
    return;
  }

  if (command == "pause") {
    if (state_ == WorkflowState::IDLE || paused_) {
      session->send_text(make_command_ack(
          command, false, "Pause/resume is only available while mapping is active."));
      return;
    }
    pause_workflow();
    session->send_text(make_command_ack(command, true));
    return;
  }

  if (command == "resume") {
    if (state_ == WorkflowState::IDLE || !paused_) {
      session->send_text(make_command_ack(
          command, false, "Pause/resume is only available while mapping is active."));
      return;
    }
    resume_workflow();
    session->send_text(make_command_ack(command, true));
    return;
  }

  session->send_text(make_error("Unsupported command."));
}

std::string WebsocketOrchestrationNode::make_map_selected_message(const std::string & map_id) const
{
  return "{\"type\":\"map_selected\",\"map_id\":\"" + escape_json(map_id) +
         "\",\"state\":\"" + state_to_string(state_) +
         "\",\"paused\":" + (paused_ ? "true" : "false") + "}";
}

void WebsocketOrchestrationNode::broadcast_state()
{
  const std::string message = make_state_update_message();
  for (const auto & session : sessions_) {
    session->send_text(message);
  }
}

void WebsocketOrchestrationNode::broadcast_rfid_scan_observation(
  const StoredWaypointNode & node,
  const RFIDScan::Response & response)
{
  const std::string message = make_rfid_scan_observation_message(node, response);
  boost::asio::post(
    io_context_,
    [this, message]() {
      for (const auto & session : sessions_) {
        session->send_text(message);
      }
    });
}

std::string WebsocketOrchestrationNode::make_state_update_message() const
{
  return "{\"type\":\"state_update\",\"state\":\"" + state_to_string(state_) +
         "\",\"paused\":" + (paused_ ? "true" : "false") +
         ",\"map_id\":\"" + escape_json(active_map_id_) + "\"}";
}

std::string WebsocketOrchestrationNode::make_rfid_scan_observation_message(
  const StoredWaypointNode & node,
  const RFIDScan::Response & response) const
{
  std::string body = "{\"type\":\"rfid_scan_observation\",\"waypoint_node_id\":" +
    std::to_string(node.id) + ",\"tags\":[";

  bool first = true;
  for (const auto & tag : response.response.scan) {
    if (!first) {
      body += ",";
    }
    first = false;
    body += "{\"uid\":\"" + escape_json(tag.uid) + "\",\"rssi\":" + std::to_string(tag.rssi) +
      "}";
  }

  body += "]}";
  return body;
}

std::string WebsocketOrchestrationNode::make_healthcheck_body() const
{
  return "{\"status\":\"ok\",\"state\":\"" + state_to_string(state_) +
         "\",\"paused\":" + (paused_ ? "true" : "false") +
         ",\"map_id\":\"" + escape_json(active_map_id_) + "\"}";
}

std::string WebsocketOrchestrationNode::make_command_ack(
  const std::string & command,
  bool accepted,
  const std::string & reason)
{
  std::string body = "{\"type\":\"command_ack\",\"command\":\"" + escape_json(command) +
    "\",\"status\":\"" + (accepted ? "accepted" : "rejected") + "\"";
  if (!accepted && !reason.empty()) {
    body += ",\"reason\":\"" + escape_json(reason) + "\"";
  }
  body += "}";
  return body;
}

std::string WebsocketOrchestrationNode::make_maps_list_message() const
{
  std::vector<fs::path> map_directories;
  try {
    if (fs::is_directory(maps_directory_)) {
      for (const auto & entry : fs::directory_iterator(maps_directory_)) {
        if (!entry.is_directory()) {
          continue;
        }

        const auto map_id = entry.path().filename().string();
        if (map_id.rfind("map_", 0) != 0) {
          continue;
        }

        map_directories.push_back(entry.path());
      }
    }
  } catch (const std::exception & ex) {
    RCLCPP_ERROR(get_logger(), "Failed to list maps under %s: %s", maps_directory_.c_str(), ex.what());
  }

  std::sort(map_directories.begin(), map_directories.end());

  std::string body = "{\"type\":\"maps_list\",\"maps\":[";
  bool first = true;
  for (const auto & map_dir : map_directories) {
    const auto map_id = map_dir.filename().string();
    if (!first) {
      body += ",";
    }
    first = false;

    const bool has_graph = fs::exists(map_dir / "waypoint_graph.json");
    const bool has_map = fs::exists(map_dir / "map.yaml") && fs::exists(map_dir / "map.png");
    body += "{\"id\":\"" + escape_json(map_id) + "\",\"path\":\"" +
      escape_json(map_dir.string()) + "\",\"has_graph\":" + (has_graph ? "true" : "false") +
      ",\"has_map\":" + (has_map ? "true" : "false") +
      ",\"selected\":" + (map_id == active_map_id_ ? "true" : "false") + "}";
  }
  body += "]}";
  return body;
}

std::string WebsocketOrchestrationNode::make_error(const std::string & message)
{
  return "{\"type\":\"error\",\"message\":\"" + escape_json(message) + "\"}";
}

std::string WebsocketOrchestrationNode::state_to_string(WorkflowState state)
{
  switch (state) {
    case WorkflowState::IDLE:
      return "IDLE";
    case WorkflowState::MAPPING:
      return "MAPPING";
    case WorkflowState::CONSTRUCTING_ROUTE:
      return "CONSTRUCTING_ROUTE";
    case WorkflowState::NAVIGATING:
      return "NAVIGATING";
    default:
      return "IDLE";
  }
}

std::string WebsocketOrchestrationNode::escape_json(const std::string & value)
{
  std::string escaped;
  escaped.reserve(value.size());
  for (const char ch : value) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped += ch;
        break;
    }
  }
  return escaped;
}

}
