#include <chrono>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"

#include "std_msgs/msg/bool.hpp"
#include "explore_lite_msgs/msg/explore_status.hpp"
#include "nav2_msgs/srv/save_map.hpp"

class StocktakeOrchestration : public rclcpp::Node
{
	public:
		StocktakeOrchestration();


	private:
		/* @brief Timer to call our explore_node activate/deactivate callback */
		rclcpp::TimerBase::SharedPtr explore_node_activate_timer;

		/* @brief Callback to call explore_node service to activate/deactivate node */ 
		void explore_node_activate_callback(void);

		/* @brief Status received from explore_node callback */
		void explore_node_status_callback(explore_lite_msgs::msg::ExploreStatus& status);

		/* @brief Publisher to publish to explore/resume topic */
		rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr explore_node_publisher;

		/* @brief Subscriber to get explore_node statuses */
		rclcpp::Subscription<explore_lite_msgs::msg::ExploreStatus>::SharedPtr explore_node_subscriber;

		// Map Saving
		 
		/* @brief Service call to save map (occupancy_grid) to png file */
		rclcpp::Client<nav2_msgs::srv::SaveMap>::SharedPtr map_saver_client;

		// NVIDIA-Swagger Graph Generation

		/* @brief Starts generating map waypoint nodes */
		void call_swagger_graph_generate(void);

	private:
		/* **State** contains each of the possible states for the robot.
		 *
		 * IDLE: Waiting for a command from the web interface.
		 *
		 * MAPPING: Currently using the explore-lite (or other frontier exploration library/package)
		 * to navigate around the store and construct a map.
		 *
		 * CONSTRUCTING_ROUTE: Taking the generated map and constructing a set route that covers the
		 * entire store and ensures all regions are scanned (using a set of predefined poses/scans
		 * orientations).
		 *
		 * NAVIGATING: Moving between waypoints along the constructed route.
		 *
		 * SCAN_ROUND: Conducting a scan of an area. Scans are not typically done while moving.
		 *
		 * PAUSED: Manually told to pause by the web interface. Previous state is saved before pause.
		 *
		 * DISCONNECTED: Heartbeat with the web interface was dropped and we are disconnected.
		 *
		 * RETURN_TO_START: Finished entire scan and now returning to dock/original position.
		 *
		 * ERROR: Something has faulted, either with navigation or otherwise. Needs manual intervention.
		 *
		 */
		enum class State {
			IDLE,
			MAPPING,
			CONSTRUCTING_ROUTE,
			NAVIGATING,
			SCAN_ROUND,
			PAUSED,
			DISCONNECTED,
			RETURN_TO_START,
			ERROR
		};

		// TODO Change back to State::IDLE
		/* @brief Current state */
		State _state{State::MAPPING};


		// Method
		//
		// Setup
		// - Open socket
		// - Pause explore-lite node
		//
		// 1. Wait to receive connection
		// 2. Start explore-lite node
		// 3. Add callback for explore-lite node finish monitoring
		// 4. Call map_saver service to generate png image of map (occupancy grid)
		// 5. Use NVIDIA SWAGGER to generate waypoints from occupancy grid
		// 6. Navigate to each waypoint and run stocktake at waypoint
		//
		//
		// - explore-lite node launch/start/stop
		// - call map_saver service to generate png
		// - nvidia-SWAGGER route generation from map
		// - Waypoint navigation (order using nearest-neighbour or other simple heuristic)
		// - Conduct scan at each waypoint
		// - Web interface: map streaming, tag reporting, status reporting/logging
		//
		//
};
