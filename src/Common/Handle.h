#pragma once
#include <cstdint>
#include <deque>

#include <tsl/robin_map.h>

struct Handle
{
	uint32_t id;
	uint32_t generation;
};

/**
 * @brief HandleMap holds handles from 0 to N. It keeps track of all valid, recycled and outdated handles.
 * 
 * References: https://floooh.github.io/2018/06/17/handles-vs-pointers.html
*/
template<class T>
class HandleMap
{
public:
	static_assert(std::is_copy_constructible_v<T>, "Requires T to be copy constructable.");
	static_assert(std::is_swappable_v<T>, "Requires T to be swappable.");
	static_assert(std::is_move_constructible_v<T>, "Requires T to be move constructable.");

	template<class ...Args>
	Handle add(Args&&... args) {
		const auto id = get_id();
		const Item& item = get_or_create_item(id, std::forward<Args>(args)...);
		return { .id = id, .generation = item.second };
	}

	void remove(const Handle& handle) {
		map_[handle.id].second++;
		recycle_buffer_.push_back(handle.id);
	}

	bool is_valid(const Handle& handle) {
		if (auto it = map_.find(handle.id); it != map_.end()) {
			return it->second.second == handle.generation;
		}
		return false;
	}

	T& at(const Handle& handle) {
		return map_.at(handle.id).first;
	}

	const T& at(const Handle& handle) const {
		return map_.at(handle.id).first;
	}

private:
	using Item = std::pair<T, uint32_t>;
	uint16_t get_id() {
		if (recycle_buffer_.empty()) {
			return frontier++;
		}
		else {
			uint16_t id = recycle_buffer_.front();
			recycle_buffer_.pop_front();
			return id;
		}
	}

	template<class ...Args>
	const Item& get_or_create_item(uint16_t id, Args&&... args) {
		if (auto it = map_.find(id); it != map_.end()) {
			return it->second;
		}
		else {
			map_.emplace(id, std::make_pair(std::move(T(std::forward<Args>(args)...)), 0));
			return map_.find(id)->second;
		}
	}

	tsl::robin_map<uint32_t, Item> map_;
	std::deque<uint32_t> recycle_buffer_;
	uint32_t frontier = 0;
};
