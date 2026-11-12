#pragma once
// 기절.

// 기절 게이지. 명중당하면 무기별 증가량에 맞은 부위 배율을 곱해 올리고, 마지막으로 오른 뒤 감소 대기 시간이 지나면 초당 감소량만큼 줄어든다. 최대치에 닿으면 기절한다.
constexpr float STUN_GAUGE_MAX = 100.0f;
constexpr float STUN_GAUGE_DECAY_PER_SECOND = 4.0f;
constexpr int STUN_GAUGE_DECAY_DELAY_MS = 5000;
constexpr float STUN_GAUGE_PART_MULTIPLIERS[] = { 1.1f, 1.0f, 0.8f, 0.8f, 0.8f, 0.8f }; // 부위 값 순서: 머리, 몸통, 팔, 손, 다리, 발
constexpr int STUN_DURATION_MS = 5000; // 기절 시간. 기절 동안 서버는 그 플레이어의 이동 입력(이동 키·조준·점프)·동작·명중을 받지 않고, 끝나면 게이지는 0이다.
constexpr int STUN_PROTECTION_MS = 30000; // 기절이 끝난 뒤 보호 시간. 보호 동안 명중당해도 기절 게이지가 오르지 않는다.
