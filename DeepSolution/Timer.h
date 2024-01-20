#pragma once

#include <chrono>

class Timer
{
public:
	using Clock = std::chrono::high_resolution_clock;
	using TimePoint = std::chrono::time_point<Clock>;

	Timer();

	float getDeltaTime();
private:
	TimePoint lastTimePoint;
};