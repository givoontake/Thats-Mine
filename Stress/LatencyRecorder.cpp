#include "LatencyRecorder.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <utility>
#include "stress_settings.h"

LatencyRecorder::LatencyRecorder(std::string output_directory)
	: output_directory_(std::move(output_directory))
{
}

bool LatencyRecorder::TryStart()
{
	std::lock_guard<std::mutex> lock(state_mutex_);
	if (is_started_.load()) return false;
	finish_time_ = std::chrono::steady_clock::now() + std::chrono::seconds(MEASUREMENT_SECONDS);
	is_started_.store(true);
	return true;
}

void LatencyRecorder::Record(LatencyKind kind, long long latency_ms)
{
	if (!IsMeasuring() || std::chrono::steady_clock::now() >= finish_time_) return;
	std::lock_guard<std::mutex> lock(samples_mutex_);
	latency_samples_[static_cast<std::size_t>(kind)].emplace_back(latency_ms);
}

bool LatencyRecorder::TryFinish()
{
	if (!is_started_.load() || std::chrono::steady_clock::now() < finish_time_) return false;
	bool expected = false;
	if (!is_finishing_.compare_exchange_strong(expected, true)) return false;
	SaveResult();
	is_finished_.store(true);
	return true;
}

void LatencyRecorder::SaveResult()
{
	std::array<std::vector<long long>, static_cast<std::size_t>(LatencyKind::COUNT)> sorted_samples;
	{
		std::lock_guard<std::mutex> lock(samples_mutex_);
		sorted_samples = latency_samples_;
	}

	std::filesystem::create_directories(output_directory_);
	std::ofstream output(std::filesystem::path(output_directory_) / "latency.txt", std::ios::trunc);
	const char* KIND_NAMES[] = { "move", "fire" };
	for (std::size_t kind = 0; kind < sorted_samples.size(); ++kind) {
		std::vector<long long>& samples = sorted_samples[kind];
		sample_counts_[kind].store(samples.size());
		std::sort(samples.begin(), samples.end());
		const long long total_latency = std::accumulate(samples.begin(), samples.end(), 0LL);
		const double average_latency = samples.empty() ? 0.0 : static_cast<double>(total_latency) / samples.size();
		output << KIND_NAMES[kind] << "_average_ms=" << average_latency << '\n';
		output << KIND_NAMES[kind] << "_p99_ms=" << CalculatePercentile(samples, 0.99) << '\n';
	}
}

long long LatencyRecorder::CalculatePercentile(const std::vector<long long>& sorted_samples, double percentile) const
{
	if (sorted_samples.empty()) return 0;
	const std::size_t index = (std::min)(sorted_samples.size() - 1, static_cast<std::size_t>(std::ceil(sorted_samples.size() * percentile)) - 1);
	return sorted_samples[index];
}
