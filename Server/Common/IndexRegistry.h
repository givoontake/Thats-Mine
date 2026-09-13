#pragma once
#include <cstddef>
#include <mutex>
#include <optional>
#include <unordered_map>

template <typename Key>
class IndexRegistry
{
	std::unordered_map<Key, int> indices_;
	mutable std::mutex mutex_;

public:
	explicit IndexRegistry(std::size_t capacity = 0)
	{
		indices_.reserve(capacity);
	}

	bool Register(Key key, int index)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return indices_.try_emplace(key, index).second;
	}

	bool Unregister(Key key, int index)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = indices_.find(key);
		if (it == indices_.end() || it->second != index) return false;
		indices_.erase(it);
		return true;
	}

	std::optional<int> Find(Key key) const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = indices_.find(key);
		if (it == indices_.end()) return std::nullopt;
		return it->second;
	}

	bool Contains(Key key) const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return indices_.contains(key);
	}

	void Clear()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		indices_.clear();
	}
};
