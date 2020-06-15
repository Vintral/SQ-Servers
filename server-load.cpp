#include <unistd.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>


//This function reads /proc/stat and returns the idle value for each cpu in a vector
std::vector<long long> get_idle() {

	//Virtual file, created by the Linux kernel on demand
	std::ifstream in( "/proc/stat" );

	std::vector<long long> result;

	//This might broke if there are not 8 columns in /proc/stat
	boost::regex reg("cpu(\\d+) (\\d+) (\\d+) (\\d+) (\\d+) (\\d+) (\\d+) (\\d+) (\\d+)");

	std::string line;
	while ( std::getline(in, line) ) {

		boost::smatch match;
		if ( boost::regex_match( line, match, reg ) ) {

			long long idle_time = boost::lexical_cast<long long>(match[5]);

			result.push_back( idle_time );
		}


	}
	return result;
}

//This function returns the avarege load in the next interval_seconds for each cpu in a vector
//get_load() halts this thread for interval_seconds
std::vector<float> get_load(unsigned interval_seconds) {

	boost::posix_time::ptime current_time_1 = boost::date_time::microsec_clock<boost::posix_time::ptime>::universal_time();
	std::vector<long long> idle_time_1 = get_idle();

	sleep(interval_seconds);

	boost::posix_time::ptime current_time_2 = boost::date_time::microsec_clock<boost::posix_time::ptime>::universal_time();
	std::vector<long long> idle_time_2 = get_idle();

	//We have to measure the time, beacuse sleep is not accurate
	const float total_seconds_elpased = float((current_time_2 - current_time_1).total_milliseconds()) / 1000.f;
	
	std::vector<float> cpu_loads;

	for ( unsigned i = 0; i < idle_time_1.size(); ++i ) {

		//This might get slightly negative, because our time measurment is not accurate
		const float load = 1.f - float(idle_time_2[i] - idle_time_1[i])/(100.f * total_seconds_elpased);
		cpu_loads.push_back( load );

	}
	return cpu_loads;
}

int main() {

	const unsigned measurement_count = 5;
	const unsigned interval_seconds = 5;

	for ( unsigned i = 0; i < measurement_count; ++i ) {

		std::vector<float> cpu_loads = get_load(interval_seconds);
		for ( unsigned i = 0; i < cpu_loads.size(); ++i ) {
			std::cout << "cpu " << i << " : " << cpu_loads[i] * 100.f << "%" << std::endl;
		}
	}


	return 0;
}