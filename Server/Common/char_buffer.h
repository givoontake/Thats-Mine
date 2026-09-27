#pragma once
#include <algorithm>
#include <cstring>
#include <string>

// 패킷의 고정 길이 문자 배열과 문자열 사이의 변환 도구. 서버 상태와 무관하므로 일반 함수로 둔다.

// 문자열을 고정 길이 배열에 복사한다. 남는 부분은 0으로 채운다.
inline void StringToCharBuf(const std::string& str, char* buf, int buf_size)
{
	memset(buf, 0, buf_size);
	const size_t copy_size = std::min(str.size(), static_cast<size_t>(buf_size));
	memcpy(buf, str.data(), copy_size);
}

// 고정 길이 배열에서 첫 0 앞까지를 문자열로 만든다.
inline std::string CharBufToString(const char* buf, int buf_size)
{
	return std::string(buf, strnlen(buf, static_cast<size_t>(buf_size)));
}

// 고정 길이 배열을 복사한다. 첫 0 뒤의 값은 버리고 남는 부분을 0으로 채워, 배열 전체 비교가 정확하게 되도록 한다.
inline void CopyCharBuf(char* dst, int dst_size, const char* src, int src_size)
{
	memset(dst, 0, dst_size);
	if (!src) return;
	const size_t copy_size = std::min(strnlen(src, static_cast<size_t>(src_size)), static_cast<size_t>(dst_size));
	memcpy(dst, src, copy_size);
}
