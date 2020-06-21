#pragma once
#include <chrono>

class time_elapsed {
public:
	std::chrono::steady_clock::time_point time_now;
	std::chrono::steady_clock::time_point time_last;
	double total_time = 0.0;

	time_elapsed() {
		tic();
	}

	void tic() {
		time_last = std::chrono::steady_clock::now();
	}
	double operator()() {
		return toc();
	}

	double toc()
	{
		time_now = std::chrono::steady_clock::now();
		double loop_time = (double)(std::chrono::duration_cast<std::chrono::microseconds>(time_now - time_last).count()) / 1e6;
		total_time += loop_time;
		return loop_time;
	}
};
