/* 

World plugin to run multiple rounds of the simulation

*/

#include <string>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <streambuf>

#include <fmt/core.h>

#include "paramsweep.hh"

#include <gz/plugin/Register.hh>
#include <gz/sim/Util.hh>
#include <gz/sim/components/Name.hh>
#include <gz/msgs.hh>

bool ParamSweep::CreateEntity()
{
	gz::msgs::EntityFactory request;
	gz::msgs::Boolean result_msg;
	bool result;

	/* NOTE: Needs to be full path name */
	request.set_sdf_filename("/home/henry/Documents/robotics/stocktake/models/robot.sdf");
	request.set_pose(

	// NOTE: What's the difference between result and result_msg - why are they both args
	auto ex = this->entitycreation_node.Request("/world/default/create",
		request, 1000, result_msg, result);

	if (ex && result) {
		gzmsg << "Entity Created\n";
	} else {
		gzwarn << "Entity Creation Failed\n";
	}

	return result;
}

void ParamSweep::PreUpdate(const gz::sim::UpdateInfo &_info, gz::sim::EntityComponentManager &_ecm)
{
	static int i=0;
	if (i%1000==0) {
		gzmsg << "in 500 loop " << i << "\n";
	}
	i++;

	/** Physics Friction Parameter Args
	 * 0. mu - default 1
	 * 1. mu1 - default 1
	 * 2. slip1 - default 0
	 * 3. slip2 - default 0
	 * 4. torsional coeff - default 1
	 * 5. torsional slp - default 0
	 * 
	*/

	/* Generate the SDF with the required parameters and then create the Entity */
	if (i==1000)
	{
		std::vector<std::string> paramv = {"0", "0", "100", "0", "1", "0"};
		sf.generateSDF("/home/henry/Documents/robotics/stocktake/models/mecanumsdf.fmt",
						"/home/henry/Documents/robotics/stocktake/models/mecanum.sdf", paramv);

		// repeat but with mecanum-mirror
		sf.generateSDF("/home/henry/Documents/robotics/stocktake/models/mecanum-mirrorsdf.fmt",
						"/home/henry/Documents/robotics/stocktake/models/mecanum-mirror.sdf", paramv);
	
		CreateEntity();
	}

	/*
	// NOTE: This is working
	if (i==2000) {
		std::ifstream mecanum_fmt("/home/henry/Documents/robotics/stocktake/models/mecanumsdf.fmt");
		if (!mecanum_fmt) {
			gzwarn << "Error opening mecanumsdf.fmt file\n";
		}

		std::string pre_fmt((std::istreambuf_iterator<char>(mecanum_fmt)), std::istreambuf_iterator<char>());
	
		//std::cout << pre_fmt << "\n";

		std::string post_fmt = fmt::format(
			pre_fmt,
			1000, // mu
			1, // mu1
			100, // slip1
			0, // slip2
			1, // torsional coeff
			0, // torsional slp
			i==1000 ? "mecanum1" : "mecanum2"
		);

		std::cout << post_fmt << "\n";

		CreateEntity(post_fmt);
	}
	*/

	/*
	if (i==5000) {
		// reset world
		// gz::msgs::WorldReset r;
		//r.set_all(true);

		gz::msgs::WorldControl w;
		gz::msgs::WorldReset *r = w.mutable_reset();
		r->set_all(true);

		gz::msgs::Boolean res_msg;
		bool res;
		this->entitycreation_node.Request("/server_control",
				w, 1000, res_msg, res);
	}
	*/

	/*
	## Option 1 - Modify SDF string and create/remove new entities each sweep
	1. Create SDF with new parameters
	2. Pass to EntityFactory

	## Option 2 - Modify the physics parameters directly (e.g. slip, friction) and reset model pose
	*/

	/*
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

	// 
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
	*/
}

void ParamSweep::Configure(const gz::sim::Entity &_entity, const std::shared_ptr<const sdf::Element> &_sdf, gz::sim::EntityComponentManager &_ecm, gz::sim::EventManager &_eventMgr)
{
	return;
}

GZ_ADD_PLUGIN(
	ParamSweep,
	gz::sim::System,
	ParamSweep::ISystemConfigure,
	ParamSweep::ISystemPreUpdate
)
