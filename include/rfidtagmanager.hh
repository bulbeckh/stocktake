/* Class for creating, tracking, and managing a collection of RFID tags
 * 
 * The RFID Tag manager should be added as a system plugin under the <world> element of the SDF.
 * 
 * e.g. <world>
 * 		<plugin name="">
 * 			... optional params ...
 * 		</plugin>
 * 		...
 * 	</world>
 */

#pragma once

#include "gz/sim/System.hh"
#include <gz/sim/Entity.hh>
#include <gz/transport.hh>
#include <gz/msgs.hh>
#include <gz/math.hh>

//#include <gz/sim/components/Component.hh>

struct RFIDTag {
	public:
		/// @brief Default Constructor
		RFIDTag(uint32_t id);

		/// @brief Tag Identifier
		uint32_t tag_id;

		/// @brief Constructor with Pose3d
		RFIDTag(uint32_t id, const gz::math::Pose3d& tag_pose);

		/// @brief Pose of tag within world
		gz::math::Pose3d tag_pose;

		/// @brief Tag is active
		bool active;
};

struct ScanResult {
	public:
		/// @brief Constructor
		ScanResult(uint32_t id, double sig_strength);

		/// @brief Tag Identifier
		uint32_t tag_id;

		/// @brief Signal Strength of this tag (NOTE: eventually move this decibel units)
		double signal_strength;

};


/* #NOTE I don't think we need to implement PreUpdate as the RFIDTagManager should be a passive plugin */
class RFIDTagManager :
	public gz::sim::System,
	public gz::sim::ISystemPreUpdate,
	public gz::sim::ISystemConfigure
{

	private:
		/// @brief an array of the tags managed by this manager
		std::vector<RFIDTag> tags;

		/// @brief integer of tag last generated
		uint32_t tag_id_next;

		/// @brief Callback - Service to add tags to manager.
		/// @param 
		bool addTag(const gz::msgs::Pose& tag_pose, gz::msgs::StringMsg& msg);
		
		/// @brief Callback - Service to remove tags from manager.
		bool removeTag(const gz::msgs::StringMsg& tag_id /*Change to INTEGER*/, gz::msgs::StringMsg& _resp);

		/// @brief node used for creating tags. Should advertise services for adding and removing tags.
		gz::transport::Node tag_manager_node;

		/* NOTE Do we want to support multiple RFID Tag Managers */
		/* NOTE Use this name for default, but get the name from the SDF as a parameter */
		/// @brief AddTag Service name
		std::string add_tag_service_name = "/RFIDManager/AddTag";

		/// @brief RemoveTag Service name
		std::string remove_tag_service_name = "/RFIDManager/RemoveTag";

	public:
		// NOTE What arguments should be part of constructor
		/// @brief Constructor
		RFIDTagManager();

		/// @brief Complete a scan of the surrounding RFID tags
		/// @param location : The current pose within the world that the scan is taking place
		/// @param returnMisses : If true, all RFID tags managed by this manager will be returned. Signal strength will be 0.0 for the tags that are out of range of the scan. If false, only tags in range will be included.
		bool completeScan(gz::math::Pose3d location, bool returnMisses, std::vector<ScanResult>&);

	public:
		/* NOTE Do we need PreUpdate? The RFIDManager responds to callbacks and doesn't modify simualation parameters */
		void PreUpdate(const gz::sim::UpdateInfo &_info, gz::sim::EntityComponentManager &_ecm) override;
		void Configure(
				const gz::sim::Entity &_entity,
				const std::shared_ptr<const sdf::Element> &_sdf,
				gz::sim::EntityComponentManager &_ecm,
				gz::sim::EventManager &_eventMgr) override;

};

