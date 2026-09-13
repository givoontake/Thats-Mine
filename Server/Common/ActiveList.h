#pragma once
#include <algorithm>
#include <cstddef>
#include <mutex>
#include <optional>
#include <vector>

template <typename T>
class ActiveList
{
	std::vector<T> values_;
	mutable std::mutex mutex_;

public:
	explicit ActiveList(std::size_t capacity = 0)
	{
		values_.reserve(capacity);
	}

	bool Add(const T& value)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (std::find(values_.begin(), values_.end(), value) != values_.end()) return false;
		values_.emplace_back(value);
		return true;
	}

	bool Remove(const T& value)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = std::find(values_.begin(), values_.end(), value);
		if (it == values_.end()) return false;
		values_.erase(it);
		return true;
	}

	std::optional<T> Get(std::size_t index) const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (index >= values_.size()) return std::nullopt;
		return values_[index];
	}

	std::size_t GetCount() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return values_.size();
	}

	void Clear()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		values_.clear();
	}
};
