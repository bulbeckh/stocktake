
#include "stocktake_orchestration.h"

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <iostream>
#include <thread>
#include <string>

using namespace std::chrono_literals;

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;

using tcp = asio::ip::tcp;

// WebSocket session to handle each client connection
void websocket_session(tcp::socket socket) {
    try {
        websocket::stream<tcp::socket> ws(std::move(socket));
        ws.accept();
        std::cout << "Client connected!\n";

        beast::flat_buffer buffer;
        while (true) {
            ws.read(buffer);
            std::string msg = beast::buffers_to_string(buffer.data());

            std::cout << "\nReceived: " << msg << std::endl;

            // Echo message back to client
            ws.text(ws.got_text());
            ws.write(buffer.data());

            buffer.consume(buffer.size()); // Clear buffer for next message
        }
    }
    catch (std::exception& e) {
        std::cerr << "WebSocket session error: " << e.what() << "\n";
    }
}

// WebSocket Server
void run_server(asio::io_context& ioc, unsigned short port) {
    tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), port));

    std::cout << "WebSocket Server running on ws://127.0.0.1:" << port << "\n\n";

    while (true) {
        tcp::socket socket(ioc);
        acceptor.accept(socket);
        std::thread(websocket_session, std::move(socket)).detach(); // Handle client in new thread
    }
}

void StocktakeOrchestration::explore_node_activate_callback(void)
{
	// Set activate message to false by default
	std_msgs::msg::Bool node_activate_msg = std_msgs::msg::Bool();
	node_activate_msg.data = false;

	// If we are in MAPPING state, then turn the explore-lite node on
	if (_state == StocktakeOrchestration::State::MAPPING) {
		node_activate_msg.data = true;
	}

	// Call node
	if (false) RCLCPP_INFO(this->get_logger(), "Calling node");
	explore_node_publisher->publish(node_activate_msg);

	return;
}

void StocktakeOrchestration::call_swagger_graph_generate(void)
{
	// TODO Call swagger node services to return graph
	return;
}



void StocktakeOrchestration::explore_node_status_callback(explore_lite_msgs::msg::ExploreStatus& status)
{
	// TODO Remove
	if (false) RCLCPP_INFO(this->get_logger(), "Received message from explore_node");

	// At completion of exploration, we save the map and generate the waypoints
	if (status.status == "exploration_complete") {

		if (_state == State::MAPPING) {
			_state = State::CONSTRUCTING_ROUTE;

			// Create map png file
			auto map_save_request = std::make_shared<nav2_msgs::srv::SaveMap::Request>();
			map_save_request->map_topic = "/map";
			// TODO Add a timestamp to the map to distinguish the maps
			map_save_request->map_url = "map_out.png";
			map_save_request->image_format = "png";
			map_save_request->map_mode = "trinary";
			map_save_request->free_thresh = 0.25;
			map_save_request->occupied_thresh = 0.65;

			// Call route construction functions
			map_saver_client->async_send_request(map_save_request,
					[this](rclcpp::Client<nav2_msgs::srv::SaveMap>::SharedFuture future) {
						auto response = future.get();

						// If we failed to generate the map then we should notify
						if (!response->result) {
							RCLCPP_INFO(this->get_logger(), "Failed to generate map file");
							// TODO Should we retry?
						} else {
							// Call nvidia-swagger to generate waypoints
							call_swagger_graph_generate();
						}

			});
		}
	}

	return;
}

StocktakeOrchestration::StocktakeOrchestration(void)
	: Node("stocktake_orchestration")
{
	RCLCPP_INFO(this->get_logger(), "Test string");

	// TODO Temporarily removed - this is a blocking server process - needs to run in own thread (that in turn spawns thread upon TCP connections)
	// Start web-server
	/*
	try {
        asio::io_context io_context;
        run_server(io_context, 9002);
    }
    catch (std::exception& e) {
        std::cerr << "Server error: " << e.what() << "\n";
    }
	*/

	// Setup timer to run callback every second
	explore_node_activate_timer = this->create_wall_timer(1s, 
		std::bind(&StocktakeOrchestration::explore_node_activate_callback, this));

	// Create explore_node publisher
	explore_node_publisher = this->create_publisher<std_msgs::msg::Bool>("explore/resume", 10);

	// Subscribe to explore_node status publisher
	explore_node_subscriber = this->create_subscription<explore_lite_msgs::msg::ExploreStatus>("explore/status", 10,
			[this](explore_lite_msgs::msg::ExploreStatus status) {
				explore_node_status_callback(status);
				});

	// Map saver service client
	map_saver_client = this->create_client<nav2_msgs::srv::SaveMap>("save_map");

	call_swagger_graph_generate();

	return;
}

int main(int argc, char** argv)
{
	rclcpp::init(argc, argv);
	rclcpp::spin(std::make_shared<StocktakeOrchestration>());
	rclcpp::shutdown();
	return 0;
}
