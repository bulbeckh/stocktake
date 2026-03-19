
from pathlib import Path
from typing import Any

import cv2
import rclpy
from rclpy.node import Node

from swagger import WaypointGraphGenerator

from stocktake_nvidia_swagger_msgs.msg import WaypointNode, WaypointEdge, WaypointGraph
from stocktake_nvidia_swagger_msgs.srv import GenerateWaypointGraph


class SwaggerGraphServer(Node):
    def __init__(self) -> None:
        super().__init__("swagger_graph_server")

        # Parameters controlling SWAGGER graph generation
        self.declare_parameter("occupancy_threshold", 127)
        self.declare_parameter("safety_distance", 0.30)
        self.declare_parameter("resolution", 0.05)
        self.declare_parameter("x_offset", 0.0)
        self.declare_parameter("y_offset", 0.0)
        self.declare_parameter("rotation", 0.0)

        self._generator = WaypointGraphGenerator()

        self._service = self.create_service(
            GenerateWaypointGraph,
            "generate_waypoint_graph",
            self.handle_generate_waypoint_graph,
        )

        self.get_logger().info("SWAGGER graph generation service ready on /generate_waypoint_graph")

    def handle_generate_waypoint_graph(
        self,
        request: GenerateWaypointGraph.Request,
        response: GenerateWaypointGraph.Response,
    ) -> GenerateWaypointGraph.Response:
        map_path = Path(request.map_path)

        try:
            if not map_path.exists():
                response.success = False
                response.message = f"Map file does not exist: {map_path}"
                return response

            occupancy_grid = cv2.imread(str(map_path), cv2.IMREAD_GRAYSCALE)
            if occupancy_grid is None:
                response.success = False
                response.message = f"Failed to load image as grayscale PNG: {map_path}"
                return response

            occupancy_threshold = int(self.get_parameter("occupancy_threshold").value)
            safety_distance = float(self.get_parameter("safety_distance").value)
            resolution = float(self.get_parameter("resolution").value)
            x_offset = float(self.get_parameter("x_offset").value)
            y_offset = float(self.get_parameter("y_offset").value)
            rotation = float(self.get_parameter("rotation").value)

            graph_nx = self._generator.build_graph_from_grid_map(
                image=occupancy_grid,
                occupancy_threshold=occupancy_threshold,
                safety_distance=safety_distance,
                resolution=resolution,
                x_offset=x_offset,
                y_offset=y_offset,
                rotation=rotation,
            )

            ros_graph = self._convert_networkx_to_ros_graph(graph_nx)

            response.graph = ros_graph
            response.success = True
            response.message = (
                f"Generated graph with {len(ros_graph.nodes)} nodes and "
                f"{len(ros_graph.edges)} edges from '{map_path}'."
            )
            return response

        except Exception as exc:
            self.get_logger().exception("Failed to generate graph")
            response.success = False
            response.message = f"Exception while generating graph: {exc}"
            return response

    def _convert_networkx_to_ros_graph(self, graph_nx: Any) -> WaypointGraph:
        ros_graph = WaypointGraph()

        # SWAGGER returns a networkx.Graph. Node IDs are whatever networkx uses.
        # We remap them to dense uint32 IDs for ROS messages.
        node_id_map = {}
        next_id = 0

        # Nodes
        for original_node_id, node_data in graph_nx.nodes(data=True):
            ros_node = WaypointNode()
            ros_node.id = next_id
            node_id_map[original_node_id] = next_id
            next_id += 1

            world = node_data.get("world", (0.0, 0.0))
            pixel = node_data.get("pixel", (0, 0))
            node_type = node_data.get("node_type", "")

            ros_node.world_x = float(world[0]) if len(world) > 0 else 0.0
            ros_node.world_y = float(world[1]) if len(world) > 1 else 0.0
            ros_node.pixel_x = int(pixel[0]) if len(pixel) > 0 else 0
            ros_node.pixel_y = int(pixel[1]) if len(pixel) > 1 else 0
            ros_node.node_type = str(node_type)

            ros_graph.nodes.append(ros_node)

        # Edges
        for source, target, edge_data in graph_nx.edges(data=True):
            ros_edge = WaypointEdge()
            ros_edge.source_id = node_id_map[source]
            ros_edge.target_id = node_id_map[target]
            ros_edge.weight = float(edge_data.get("weight", 0.0))
            ros_edge.edge_type = str(edge_data.get("edge_type", ""))

            ros_graph.edges.append(ros_edge)

        return ros_graph


def main(args=None) -> None:
    rclpy.init(args=args)
    node = SwaggerGraphServer()

    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
