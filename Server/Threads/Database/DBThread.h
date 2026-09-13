// DBThread.h
#pragma once
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/exception.h>
#include "ServerThread.h"

struct DBConnectionInfo
{
	std::string host;
	std::uint16_t port = 0;
	std::string id;
	std::string password;
	std::string schema;
};

class DBThread : public ServerThread
{
	sql::mysql::MySQL_Driver* driver_ = nullptr;
	std::unique_ptr<sql::Connection> connection_;
	DBConnectionInfo connection_info_;
	std::mutex wait_mutex_;
	std::condition_variable cv_;

	bool LoadDBConfigFromFile(const std::string& file_path);
	bool Connect();
	void Disconnect();
	static void PrintErrorLog(const char* func_name, const std::exception& e);
	static void PrintErrorLog(const char* func_name, const sql::SQLException& e);
	static void PrintErrorLog(const char* func_name);

public:
	DBThread();
	~DBThread() override;
	DBThread(const DBThread&) = delete;
	DBThread& operator=(const DBThread&) = delete;
	void Init(sql::mysql::MySQL_Driver* driver);
	void Run() override;
	void Close() override;
};
