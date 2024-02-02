#pragma once

#include <chrono>
#include <vector>

class Bench
{
public:
	using Clock = std::chrono::high_resolution_clock;
	using TimePoint = std::chrono::time_point<Clock>;

	static TimePoint record() {
		return Clock::now();
	}

	template<class T, class Period = std::milli>
	static T diff(TimePoint start, TimePoint end) {
		const std::chrono::duration<T, Period> difference = end - start;
		return difference.count();
	}
};