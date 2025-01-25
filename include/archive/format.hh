/* SDF Formatter Class for param sweep */

#include <gz/sim/System.hh>

#include <fmt/core.h>

class SDFFormatter
{
	public:
		void generateSDF(std::string path, std::string outname, std::vector<std::string>& params);

};

