
#include "format.hh"

void SDFFormatter::generateSDF(std::string path, std::string outname, std::vector<std::string>& params)
{
	// Open file
	std::ifstream infile(path);
	if (!infile) gzwarn << "Error: template SDF file not opened\n";

	// Obtain string object
	std::string pre_format((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
	
	// DEBUG
	if (true) std::cout << pre_format << "\n";

	for (int i=0;i<6;i++) {
		std::cout << params[i] << "\n";
	}

	// Format
	/* NOTE: Expecting fixed length array in params */
	std::string post_format = fmt::format(pre_format,
		params[0],
		params[1],
		params[2],	
		params[3],
		params[4],
		params[5]
	);
		
	// DEBUG
	if (true) std::cout << post_format << "\n";

	// Output to a string
	std::ofstream outfile(outname);
	outfile << post_format;

	return;
}
