#include <gz/sim/System.hh>
#include <gz/msgs.hh>

/* StoreGenerator

- Responsible for running each round of the simulation

*/

class StoreGenerator :
	public gz::sim::System,
	public gz::sim::ISystemPreUpdate,
	public gz::sim::ISystemConfigure
{

	public:
		StoreGenerator();
		~StoreGenerator() override;

	private:
		// Holds a 10x40 grid
		double floorplan[400];


		/* 1. Resets the simulation and tells the robot plugin to commence */
		void PrepareRound();
		
		// Moves the robot
		void ResetState();

		// Simulates old tags being removed and new ones added
		void GenerateTags()

		// Loads a single tag model to the world at the specified location
		void AddTag(/* add in pose - vector3d */);

		/* 2. Commences a new round of stocktake */
		void DoRound();

		/* 3. Finalises the round of stocktake */
		void EndRound();

		// Called at the end of a round to record what was captured
		void LogResult();




	public:
		void PreUpdate(const gz::sim::UpdateInfo &_info, gz::sim::EntityComponentManager &_ecm) override;
		void Configure(
				const gz::sim::Entity &_entity,
				const std::shared_ptr<const sdf::Element> &_sdf,
				gz::sim::EntityComponentManager &_ecm,
				gz::sim::EventManager &_eventMgr) override;

}

