#pragma once
#include <cstdint>
#include <deque>

#include <tsl/robin_map.h>

enum class Handle: uint32_t
{
	Invalid = 0
};

template<class T, class H = Handle>
class HandleMap
{
	using underlying_map = tsl::robin_map<H, T>;
public:
	// initialise
	HandleMap(): data_map(), recycle_handles(), frontier()
	{

	}

	// iterators
	using iterator = typename underlying_map::iterator;
	using const_iterator = typename underlying_map::const_iterator;
	iterator begin() noexcept { return data_map.begin(); }
	const_iterator begin() const noexcept { return data_map.begin(); }
	iterator end() noexcept { return data_map.end(); }
	const_iterator end() const noexcept { return data_map.end(); }

	template<class ...Args>
	H add(Args... args)
	{
		H id = get_id();
			
		data_map[id] = T(std::forward<Args>(args)...);

		return id;
	}

	H add(T&& t)
	{
		H id = get_id();

		data_map[id] = std::move(t);

		return id;
	}

	T& find(H handle)
	{
		return data_map.at(handle);
	}

	void remove(H handle)
	{
		data_map.erase(handle);
		recycle_handles.push_back(handle);
	}

private:
	underlying_map data_map;
	std::deque<H> recycle_handles;
	uint64_t frontier;

	H get_id()
	{
		bool is_recycled = !recycle_handles.empty();
		H id;
		if (is_recycled)
		{
			id = recycle_handles.front();
			recycle_handles.pop_front();
		}
		else
		{
			id = static_cast<H>(++frontier);
		}
		return id;
	}
};
