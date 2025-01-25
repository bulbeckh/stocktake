/* RFID Tag model class

- Keeps track of all existing RFID tags and position relative to robot. Emulates the scanning of the tags
- Will report itself as tagged to robot via a gz topic when the robot is within a certain range (use a prob distribution dependent on how close robot is to tags)

*/

#include "rfidtagmanager.hh"

#include <gz/plugin/Register.hh>
#include <gz/sim/System.hh>


RFIDTagManager::RFIDTagManager()
{
	// Set tag identifier to 0
	this->tag_id_next=0;

}

bool RFIDTagManager::addTag(const gz::msgs::Pose& tag_pose, gz::msgs::StringMsg& msg)
{
	
	// NOTE No checks yet for nullptr, etc.
	gz::math::Vector3d v(tag_pose.position().x(), tag_pose.position().y(), tag_pose.position().z());

	gz::math::Pose3d pose(v.X(), v.Y(), v.Z(), 0, 0, 0);

	auto a = RFIDTag(this->tag_id_next, pose);

	this->tag_id_next++;

	this->tags.push_back(a);

	gzmsg << tag_pose.position().x() << " " << tag_pose.position().y() << "\n";
	gzmsg << "Tag array now has size: " << this->tags.size() << "\n";
	
	return true;
}

bool RFIDTagManager::removeTag(const gz::msgs::StringMsg& tag_id /*Change to INTEGER*/, gz::msgs::StringMsg& _resp)
{
	/* NOTE We should use a map here to store tags - easier to remove */
	for (auto tag_it=this->tags.begin(); tag_it!=this->tags.end(); tag_it++) {
		auto id_as_uint = std::stoul(tag_id.data());
		// NOTE Is this a safe comparison? Between uint32_t and the return value of std::stoul
		if (id_as_uint==(*tag_it).tag_id) {
			this->tags.erase(tag_it);
			return true;
		}
	}

	gzwarn << "In RFIDManager::removeTag - tag_id not found in list\n";
	return false;
}

void RFIDTagManager::Configure(const gz::sim::Entity &_entity,
				const std::shared_ptr<const sdf::Element> &_sdf,
				gz::sim::EntityComponentManager &_ecm,
				gz::sim::EventManager &_eventMgr)
{
	// Bind callback functions for add and remove tag services
	auto bound_add_tag = std::bind(&RFIDTagManager::addTag, this, std::placeholders::_1, std::placeholders::_2);
	std::function<bool(const gz::msgs::Pose&, gz::msgs::StringMsg&)> cb_add_tag = bound_add_tag;

	auto bound_remove_tag = std::bind(&RFIDTagManager::removeTag, this, std::placeholders::_1, std::placeholders::_2);
	std::function<bool(const gz::msgs::StringMsg&, gz::msgs::StringMsg&)> cb_remove_tag = bound_remove_tag;

	// Create services
	if (!this->tag_manager_node.Advertise(this->add_tag_service_name, cb_add_tag))
	{
		gzerr << "[RFID Manager] Error in setting up add tag service\n";
		return;
	}

	if (!this->tag_manager_node.Advertise(this->remove_tag_service_name, cb_remove_tag))
	{
		gzerr << "[RFID Manager] Error in setting up remove tag service\n";
		return;
	}
	
	return;
}

bool RFIDTagManager::completeScan(gz::math::Pose3d location, bool returnMisses, std::vector<ScanResult>& result)
{

	// Complete a scan round
	// 1) Iterate over all tags and assign a signal strength based on distance an some gaussian noise
	// 2) Create vector of ScanResults and return
	
	gzwarn << "Completing Scan TBD\n";

	return false;
}

void RFIDTagManager::PreUpdate(const gz::sim::UpdateInfo &_info, gz::sim::EntityComponentManager &_ecm)
{


}

GZ_ADD_PLUGIN(RFIDTagManager,
		gz::sim::System,
		RFIDTagManager::ISystemConfigure,
		RFIDTagManager::ISystemPreUpdate)




