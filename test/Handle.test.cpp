#include <gtest/gtest.h>
#include "../DeepSolution/Common/Handle.h"

#include <iostream>

TEST(Handles, HandleHasValidIdAndGeneration) {
	HandleMap<int> map;
	auto handle0 = map.add();
	auto handle1 = map.add();
	ASSERT_EQ(handle0.generation, 0);
	ASSERT_EQ(handle1.id, 1);

	map.remove(handle0);
	ASSERT_FALSE(map.is_valid(handle0));
	auto handle0v2 = map.add();
	ASSERT_EQ(handle0v2.id, 0);
	ASSERT_TRUE(map.is_valid(handle0v2)); 
	ASSERT_TRUE(map.is_valid(handle1));
	ASSERT_FALSE(map.is_valid(handle0));
}

struct NoDefault {
	NoDefault() = delete;
	NoDefault(int a) {}
};
// The noexcept is not necessary.
struct NoDefaultMinimalCopy {
	NoDefaultMinimalCopy() = delete;
	NoDefaultMinimalCopy(const NoDefaultMinimalCopy&) {}
	NoDefaultMinimalCopy& operator=(const NoDefaultMinimalCopy&) = delete;
	NoDefaultMinimalCopy(NoDefaultMinimalCopy&&) noexcept {}
	NoDefaultMinimalCopy& operator=(NoDefaultMinimalCopy&&) noexcept { return *this; }
	NoDefaultMinimalCopy(int a) {}
};

struct MoveCounter {
	int copyCounter = 0;
	int moveCounter = 0;
	MoveCounter() = default;
	MoveCounter(const MoveCounter&) { copyCounter++; }
	MoveCounter& operator=(const MoveCounter&) { copyCounter++; return *this; }
	MoveCounter(MoveCounter&&) noexcept { moveCounter++; }
	MoveCounter& operator=(MoveCounter&&) noexcept { moveCounter++; return *this; }
};

TEST(Handles, HandleWithTypes) {
	HandleMap<NoDefault> mapNoDefault;
	auto handle0 = mapNoDefault.add(0);

	HandleMap<NoDefaultMinimalCopy> mapNoDefaultCopy;
	auto handle1 = mapNoDefaultCopy.add(0);

	HandleMap<MoveCounter> mapCopyCount;
	auto handle2 = mapCopyCount.add();
	auto& counter = mapCopyCount.at(handle2);
	ASSERT_EQ(counter.copyCounter, 0);
	ASSERT_EQ(counter.moveCounter, 1); // As the type has a move constructor, it uses the move instead of copy.
}

TEST(Handles, ErrorHandling) {
	HandleMap<double> map;
	Handle fakeHandle{ .id = 1200, .generation = 0 };
	ASSERT_THROW(map.at(fakeHandle), std::out_of_range);
	ASSERT_FALSE(map.is_valid(fakeHandle));
}