#include <boost/program_options.hpp>
#include <boost/config.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>                      // for std::cout
#include <utility>                       // for std::pair
#include <algorithm>                     // for std::for_each
#include "filter_osm_data.hpp"
#include <chrono>
#include <fstream>
#include <thread>
#include <regex>
#include "geometry.hpp"

#include "time_elapsed.hpp"

uint64_t bytes_loaded = 0;



void run_parser(osm_sax_parser& parser, std::string input_filename)
{
	try
	{
		parser.loading = true;
		std::cout << std::endl << "Incremental SAX Parser:" << std::endl;
		char buffer[4096];
		const size_t buffer_size = sizeof(buffer) / sizeof(char);

		std::ifstream is(input_filename.c_str(),std::ios::in | std::ios::binary);
		if (!is.is_open())
			throw xmlpp::exception("Could not open file " + input_filename);
		is.clear();
		is.seekg(0);
		is.clear();

		boost::iostreams::filtering_stream<boost::iostreams::input> decompressor;
		{
			const std::regex txt_regex("[a-z]+\\.gz");
			if (std::regex_match(input_filename, txt_regex)) {
				decompressor.push(boost::iostreams::gzip_decompressor());
			}
		}
		decompressor.push(is);

		parser.set_substitute_entities(true);
		do
		{
			std::memset(buffer, 0, buffer_size);
			decompressor.read(buffer, buffer_size - 1);
			if (decompressor.gcount())
			{
				bytes_loaded += decompressor.gcount();
				Glib::ustring input(buffer, buffer + decompressor.gcount());
				parser.parse_chunk(input);
			}
		} while (!decompressor.fail());

		is.close();
		parser.finish_chunk_parsing();
	}
	catch (const xmlpp::exception& ex)
	{
		std::cerr << "libxml exception: " << ex.what() << std::endl;
	}
	parser.loading = false;
}

void run_node_filter(osm_sax_parser& parser)
{
	
	while (parser.loading) {
		
		while (parser.nodes_queue.size()) {
			parser.mutex_nodes_queue.lock();
			node inp_node = parser.nodes_queue.front();
			parser.nodes_queue.pop();
			parser.mutex_nodes_queue.unlock();

			if (parser.extent.within(inp_node)) {
				parser.nodes_kept++;
				if (parser.extentp.within(inp_node)) {
					if (parser.nodes_kept_fine++ < 100)
						inp_node.print_trkpt();
					parser.nodes[inp_node.id] = inp_node;

				}
			}
			
		}
		
		std::this_thread::sleep_for(std::chrono::milliseconds(25));
	}
}

