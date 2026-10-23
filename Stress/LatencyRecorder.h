#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

// 잴 지연 종류. 이동은 내 이동을 보낸 뒤 서버 중계로 돌아오기까지, 발사는 발사를 보낸 뒤 서버의 탄약 사용 알림을 받기까지다.
enum class LatencyKind { MOVE, FIRE, COUNT };

// 측정 시간 동안 종류마다 지연을 모으고, 끝나면 평균과 p99를 결과 파일로 남긴다.
class LatencyRecorder
{
	std::string output_directory_;
	std::chrono::steady_clock::time_point finish_time_;
	std::array<std::vector<long long>, static_cast<std::size_t>(LatencyKind::COUNT)> latency_samples_;
	std::mutex state_mutex_;
	std::mutex samples_mutex_;
	std::atomic<bool> is_started_{ false };
	std::atomic<bool> is_finishing_{ false };
	std::atomic<bool> is_finished_{ false };
	std::array<std::atomic<std::size_t>, static_cast<std::size_t>(LatencyKind::COUNT)> sample_counts_{};

public:
	LatencyRecorder(std::string output_directory);

	bool TryStart();
	void Record(LatencyKind kind, long long latency_ms);
	bool TryFinish();
	bool IsStarted() const { return is_started_.load(); }
	bool IsFinished() const { return is_finished_.load(); }
	// 결과 파일에 남긴 표본 수. 측정이 끝난 뒤 확인용으로 출력한다.
	std::size_t GetSampleCount(LatencyKind kind) const { return sample_counts_[static_cast<std::size_t>(kind)].load(); }

private:
	bool IsMeasuring() const { return is_started_.load() && !is_finishing_.load(); }
	void SaveResult();
	long long CalculatePercentile(const std::vector<long long>& sorted_samples, double percentile) const;
};
