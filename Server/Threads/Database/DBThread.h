// DBThread.h
#pragma once
#include <condition_variable>
#include <queue>
#include "db_login_task.h"
#include "db_login_result.h"
#include "db_session_task.h"
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
	// 계정 조회 요청. 이 스레드가 세션 작업을 처리하며 넣고, DB 연결이 있을 때 순서대로 처리한다. 이 스레드만 쓰므로 잠그지 않는다.
	std::queue<DBLoginTask> task_queue_;
	virtual DBLoginResult ProcessTask(const DBLoginTask& task) = 0;
	// 조회 결과를 처리한다. 조회를 맡은 스레드가 결과를 받은 자리에서 바로 처리한다.
	virtual void HandleResult(SessionKey session_key, DBLoginResult result) = 0;
	// DB를 쓰지 않는 세션 작업(새 세션, 받은 패킷, 연결 종료 정리)을 처리한다.
	virtual void ProcessSessionTask(const DBSessionTask& task) = 0;

private:
	// DB를 쓰지 않는 세션 작업. 다른 스레드가 넣으며, DB 연결이 끊겨 다시 연결하는 동안에도 처리한다.
	std::queue<DBSessionTask> session_task_queue_;
	DBConnectionInfo connection_info_;
	std::mutex wait_mutex_;
	std::condition_variable cv_;

	bool LoadDBConfigFromFile(const std::string& file_path);
	bool Connect();
	void Disconnect();
	bool IsConnectionAlive();
	static bool IsExpired(const DBLoginTask& task);
	void DiscardExpiredTasks();
	void ProcessSessionTasks();
	void WaitReconnect();
	static void PrintErrorLog(const char* func_name, const std::exception& e);
	static void PrintErrorLog(const char* func_name, const sql::SQLException& e);
	static void PrintErrorLog(const char* func_name);

public:
	DBThread();
	~DBThread() override;
	DBThread(const DBThread&) = delete;
	DBThread& operator=(const DBThread&) = delete;
	void Init(sql::mysql::MySQL_Driver* driver);
	bool Enqueue(DBSessionTask task);
	void Run() override;
	void Close() override;
};
