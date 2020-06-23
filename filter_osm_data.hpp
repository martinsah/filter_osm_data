#pragma once
#include <libxml++/libxml++.h>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <glibmm/convert.h> //For Glib::ConvertError
#include <iostream>
#include <cmath>
#include <iomanip>
#include <queue>
#include <mutex>
#include "geometry.hpp"

const long double pi = 3.14159265359;

struct tag {
	std::string k;
	std::string v;
};

struct node : point{
    uint64_t id;
	uint64_t num;
	uint32_t norm_num;
	//long double lat = 0.0;
	//long double lon = 0.0;
	int roads = 0;
	long double distance = 0.0;
	bool used_by_road = false;

    node(double lat=0., double lon=0.) {
        this->lat = (long double)lat;
        this->lon = (long double)lon;
        num = 0;
        norm_num = 0;

    }


	long double operator-(node const& other) {
		long double dlat = (lat - other.lat) * pi / 180.0;
		long double dlon = (lon - other.lon) * pi / 180.0;
		const long double R = 6373; // km radius of earth

		long double a = pow((sin(dlat / 2.0)), 2) + cos(lat * pi / 180.0) * cos(other.lat * pi / 180.0) * pow((sin(dlon / 2.0)), 2);
		long double c = 2 * atan2(sqrt(a), sqrt(1 - a));
		long double d = R * c;
		return d;
	}
	void print() {
		std::cout << std::setprecision(10) << lat << ", " << lon;
	}
	std::string print_wpt(std::string name = "noname") {
		std::stringstream ss;
		ss << "<wpt lat=\"" << std::setprecision(10) << lat << "\" lon=\"" << lon << "\"><name>" << name << "</name></wpt>";
		return ss.str();
	}
	std::string print_trkpt() {
		std::stringstream ss;
		ss << "<trkpt lat=\"" << std::setprecision(10) << lat << "\" lon=\"" << lon << "\"><name>" << distance << "</name></trkpt>";
		return ss.str();
	}
    std::string print_osm() {
        std::stringstream ss;
        ss.imbue(std::locale(std::cout.getloc(), new std::numpunct<char>()));
        ss << "\n<node id=\"" << id << "\" lat=\"" << std::setprecision(10) << lat << "\" lon=\"" << lon << "\"/>";
        return ss.str();
    }
};

enum class way_types {
	road,
	building,
	parking
};

struct way {
	uint64_t id;
	std::string name;
	std::vector<node> nodes;
	std::vector<std::reference_wrapper<node>> v_node_refs;
	std::unordered_map<std::string, std::string>tags;
    bool keep;
	std::string get_tag_string_if_exists(std::string key, std::string noname = "noname")
	{
		if (tags.count(key))
			return(tags[key]);
		else
			return(noname);
	}

    std::string print_osm()
    {
        std::stringstream r;
        r.imbue(std::locale(std::cout.getloc(), new std::numpunct<char>()));
        r << "\n<way id=\"" << id << "\">";
        for (auto& nd : nodes) {
            r << "\n <nd ref=\"" << nd.id << "\"/>";
        }
        for (auto& kv : tags) {
            r << "\n <tag k=\"" << kv.first << "\" v=\"" << kv.second << "\"/>";
        }
        r << "\n</way>";
        return r.str();
    }
};

struct extent_s {
    double latlow;
    double lathigh;
    double lonlow;
    double lonhigh;

    bool within(const node& node_in) {
        return ((node_in.lat > latlow &&
                 node_in.lat < lathigh &&
                node_in.lon < lonhigh &&
                node_in.lon > lonlow)?true : false);
    }
};



class osm_sax_parser : public xmlpp::SaxParser
{
public:
    osm_sax_parser();
    ~osm_sax_parser() override;

    std::atomic<bool> loading;
    bool way_tag;
    enum osm_states {
        s_top,
        s_node,
        s_way,
        s_tag
    };

    uint64_t node_count;
    uint64_t nodes_kept;
    uint64_t nodes_kept_fine;
    uint64_t nd_count;
    uint64_t way_count;
    uint64_t way_count_kept;
    extent_s extent{};

    //boost::lockfree::spsc_queue<node, boost::lockfree::capacity<1024>> nodes_queue;

    std::queue<node> nodes_queue;
    std::mutex mutex_nodes_queue;

    std::unordered_map<uint64_t, node> nodes;
    std::unordered_map<uint64_t, way> ways;
    std::vector<std::reference_wrapper<way>> roads;
    osm_states osm_state;
    extent_poly extentp;

    std::unordered_map<std::string, std::string> ways_tags_keep;

    way new_way{};

    void generate_gpx_trks() {
        uint32_t road_num = 1;
        for (auto way : roads) {
            auto road = way.get();
            auto name = road.get_tag_string_if_exists("name");
            std::cout << "\n<trk><name>" << name << "</name><number>" << road_num++ << "</number><trkseg>";
            for (auto nd : road.v_node_refs) {
                std::cout << "\n\t" << nd.get().print_trkpt();
            }
            std::cout << "\n</trkseg></trk>";
        }
    }

