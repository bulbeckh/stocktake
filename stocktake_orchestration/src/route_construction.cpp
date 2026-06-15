#include "stocktake_orchestration2/websocket_orchestration_node.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <type_traits>
#include <utility>

namespace stocktake_orchestration2
{
namespace
{

namespace fs = std::filesystem;

struct SavedMapMetadata
{
  double resolution;
  double origin_x;
  double origin_y;
  double origin_yaw;
  uint32_t image_width;
  uint32_t image_height;
};

template<typename RequestT, typename = void>
struct has_output_dir : std::false_type {};

template<typename RequestT>
struct has_output_dir<RequestT, std::void_t<decltype(std::declval<RequestT>().output_dir)>>
: std::true_type {};

template<typename RequestT>
void set_output_dir_if_supported(RequestT & request, const std::string & output_dir)
{
  if constexpr (has_output_dir<RequestT>::value) {
    request.output_dir = output_dir;
  } else {
    (void)request;
    (void)output_dir;
  }
}

std::string trim_copy(const std::string & value)
{
  auto first = value.begin();
  while (first != value.end() && std::isspace(static_cast<unsigned char>(*first))) {
    ++first;
  }

  auto last = value.end();
  while (last != first && std::isspace(static_cast<unsigned char>(*(last - 1)))) {
    --last;
  }

  return std::string(first, last);
}

bool parse_yaml_scalar(
  const std::string & line,
  const std::string & key,
  double & value)
{
  const std::string prefix = key + ":";
  if (line.rfind(prefix, 0) != 0) {
    return false;
  }

  value = std::stod(trim_copy(line.substr(prefix.size())));
  return true;
}

bool parse_yaml_origin(
  const std::string & line,
  double & origin_x,
  double & origin_y,
  double & origin_yaw)
{
  const std::string prefix = "origin:";
  if (line.rfind(prefix, 0) != 0) {
    return false;
  }

  auto origin_text = trim_copy(line.substr(prefix.size()));
  if (!origin_text.empty() && origin_text.front() == '[') {
    origin_text.erase(origin_text.begin());
  }
  if (!origin_text.empty() && origin_text.back() == ']') {
    origin_text.pop_back();
  }

  std::replace(origin_text.begin(), origin_text.end(), ',', ' ');
  std::istringstream stream(origin_text);
  stream >> origin_x >> origin_y >> origin_yaw;
  return !stream.fail();
}

std::optional<std::array<uint32_t, 2>> read_png_dimensions(const std::string & image_path)
{
  std::ifstream stream(image_path, std::ios::binary);
  if (!stream) {
    return std::nullopt;
  }

  std::array<unsigned char, 24> header{};
  stream.read(reinterpret_cast<char *>(header.data()), static_cast<std::streamsize>(header.size()));
  if (stream.gcount() != static_cast<std::streamsize>(header.size())) {
    return std::nullopt;
  }

  constexpr std::array<unsigned char, 8> png_signature{
    0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  for (std::size_t i = 0; i < png_signature.size(); ++i) {
    if (header[i] != png_signature[i]) {
      return std::nullopt;
    }
  }

  const auto read_u32_be = [&header](std::size_t offset) {
      return (static_cast<uint32_t>(header[offset]) << 24) |
             (static_cast<uint32_t>(header[offset + 1]) << 16) |
             (static_cast<uint32_t>(header[offset + 2]) << 8) |
             static_cast<uint32_t>(header[offset + 3]);
    };

  return std::array<uint32_t, 2>{read_u32_be(16), read_u32_be(20)};
}

std::optional<SavedMapMetadata> load_saved_map_metadata(
  const std::string & metadata_path,
  const std::string & image_path)
{
  std::ifstream metadata_stream(metadata_path);
  if (!metadata_stream) {
    return std::nullopt;
  }

  SavedMapMetadata metadata{};
  bool has_resolution = false;
  bool has_origin = false;

  try {
    std::string line;
    while (std::getline(metadata_stream, line)) {
      line = trim_copy(line);
      if (line.empty() || line.front() == '#') {
        continue;
      }

      if (parse_yaml_scalar(line, "resolution", metadata.resolution)) {
        has_resolution = true;
        continue;
      }

      if (parse_yaml_origin(line, metadata.origin_x, metadata.origin_y, metadata.origin_yaw)) {
        has_origin = true;
        continue;
      }
    }
  } catch (const std::exception &) {
    return std::nullopt;
  }

  const auto dimensions = read_png_dimensions(image_path);
  if (!has_resolution || !has_origin || !dimensions.has_value() || metadata.resolution <= 0.0) {
    return std::nullopt;
  }

  metadata.image_width = (*dimensions)[0];
  metadata.image_height = (*dimensions)[1];
  return metadata;
}

bool image_pixel_to_nav2_map_coordinates(
  int32_t image_x,
  int32_t image_y,
  const SavedMapMetadata & metadata,
  double & map_x,
  double & map_y)
{
  if (image_x < 0 || image_y < 0 ||
    static_cast<uint32_t>(image_x) >= metadata.image_width ||
    static_cast<uint32_t>(image_y) >= metadata.image_height)
  {
    return false;
  }

  const double local_x = (static_cast<double>(image_x) + 0.5) * metadata.resolution;
  const double local_y =
    (static_cast<double>(metadata.image_height) - static_cast<double>(image_y) - 0.5) *
    metadata.resolution;
  const double cos_yaw = std::cos(metadata.origin_yaw);
  const double sin_yaw = std::sin(metadata.origin_yaw);

  map_x = metadata.origin_x + cos_yaw * local_x - sin_yaw * local_y;
  map_y = metadata.origin_y + sin_yaw * local_x + cos_yaw * local_y;
  return true;
}

}  // namespace

void WebsocketOrchestrationNode::request_map_save()
{
  if (!prepare_new_map_directory()) {
    RCLCPP_ERROR(get_logger(), "Failed to prepare map output directory under %s", maps_directory_.c_str());
    return_constructing_route_to_idle();
    return;
  }

  if (!map_saver_client_->service_is_ready()) {
    RCLCPP_WARN(get_logger(), "Map saver service /map_saver/save_map is not available");
    // TODO Should transition state here too?
    return;
  }

  auto request = std::make_shared<nav2_msgs::srv::SaveMap::Request>();
  request->map_topic = "/map";
  request->map_url = saved_map_base_path_;
  request->image_format = "png";
  request->map_mode = "trinary";
  request->free_thresh = 0.25F;
  request->occupied_thresh = 0.65F;

  RCLCPP_INFO(get_logger(), "Requesting map save to %s", saved_map_base_path_.c_str());
  map_saver_client_->async_send_request(
    request,
    std::bind(&WebsocketOrchestrationNode::handle_map_save_response, this, std::placeholders::_1));
}

void WebsocketOrchestrationNode::handle_map_save_response(
  rclcpp::Client<nav2_msgs::srv::SaveMap>::SharedFuture future)
{
  const auto response = future.get();
  if (!response->result) {
    RCLCPP_ERROR(get_logger(), "Map save request failed");
    return;
  }

  RCLCPP_INFO(
    get_logger(), "Map save complete, requesting waypoint graph generation from %s",
    saved_map_image_path_.c_str());
  request_waypoint_graph_generation(saved_map_image_path_);
}

void WebsocketOrchestrationNode::request_waypoint_graph_generation(const std::string & map_image_path)
{
  if (!generate_waypoint_graph_client_->service_is_ready()) {
    RCLCPP_WARN(get_logger(), "Waypoint graph service /generate_waypoint_graph is not available");
    return;
  }

  auto request =
    std::make_shared<stocktake_nvidia_swagger_msgs::srv::GenerateWaypointGraph::Request>();
  request->map_path = map_image_path;
  set_output_dir_if_supported(*request, active_map_directory_);

  generate_waypoint_graph_client_->async_send_request(
    request,
    std::bind(
      &WebsocketOrchestrationNode::handle_generate_waypoint_graph_response, this,
      std::placeholders::_1));
}

void WebsocketOrchestrationNode::handle_generate_waypoint_graph_response(
  rclcpp::Client<stocktake_nvidia_swagger_msgs::srv::GenerateWaypointGraph>::SharedFuture future)
{
  const auto response = future.get();
  if (!response->success) {
    RCLCPP_ERROR(
      get_logger(), "Waypoint graph generation failed: %s", response->message.c_str());
    return;
  }

  RCLCPP_INFO(
    get_logger(), "Waypoint graph generation complete: %zu nodes, %zu edges",
    response->graph.nodes.size(), response->graph.edges.size());

  const auto map_metadata = load_saved_map_metadata(saved_map_metadata_path_, saved_map_image_path_);
  if (!map_metadata.has_value()) {
    RCLCPP_ERROR(
      get_logger(),
      "Failed to load saved map metadata from %s and image dimensions from %s",
      saved_map_metadata_path_.c_str(),
      saved_map_image_path_.c_str());

    return_constructing_route_to_idle();
    return;
  }

  RCLCPP_INFO(
    get_logger(),
    "Converting swagger graph pixels to Nav2 map frame using resolution=%.3f origin=(%.3f, %.3f, %.3f) image=%ux%u",
    map_metadata->resolution,
    map_metadata->origin_x,
    map_metadata->origin_y,
    map_metadata->origin_yaw,
    map_metadata->image_width,
    map_metadata->image_height);

  stored_waypoint_graph_.nodes_by_id.clear();
  stored_waypoint_graph_.adjacency_list.clear();

  for (const auto & node : response->graph.nodes) {
    double nav2_map_x = 0.0;
    double nav2_map_y = 0.0;
    const int32_t image_x = node.pixel_y;
    const int32_t image_y = node.pixel_x;
    if (!image_pixel_to_nav2_map_coordinates(
        image_x, image_y, *map_metadata, nav2_map_x, nav2_map_y))
    {
      RCLCPP_ERROR(
        get_logger(),
        "Rejecting waypoint graph because node id=%u has out-of-bounds swagger pixel row=%d column=%d for image %ux%u",
        node.id,
        node.pixel_x,
        node.pixel_y,
        map_metadata->image_width,
        map_metadata->image_height);

      stored_waypoint_graph_.nodes_by_id.clear();
      stored_waypoint_graph_.adjacency_list.clear();

      return_constructing_route_to_idle();
      return;
    }

    RCLCPP_INFO(
      get_logger(),
      "Graph node: id=%u swagger_world=(%.3f, %.3f) nav2_map=(%.3f, %.3f) swagger_pixel=(row=%d, column=%d) image_pixel=(x=%d, y=%d) node_type=%s",
      node.id,
      node.world_x,
      node.world_y,
      nav2_map_x,
      nav2_map_y,
      node.pixel_x,
      node.pixel_y,
      image_x,
      image_y,
      node.node_type.c_str());

    stored_waypoint_graph_.nodes_by_id.emplace(
      node.id,
      StoredWaypointNode{
        node.id,
        nav2_map_x,
        nav2_map_y,
        node.pixel_x,
        node.pixel_y,
        node.node_type});
    stored_waypoint_graph_.adjacency_list.try_emplace(node.id);
  }

  for (const auto & edge : response->graph.edges) {
    RCLCPP_INFO(
      get_logger(),
      "Graph edge: source_id=%u target_id=%u weight=%.3f edge_type=%s",
      edge.source_id, edge.target_id, edge.weight, edge.edge_type.c_str());
    stored_waypoint_graph_.adjacency_list[edge.source_id].push_back(
      TraversalGraphEdge{
        edge.target_id,
        edge.weight,
        edge.edge_type});
  }
  has_stored_waypoint_graph_ = true;

  if (!persist_current_map_artifacts(response->graph.nodes.size(), response->graph.edges.size())) {
    RCLCPP_ERROR(get_logger(), "Failed to persist generated map artifacts");
    has_stored_waypoint_graph_ = false;
    stored_waypoint_graph_.nodes_by_id.clear();
    stored_waypoint_graph_.adjacency_list.clear();
    return_constructing_route_to_idle();
    return;
  }

  mark_route_construction_complete();
}

bool WebsocketOrchestrationNode::persist_current_map_artifacts(
  std::size_t node_count,
  std::size_t edge_count) const
{
  if (active_map_id_.empty() || active_map_directory_.empty()) {
    return false;
  }

  try {
    const fs::path map_dir(active_map_directory_);
    fs::create_directories(map_dir);

    const fs::path generated_graph_source("/tmp/swagger_generated_graph.png");
    const fs::path generated_graph_destination = map_dir / "swagger_generated_graph.png";
    if (fs::exists(generated_graph_source) && !fs::exists(generated_graph_destination)) {
      fs::copy_file(
        generated_graph_source,
        generated_graph_destination,
        fs::copy_options::overwrite_existing);
    }

    const fs::path graph_path = map_dir / "waypoint_graph.json";
    std::ofstream graph_stream(graph_path);
    if (!graph_stream) {
      return false;
    }

    graph_stream << "{\n";
    graph_stream << "  \"nodes\": [\n";
    bool first_node = true;
    for (const auto & [node_id, node] : stored_waypoint_graph_.nodes_by_id) {
      (void)node_id;
      if (!first_node) {
        graph_stream << ",\n";
      }
      first_node = false;
      graph_stream << "    {\"id\":" << node.id
                   << ",\"world_x\":" << node.world_x
                   << ",\"world_y\":" << node.world_y
                   << ",\"pixel_x\":" << node.pixel_x
                   << ",\"pixel_y\":" << node.pixel_y
                   << ",\"node_type\":\"" << escape_json(node.node_type) << "\"}";
    }
    graph_stream << "\n  ],\n";
    graph_stream << "  \"edges\": [\n";
    bool first_edge = true;
    for (const auto & [source_id, edges] : stored_waypoint_graph_.adjacency_list) {
      for (const auto & edge : edges) {
        if (!first_edge) {
          graph_stream << ",\n";
        }
        first_edge = false;
        graph_stream << "    {\"source_id\":" << source_id
                     << ",\"target_id\":" << edge.target_id
                     << ",\"weight\":" << edge.weight
                     << ",\"edge_type\":\"" << escape_json(edge.edge_type) << "\"}";
      }
    }
    graph_stream << "\n  ]\n";
    graph_stream << "}\n";
    graph_stream.close();
    if (!graph_stream) {
      return false;
    }

    const auto metadata = load_saved_map_metadata(saved_map_metadata_path_, saved_map_image_path_);
    const fs::path manifest_path = map_dir / "manifest.json";
    std::ofstream manifest_stream(manifest_path);
    if (!manifest_stream) {
      return false;
    }

    const std::string timestamp_text = active_map_id_.rfind("map_", 0) == 0 ?
      active_map_id_.substr(4) : "";
    manifest_stream << "{\n";
    manifest_stream << "  \"id\":\"" << escape_json(active_map_id_) << "\",\n";
    manifest_stream << "  \"created_unix\":\"" << escape_json(timestamp_text) << "\",\n";
    manifest_stream << "  \"map_image\":\"map.png\",\n";
    manifest_stream << "  \"map_yaml\":\"map.yaml\",\n";
    manifest_stream << "  \"swagger_graph_image\":\"swagger_generated_graph.png\",\n";
    manifest_stream << "  \"waypoint_graph\":\"waypoint_graph.json\",\n";
    manifest_stream << "  \"node_count\":" << node_count << ",\n";
    manifest_stream << "  \"edge_count\":" << edge_count;
    if (metadata.has_value()) {
      manifest_stream << ",\n";
      manifest_stream << "  \"resolution\":" << metadata->resolution << ",\n";
      manifest_stream << "  \"origin\":[" << metadata->origin_x << "," << metadata->origin_y <<
        "," << metadata->origin_yaw << "]";
    }
    manifest_stream << "\n}\n";
    manifest_stream.close();
    return static_cast<bool>(manifest_stream);
  } catch (const std::exception & ex) {
    RCLCPP_ERROR(get_logger(), "Failed to persist map artifacts: %s", ex.what());
    return false;
  }
}

bool WebsocketOrchestrationNode::prepare_new_map_directory()
{
  try {
    fs::create_directories(maps_directory_);

    const auto now = std::chrono::system_clock::now();
    const auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
      now.time_since_epoch()).count();

    for (int attempt = 0; attempt < 100; ++attempt) {
      const std::string candidate_id =
        (attempt == 0) ? "map_" + std::to_string(timestamp) :
        "map_" + std::to_string(timestamp) + "_" + std::to_string(attempt);
      const fs::path candidate_dir = fs::path(maps_directory_) / candidate_id;
      if (fs::exists(candidate_dir)) {
        continue;
      }

      fs::create_directories(candidate_dir);
      active_map_id_ = candidate_id;
      active_map_directory_ = candidate_dir.string();
      saved_map_base_path_ = (candidate_dir / "map").string();
      saved_map_image_path_ = (candidate_dir / "map.png").string();
      saved_map_metadata_path_ = (candidate_dir / "map.yaml").string();

      RCLCPP_INFO(
        get_logger(),
        "Prepared map output directory %s",
        active_map_directory_.c_str());
      return true;
    }
  } catch (const std::exception & ex) {
    RCLCPP_ERROR(get_logger(), "Failed to create map directory: %s", ex.what());
    return false;
  }

  RCLCPP_ERROR(get_logger(), "Failed to choose a unique map directory name");
  return false;
}


}
