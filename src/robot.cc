#include "robot.hh"

#include <gz/plugin/Register.hh>
#include <gz/sim/Util.hh>
#include <gz/sim/components/Name.hh>
#include <gz/msgs.hh>
#include <gz/math.hh>

#include <math.h>

void StocktakeRobot::PreUpdate(const gz::sim::UpdateInfo &_info, gz::sim::EntityComponentManager &_ecm)
{
	gz::sim::Entity wheel_frontleft = _ecm.EntityByComponents(gz::sim::components::Name("frontleft"));
	gz::sim::Entity wheel_frontright = _ecm.EntityByComponents(gz::sim::components::Name("frontright"));
	gz::sim::Entity wheel_backleft = _ecm.EntityByComponents(gz::sim::components::Name("backleft"));
	gz::sim::Entity wheel_backright = _ecm.EntityByComponents(gz::sim::components::Name("backright"));

	if (wheel_frontleft==gz::sim::kNullEntity
		|| wheel_frontright==gz::sim::kNullEntity
		|| wheel_backleft==gz::sim::kNullEntity
		|| wheel_backright==gz::sim::kNullEntity) {
			gzwarn << "A joint was not found\n";
		}

	auto jvel_fl = _ecm.Component<gz::sim::components::JointVelocityCmd>(wheel_frontleft);
	auto jvel_fr = _ecm.Component<gz::sim::components::JointVelocityCmd>(wheel_frontright);
	auto jvel_bl = _ecm.Component<gz::sim::components::JointVelocityCmd>(wheel_backleft);
	auto jvel_br = _ecm.Component<gz::sim::components::JointVelocityCmd>(wheel_backright);

	// Simulation repeats 1000 times per second
	// Period of rotation should be 2 seconds 2pi/n = 2000

	static int counter=0;
	if (counter%1000==0) gzwarn << "counter reached: " << counter << "\n";
	counter+=1;
		

	if (!jvel_fl) { _ecm.CreateComponent(wheel_frontleft, gz::sim::components::JointVelocityCmd({this->wheel_velocities[0]}));
	} else {
		jvel_fl->Data() = {this->wheel_velocities[0]};
	}

	if (!jvel_fr) { _ecm.CreateComponent(wheel_frontright, gz::sim::components::JointVelocityCmd({this->wheel_velocities[1]}));
	} else {
		jvel_fr->Data() = {this->wheel_velocities[1]};
	}

	if (!jvel_bl) { _ecm.CreateComponent(wheel_backleft, gz::sim::components::JointVelocityCmd({this->wheel_velocities[2]}));
	} else {
		jvel_bl->Data() = {this->wheel_velocities[2]};
	}

	if (!jvel_br) { _ecm.CreateComponent(wheel_backright, gz::sim::components::JointVelocityCmd({this->wheel_velocities[3]}));
	} else {
		jvel_br->Data() = {this->wheel_velocities[3]};
	}

	return;

	/*

	
	1. Find components.
	2. Determine current pose.
	3. Calculate motor output.
	4. (Simulation) Set joint speed.

	*/
}

void StocktakeRobot::Configure(const gz::sim::Entity &_entity, const std::shared_ptr<const sdf::Element> &_sdf, gz::sim::EntityComponentManager &_ecm, gz::sim::EventManager &_eventMgr)
{
	return;
}

GZ_ADD_PLUGIN(
	StocktakeRobot,
	gz::sim::System,
	StocktakeRobot::ISystemConfigure,
	StocktakeRobot::ISystemPreUpdate
)
