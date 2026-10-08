#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>

// 탑 배치 후보 자리를 서버 시작 때 읽어 둔다. 후보 번호는 파일의 줄 순서다.
// 읽은 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 읽는다.
class TowerCandidateData
{
	std::vector<DirectX::XMFLOAT3> candidates_;
	bool is_loaded_ = false;

public:
	bool Load(const std::string& file_path);
	bool IsLoaded() const { return is_loaded_; }
	// 후보마다 탑 바닥 가운데 위치(cm)
	const std::vector<DirectX::XMFLOAT3>& GetCandidates() const { return candidates_; }
};
