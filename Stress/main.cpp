#include <WinSock2.h>
#include <Windows.h>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "TestManager.h"

// 사용법: Stress.exe <서버 IPv4> <봇 수> <결과 폴더>. 봇 수는 ROOM_CLIENT_COUNT의 배수여야 한다.
int main(int argc, char* argv[])
{
	if (argc < 4) {
		std::cerr << "usage: Stress.exe <server ip> <client count> <output directory>" << std::endl;
		return 1;
	}
	const int client_count = std::stoi(argv[2]);
	if (client_count <= 0 || client_count > MAX_CLIENT_COUNT || client_count % ROOM_CLIENT_COUNT != 0) {
		std::cerr << "client count must be a multiple of " << ROOM_CLIENT_COUNT << " up to " << MAX_CLIENT_COUNT << std::endl;
		return 1;
	}
	std::unique_ptr<TestManager> test_manager;
	try {
		test_manager = std::make_unique<TestManager>(argv[1], argv[3]);
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}

	std::vector<std::thread> threads;
	for (int i = 0; i < IO_THREAD_COUNT; ++i) threads.emplace_back([&test_manager] { while (true) test_manager->ProcessIO(); });
	for (int i = 0; i < CONNECT_THREAD_COUNT; ++i) threads.emplace_back([&test_manager] { test_manager->ProcessConnect(); });
	for (int i = 0; i < SEND_THREAD_COUNT; ++i) threads.emplace_back([&test_manager, i] {
		while (true) {
			test_manager->ProcessSend(i);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	});

	test_manager->Start(client_count);
	while (!test_manager->IsMeasurementFinished()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
	ExitProcess(0);
}
