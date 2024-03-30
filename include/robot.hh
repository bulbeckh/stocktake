/* Robot model classs

- Records video for SLAM
- Communicates with ROS for SLAM
- Responsible for movement

*/

#include <gz/sim/System.hh>
#include <gz/sim/Entity.hh>
#include <gz/transport.hh>
#include <gz/msgs.hh>

#include <gz/sim/components/Component.hh>
#include <gz/sim/components/JointVelocityCmd.hh>

class StocktakeRobot :
	public gz::sim::System,
	public gz::sim::ISystemPreUpdate,
	public gz::sim::ISystemConfigure
{

	private:
		/* Holds the current wheel velocities.
		Ordered in frontleft, frontright, backleft, backright */
		std::vector<double> wheel_velocities={1.4, -1.4, -1.4, 1.4};

	public:
		void PreUpdate(const gz::sim::UpdateInfo &_info, gz::sim::EntityComponentManager &_ecm) override;
		void Configure(
				const gz::sim::Entity &_entity,
				const std::shared_ptr<const sdf::Element> &_sdf,
				gz::sim::EntityComponentManager &_ecm,
				gz::sim::EventManager &_eventMgr) override;

};