int main(int ac, char** av)
{
	using namespace std;
	time_elapsed time_start{};

	namespace po = boost::program_options;
	std::locale::global(std::locale(""));

	double latlow, lathigh, lonlow, lonhigh;
	std::vector<std::string> waytags{};

	// Declare the supported options.
	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "produce help message")
		("input", po::value<std::string>()->default_value("map.osm.gz"), "input file")
		("latlow", po::value<double>(&latlow)->default_value(42.3974), "lat low")
		("lathigh", po::value<double>(&lathigh)->default_value(42.4363), "lat high")
		("lonlow", po::value<double>(&lonlow)->default_value(-71.202643), "lon low")
		("lonhigh", po::value<double>(&lonhigh)->default_value(-71.13221), "lon high")
		("extent", po::value<std::string>()->default_value(""), "extent polygon file, csv")
		("out_csv", po::value<std::string>()->default_value(""), "output node file, csv")
		("way_csv", po::value<std::string>()->default_value(""), "output way file, csv")
		("waytag", po::value<vector<std::string>>(&waytags)->multitoken(), "way tag[s] to keep e.g.: k:v k:v k:v")
		("verbose", "report extra data to stdout")
		("csv", "csv readout to stdout")
		;

	po::variables_map vm;
	po::store(po::parse_command_line(ac, av, desc), vm);
	po::notify(vm);

	osm_sax_parser parser;

	if (vm.count("waytag")) {
		for (auto& kv : waytags) {
			std::vector<std::string> splits;
			boost::split(splits, kv, [](char c) {return c == ':'; });
			parser.ways_tags_keep[splits[0]] = splits[1];
		}
	}

	if (vm.count("help")) {
		std::cout << desc << "\n";
		return 1;
	}
	bool verbose = false;
	if (vm.count("verbose")) {
		std::cout << "verbose set\n";
		verbose = true;
	}

	std::string input_filename = vm["input"].as<std::string>();
	std::string polygon_filename = vm["extent"].as<std::string>();
	std::string out_csv_filename = vm["out_csv"].as<std::string>();
	std::string out_way_csv_filename = vm["way_csv"].as<std::string>();

	//
	// Load boundary polygon 
	//
	if (polygon_filename != "") {
		ifstream pf(polygon_filename);
		if (!pf)
			return 0;
		std::string line;
		std::vector<std::string> splits;
		while (getline(pf, line)) {
			boost::split(splits, line, [](char c) {return c == ','; });
			point p{ boost::lexical_cast<long double>(splits[1]),
					boost::lexical_cast<long double>(splits[0]) };
			parser.extentp.poly.push_back(std::move(p));
		}
		pf.close();
	}

	std::vector<node> extents = { {latlow, lonlow},{latlow, lonhigh},{lathigh,lonhigh},{lathigh,lonlow} };
	
	for (auto& nd : extents) {
		std::cout << "\n";
		nd.print();
	}


	
	parser.loading = true;
	parser.extent.lathigh = lathigh;
	parser.extent.latlow = latlow;
	parser.extent.lonhigh = lonhigh;
	parser.extent.lonlow = lonlow;

	std::thread p1{ run_parser, std::ref(parser), input_filename };
	std::thread p2{ run_node_filter, std::ref(parser) };
	//run_parser(parser, input_filename);
	uint64_t nodes_last = 0;
	int runtime = 0;
	while (parser.loading) {
		if (time_start() > 1.0) {
			std::cout << "\n" << runtime++ << ": Loading: nodes: " << parser.node_count << " kbytes: " << (bytes_loaded >> 10) << " ways: " << parser.way_count << "way_count_kept: " << parser.way_count_kept << " way_nd: " << parser.nd_count << " nodes kept: " << parser.nodes_kept << " nodes_kept_fine: " << parser.nodes_kept_fine;
			std::cout << "\n\t" << (parser.node_count- nodes_last) << " nodes/sec";
			nodes_last = parser.node_count;
			time_start.tic();
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}
	}
	p1.join();
	p2.join();

	std::cout << "\nnodes: " << parser.node_count;
	std::cout << std::endl;


	/// <summary>
	/// Output nodes to CSV file
	/// </summary>
	/// <param name="ac"></param>
	/// <param name="av"></param>
	/// <returns></returns>
	if (out_csv_filename != "")
	{
		ofstream out_csv(out_csv_filename);
		if (!out_csv)
			return 0;
		for (auto& p : parser.nodes) {
			out_csv << std::setprecision(8) << p.second.lat << ", " << p.second.lon << "\n";
		}
		out_csv << "\n";
		out_csv.close();
	}

	std::cout << "\nkept: " << parser.way_count_kept << " ways based on tag k/v";
	int y = 0;
	// Loop through ways 
	for (auto& way : parser.ways) {
		// Filter nodes into new container based on their Way membership.
		int x = 0;
		for (auto& nd : way.second.nodes) {
			if (parser.nodes.count(nd.id)) {
				parser.nodes[nd.id].roads++;
				x++;
			}
		}
		if (x) {
			way.second.keep = true;
			y++;
			//std::cout << "\nname: " << way.second.get_tag_string_if_exists("name");
			//std::cout << "\n\tnd count: " << way.second.nodes.size();
			//std::cout << "\n\tnd within bounds: " << x;
		}
		else {
			way.second.keep = false;
		}
	}

	for (auto& node : parser.nodes) {
		if (node.second.roads) {
			std::cout << node.second.print_osm();
		}
	}

	for (auto& way : parser.ways) {
		if (way.second.keep) {
			std::cout << way.second.print_osm();
		}
	}


	return 0;
}