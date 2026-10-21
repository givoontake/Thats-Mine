// DBThread.h
#pragma once
#include <condition_variable>
#include <queue>
#include "db_login_task.h"
#include "db_login_result.h"
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
protected:
	std::unique_ptr<sql::Connection> connection_;
	virtual DBLoginResult ProcessTask(const DBLoginTask& task) = 0;
	// 조회 결과를 처리한다. 조회를 맡은 스레드가 결과를 받은 자리에서 바로 처리한다.
	virtual void HandleResult(SessionKey session_key, DBLoginResult result) = 0;

private:
	std::queue<DBLoginTask> task_queue_;
	DBConnectionInfo connection_info_;
	std::mutex wait_mutex_;
	std::condition_variable cv_;

	bool LoadDBConfigFromFile(const std::string& file_path);
	bool Connect();
	void Disconnect();
	bool IsConnectionAlive();
	static bool IsExpired(const DBLoginTask& task);
	void DiscardExpiredTasks();
	static void PrintErrorLog(const char* func_name, const std::exception& e);
	static void PrintErrorLog(const char* func_name, const sql::SQLException& e);
	static void PrintErrorLog(const char* func_name);

public:
	DBThread();
	~DBThread() override;
	DBThread(const DBThread&) = delete;
	DBThread& operator=(const DBThread&) = delete;
	void Init(sql::mysql::MySQL_Driver* driver);
	bool Enqueue(DBLoginTask task);
	void Run() override;
	void Close() override;
};
