/*

*/

#include "format.hh"

#include <gz/sim/System.hh>
#include <gz/sim/Entity.hh>
#include <gz/transport.hh>
#include <gz/msgs.hh>

#include <gz/sim/components/Component.hh>
#include <gz/sim/components/JointVelocityCmd.hh>

class ParamSweep :
	public gz::sim::System,
	public gz::sim::ISystemPreUpdate,
	public gz::sim::ISystemConfigure
{

	private:
		bool CreateEntity();

		gz::transport::Node entitycreation_node;
	
		SDFFormatter sf;
		

	public:
		void PreUpdate(const gz::sim::UpdateInfo &_info, gz::sim::EntityComponentManager &_ecm) override;
		void Configure(
				const gz::sim::Entity &_entity,
				const std::shared_ptr<const sdf::Element> &_sdf,
				gz::sim::EntityComponentManager &_ecm,
				gz::sim::EventManager &_eventMgr) override;

};
