
import sys
import rclpy
from rclpy.node import Node

from stocktake_nvidia_swagger_msgs.srv import GenerateWaypointGraph


class GraphClient(Node):
    def __init__(self) -> None:
        super().__init__("graph_client")
        self.cli = self.create_client(GenerateWaypointGraph, "generate_waypoint_graph")

        while not self.cli.wait_for_service(timeout_sec=1.0):
            self.get_logger().info("Waiting for service...")

    def send_request(self, map_path: str):
        req = GenerateWaypointGraph.Request()
        req.map_path = map_path
        return self.cli.call_async(req)


def main():
    if len(sys.argv) < 2:
        print("Usage: ros2 run swagger_graph_server test_client.py /path/to/map.png")
        return

    rclpy.init()
    node = GraphClient()
    future = node.send_request(sys.argv[1])

    rclpy.spin_until_future_complete(node, future)

    result = future.result()
    if result is not None:
        print(f"success: {result.success}")
        print(f"message: {result.message}")
        print(f"nodes: {len(result.graph.nodes)}")
        print(f"edges: {len(result.graph.edges)}")

        for i in result.graph.nodes:
            print(i)

        for i in result.graph.edges:
            print(i)

    else:
        print("Service call failed")

    node.destroy_node()
    rclpy.shutdown()


if __name__ == "__main__":
    main()
