#pragma once
// 서버 이동 검증.

// 이동 검증. 어기면 클라이언트를 서버 이동 상태로 맞춘다.
// 허용치(cm): 캡슐 아래 끝(발)이 랜드스케이프보다 이만큼 넘게 아래면 땅속이고, 받은 위치가 그 시각의 서버 이동 상태에서 이만큼 넘게 벗어나면 인정하지 않는다.
// 서버 이동 상태에서 받은 위치까지 쓸 때 캡슐을 이만큼 줄인다.
constexpr float MOVE_TOLERANCE = 20.0f;
// 공중 이동 끊김. 공중에서 이 시간(초) 넘게 이동 상태가 오지 않으면 그 클라이언트를 서버 이동 상태(공중 궤적)로 맞춘다.
constexpr float MOVE_AIR_DELAY_SECONDS = 0.5f;
// 이동 끊김. 이 시간(초) 넘게 이동 상태가 오지 않으면 서버가 그 플레이어를 서버 이동 상태 자리에 세우고 다른 플레이어에게 멈춤을 알린다. 그 전까지는 서버 이동 상태를 계속 중계한다.
// 공중 이동 끊김과 같은 값이다.
constexpr float MOVE_STOP_DELAY_SECONDS = MOVE_AIR_DELAY_SECONDS;
// 땅 확인. 캡슐 아래 이만큼(cm) 안에 랜드스케이프나 캐릭터를 막는 배치물 바닥이 있으면 땅 위다. 걸을 수 있는 가장 가파른 경사를 덮는 깊이다.
constexpr float GROUND_CHECK_DEPTH = 30.0f;
// 클라이언트 시각 검사. 이동 상태 사이 "클라이언트 시각 차이 - 서버가 받은 시각 차이"를 쌓아 클라이언트 시각이 이만큼(ms) 넘게 앞서면 시간을 빠르게 돌린 것으로 보고 되돌린다.
// 늦은 쪽은 CLOCK_MAX_BEHIND_MS까지만 쌓아, 오래 끊겼다가 한 번에 몰아 보내는 몫을 무한히 미리 쌓아 두지 못하게 한다.
constexpr long long CLOCK_MAX_AHEAD_MS = 250;
constexpr long long CLOCK_MAX_BEHIND_MS = 2000;
