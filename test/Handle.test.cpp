#include <gtest/gtest.h>
#include "../DeepSolution/Common/Handle.h"

#include <iostream>

TEST(HandleMap, CanAddValues) {
	HandleMap<float> map;
	auto h0 = map.add(4.0);
	auto h1 = map.add(12.0);

	ASSERT_EQ(map.find(h0), 4.0);
	ASSERT_EQ(map.find(h1), 12.0);

	for (const auto& item : map)
	{
		std::cout << item.second << "\n";
	}
}

TEST(HandleMap, CanDeleteValues) {
	HandleMap<float> map;
	auto h0 = map.add(4.0);
	auto h1 = map.add(12.0);

	ASSERT_EQ(map.find(h0), 4.0);
	ASSERT_EQ(map.find(h1), 12.0);

	map.remove(h0);

	auto h3 = map.add(80.0);
	auto h4 = map.add(50.0);

	ASSERT_EQ(map.find(h3), 80.0);
	ASSERT_EQ(map.find(h4), 50.0);

	for (const auto& item : map)
	{
		std::cout << item.second << "\n";
	}
}