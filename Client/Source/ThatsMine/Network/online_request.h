#pragma once

// MATCHING: 빠른 매칭 결과를 기다리는 중, CANCEL_MATCHING: 빠른 매칭 취소를 보내고 결과를 기다리는 중
enum class OnlineRequest { NONE, LOGIN, ENTER_ROOM, MATCHING, CANCEL_MATCHING, READY, START, LEAVE, KICK };
