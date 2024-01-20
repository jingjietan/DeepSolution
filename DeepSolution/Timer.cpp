#include "Timer.h"

Timer::Timer()
{
	lastTimePoint = Clock::now();
}

float Timer::getDeltaTime()
{
	const auto currentTimePoint = Clock::now();
	const auto time = std::chrono::duration<float, std::chrono::seconds::period>(currentTimePoint - lastTimePoint).count();
	lastTimePoint = currentTimePoint;
	return time;
}