    void print_road_list()
    {
        // print list of roads
        for (auto way : roads) {
            auto road = way.get();
            std::cout << std::endl << road.get_tag_string_if_exists("name");
            int n = 0;
            for (auto nd : road.v_node_refs) {

                std::cout << "\n\t" << nd.get().norm_num << ", " << nd.get().distance;
            }
        }
    }
protected:
    //overrides:
    void on_start_document() override;
    void on_end_document() override;
    void on_start_element(const Glib::ustring& name, const AttributeList& properties) override;
    void on_end_element(const Glib::ustring& name) override;
    void on_warning(const Glib::ustring& text) override;
    void on_error(const Glib::ustring& text) override;
    void on_fatal_error(const Glib::ustring& text) override;
};


void osm_sax_parser::on_warning(const Glib::ustring& text)
{
    try
    {
        std::cout << "on_warning(): " << text << std::endl;
    }
    catch (const Glib::ConvertError& ex)
    {
        std::cerr << "osm_sax_parser::on_warning(): Exception caught while converting text for std::cout: " << ex.what() << std::endl;
    }
}

void osm_sax_parser::on_error(const Glib::ustring& text)
{
    try
    {
        std::cout << "on_error(): " << text << std::endl;
    }
    catch (const Glib::ConvertError& ex)
    {
        std::cerr << "osm_sax_parser::on_error(): Exception caught while converting text for std::cout: " << ex.what() << std::endl;
    }
}

void osm_sax_parser::on_fatal_error(const Glib::ustring& text)
{
    try
    {
        std::cout << "on_fatal_error(): " << text << std::endl;
    }
    catch (const Glib::ConvertError& ex)
    {
        std::cerr << "osm_sax_parser::on_characters(): Exception caught while converting value for std::cout: " << ex.what() << std::endl;
    }
}

osm_sax_parser::osm_sax_parser() : xmlpp::SaxParser()
{
    node_count = 0;
    nd_count = 0;
    nodes_kept = 0;
    way_count = 0;
    nodes_kept_fine = 0;
    way_count_kept = 0;
}

osm_sax_parser::~osm_sax_parser()
{
}

void osm_sax_parser::on_start_document()
{
    std::cout << "on_start_document()" << std::endl;
}

void osm_sax_parser::on_end_document()
{
    std::cout << "on_end_document()" << std::endl;
}

void osm_sax_parser::on_start_element(const Glib::ustring& name,
    const AttributeList& attributes)
{
    //std::cout << "node name=" << name << std::endl;
    std::string name_str{ name };
    // parser states
    if (name_str== "node") {
        osm_state = s_node;
        node newnode{};
        // Print attributes:
		std::string attrname, value;
        for (const auto& attr_pair : attributes)
        {
            attrname = attr_pair.name;
			value = attr_pair.value;

            if (attrname =="id"){
                newnode.id = std::stoll(value);
                continue;
            }
            if (attrname == "lat"){
                newnode.lat = std::stod(value);
                continue;
            }
            if (attrname =="lon"){
                newnode.lon = std::stod(value);
                continue;
            }
        }
        newnode.num = node_count++;
        mutex_nodes_queue.lock();
        nodes_queue.push(newnode);
        mutex_nodes_queue.unlock();
        return;
    }
    if (name_str== "way") {
        osm_state = s_way;
        // Print attributes:
        for (const auto& attr_pair : attributes)
        {
            std::string name, value;
            name = attr_pair.name;
            value = attr_pair.value;

            new_way.tags[name] = value;

            if (name == "id") {
                new_way.id = std::stoll(value);
            }


        }
        way_count++;
        return;
    }
    if (name_str=="nd") {
        node nd{};
        std::string name, value;
        for (const auto& attr_pair : attributes)
        {
            name = attr_pair.name;
            value = attr_pair.value;

            if (name == "ref")
                nd.id = std::stoll(value);
        }
        new_way.nodes.emplace_back(nd);
        nd_count++;
        return;
    }
    if (name_str=="tag") {
        if (osm_state == s_way) {
            std::string name, value, k, v;
            for (const auto& attr_pair : attributes)
            {
                name = attr_pair.name;
                value = attr_pair.value;

                if (name == "k")
                    k = value;
                if (name == "v")
                    v = value;
            }
            new_way.tags[k] = v;
        }
        return;
    }
}

void osm_sax_parser::on_end_element(const Glib::ustring& name)
{
    if (std::string{ name } == "way") {
        osm_state = s_top;
        bool keep = false;
        for (auto& k : new_way.tags) {
            for (auto& kk : ways_tags_keep) {
                if (k.first == kk.first && k.second == kk.second && keep == false) {
                    keep = true;
                }
                if (keep)
                    break;
            }
            if (keep)
                break;
        }
        if (keep) {
            ways[new_way.id] = new_way;
            way_count_kept++;

        }
        new_way = way{};
    }
}
