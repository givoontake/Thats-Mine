#pragma once
#include <string>
#include <vector>
#include "hitbox_types.h"

// 언리얼에서 내보낸 판정용 데이터(뼈 계층, 히트박스, 애니메이션 키, 재생 규칙). 서버 시작 시 한 번 읽고 이후에는 읽기만 하므로 스레드끼리 잠금 없이 함께 쓴다.
class HitboxData
{
	std::vector<std::string> bone_names_;
	std::vector<int> bone_parents_;
	float head_pitch_up_limit_ = 0.0f;
	float head_pitch_down_limit_ = 0.0f;
	std::vector<WeaponInfo> weapon_infos_;
	std::vector<HitboxBody> bodies_;
	std::vector<AnimationTrack> tracks_;
	std::vector<LocomotionSample> locomotion_samples_;
	std::vector<int> pose_tracks_;
	int jump_track_ = -1;
	std::vector<MontageInfo> montages_;
	std::vector<BoneSphere> bone_spheres_;
	// 데이터를 읽은 뒤 판정용 자세를 훑어 구한 1차 판정 캡슐
	BroadphaseCapsule broadphase_capsule_{};

	// 자세 계산에서 따로 다루는 뼈
	int hips_bone_ = -1;
	int spine_bone_ = -1;
	int chest_bone_ = -1;
	int head_bone_ = -1;
	int upperarm_r_bone_ = -1;

public:
	bool Load(const std::string& file_path);
	bool IsLoaded() const { return !tracks_.empty(); }

	int GetBoneCount() const { return static_cast<int>(bone_parents_.size()); }
	int GetBoneParent(int bone) const { return bone_parents_[bone]; }
	int FindBone(const std::string& name) const;
	// bone이 ancestor 자신이거나 그 아래 뼈인지 확인한다.
	bool IsInBranch(int bone, int ancestor) const;

	const BroadphaseCapsule& GetBroadphaseCapsule() const { return broadphase_capsule_; }
	const BoneSphere& GetBoneSphere(int bone) const { return bone_spheres_[bone]; }
	float GetHeadPitchUpLimit() const { return head_pitch_up_limit_; }
	float GetHeadPitchDownLimit() const { return head_pitch_down_limit_; }
	std::uint8_t GetWeaponCount() const { return static_cast<std::uint8_t>(weapon_infos_.size()); }
	const WeaponInfo* GetWeaponInfo(std::uint8_t weapon) const { return weapon < weapon_infos_.size() ? &weapon_infos_[weapon] : nullptr; }
	const std::vector<HitboxBody>& GetBodies() const { return bodies_; }
	const AnimationTrack& GetTrack(int track) const { return tracks_[track]; }
	const std::vector<LocomotionSample>& GetLocomotionSamples() const { return locomotion_samples_; }
	int GetPoseTrack(UpperBodyPose pose) const { return pose_tracks_[static_cast<int>(pose)]; }
	int GetJumpTrack() const { return jump_track_; }
	// 동작과 무기에 맞는 몽타주를 찾는다. 무기와 상관없는 동작도 찾는다. 없으면 nullptr다.
	const MontageInfo* FindMontage(std::uint8_t action, std::uint8_t weapon) const;
	// 동작과 무기에 맞는 몽타주가 재생 속도로 재생되는 시간(ms). 몽타주가 없으면 0이다.
	long long GetMontageMs(std::uint8_t action, std::uint8_t weapon) const;
	const std::vector<MontageInfo>& GetMontages() const { return montages_; }

	int GetHipsBone() const { return hips_bone_; }
	int GetSpineBone() const { return spine_bone_; }
	int GetChestBone() const { return chest_bone_; }
	int GetHeadBone() const { return head_bone_; }
	int GetUpperarmRBone() const { return upperarm_r_bone_; }
};
