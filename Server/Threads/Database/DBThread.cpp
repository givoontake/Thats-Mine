// DBThread.cpp
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <optional>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <jdbc/cppconn/connection.h>
#include <jdbc/cppconn/driver.h>
#include "DBThread.h"
#include "server_settings.h"


// -------------------- 유틸(로컬) --------------------
static inline void Trim(std::string& s)
{
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };

    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

// -------------------- DBThread --------------------

DBThread::DBThread()
{
}

DBThread::~DBThread()
{
}

void DBThread::Init(sql::mysql::MySQL_Driver* driver)
{
	driver_ = driver;
}

void DBThread::Close()
{
    {
        std::lock_guard<std::mutex> lock(wait_mutex_);
        is_running_ = false;
    }
    cv_.notify_one();
}

bool DBThread::LoadDBConfigFromFile(const std::string& file_path)
{
    std::ifstream file(file_path);
    if (!file.is_open())
        return false;

    std::string line;
    while (std::getline(file, line))
    {
        // CR 제거(윈도우 \r\n 대응)
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        Trim(line);
        if (line.empty() || line[0] == '#')
            continue;

        auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        Trim(key);
        Trim(val);

        if (key == "host")
            connection_info_.host = val;
        else if (key == "port")
        {
            try
            {
                int p = std::stoi(val);
                if (p < 0) p = 0;
                if (p > 65535) p = 65535;
                connection_info_.port = static_cast<uint16_t>(p);
            }
            catch (...)
            {
                connection_info_.port = 0;
                PrintErrorLog(__func__);
            }
        }
        else if (key == "user")
            connection_info_.id = val;
        else if (key == "password")
            connection_info_.password = val;
        else if (key == "schema")
            connection_info_.schema = val;
    }

    return true;
}

// ---- DB 스레드 루프 ----
bool DBThread::Enqueue(DBSessionTask task)
{
	{
		std::lock_guard<std::mutex> lock(wait_mutex_);
		if (!is_running_.load()) return false;
		session_task_queue_.push(std::move(task));
	}
	cv_.notify_one();
	return true;
}

// 연결이 없으면 성공할 때까지 간격을 두고 다시 연결한다. 그사이 요청은 큐에 그대로 두되, 너무 오래 기다린 요청은 시간 초과로 응답하고 버린다.
// 조회 중 연결이 끊기면 그 요청을 들고 있다가 다시 연결한 뒤 큐보다 먼저 처리해 순서를 지킨다.
// 연결 끊김이 아닌 오류는 다시 해도 같은 결과이므로 그 요청만 실패로 응답한다.
// DB를 쓰지 않는 세션 작업은 조회 요청보다 먼저, 연결이 없을 때도 처리한다. 조회 요청은 세션 작업(받은 로그인 패킷)에서 생기므로,
// 같은 세션의 새 세션 작업은 언제나 그 조회 전에 처리된다.
void DBThread::Run()
{
	LoadDBConfigFromFile("db_config.txt");
	driver_->threadInit();
	std::optional<DBLoginTask> retry_task;
	while (is_running_.load()) {
		ProcessSessionTasks();
		if (!connection_) {
			// 연결 시도는 정해진 시간까지 막힐 수 있으므로, 너무 오래 기다린 요청을 먼저 정리해 응답이 늦어지지 않게 한다.
			if (retry_task && IsExpired(*retry_task)) {
				HandleResult(retry_task->session_key, { ErrorCode::LOGIN_TIMEOUT });
				retry_task.reset();
			}
			DiscardExpiredTasks();
			if (Connect()) continue;
			WaitReconnect();
			continue;
		}

		DBLoginTask task;
		if (retry_task) {
			task = std::move(*retry_task);
			retry_task.reset();
		}
		else if (!task_queue_.empty()) {
			task = std::move(task_queue_.front());
			task_queue_.pop();
		}
		else {
			// 조회 요청은 세션 작업에서만 생기므로, 세션 작업이 들어올 때까지 기다린다.
			std::unique_lock<std::mutex> lock(wait_mutex_);
			cv_.wait(lock, [this]() { return !is_running_.load() || !session_task_queue_.empty(); });
			continue;
		}
		if (IsExpired(task)) {
			HandleResult(task.session_key, { ErrorCode::LOGIN_TIMEOUT });
			continue;
		}

		DBLoginResult result;
		try {
			result = ProcessTask(task);
		}
		catch (const sql::SQLException& e) {
			PrintErrorLog(__func__, e);
			if (!IsConnectionAlive()) {
				std::cerr << "[DB] Connection lost. Reconnecting." << std::endl;
				Disconnect();
				retry_task = std::move(task);
				continue;
			}
			result.error_code = ErrorCode::LOGIN_QUERY_FAILED;
		}
		catch (const std::exception& e) {
			PrintErrorLog(__func__, e);
			result.error_code = ErrorCode::LOGIN_QUERY_FAILED;
		}
		HandleResult(task.session_key, std::move(result));
	}
	{
		std::lock_guard<std::mutex> lock(wait_mutex_);
		while (!session_task_queue_.empty()) session_task_queue_.pop();
	}
	while (!task_queue_.empty()) task_queue_.pop();
	Disconnect();
	driver_->threadEnd();
}

// 쌓인 세션 작업을 모두 꺼내 들어온 순서대로 처리한다. 처리는 잠금 밖에서 한다.
void DBThread::ProcessSessionTasks()
{
	std::queue<DBSessionTask> session_tasks;
	{
		std::lock_guard<std::mutex> lock(wait_mutex_);
		session_tasks.swap(session_task_queue_);
	}
	while (!session_tasks.empty()) {
		ProcessSessionTask(session_tasks.front());
		session_tasks.pop();
	}
}

// 다음 연결 시도까지 기다린다. 기다리는 동안 들어온 세션 작업은 바로 처리한다.
void DBThread::WaitReconnect()
{
	const auto retry_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(DB_RECONNECT_INTERVAL_MS);
	while (is_running_.load()) {
		{
			std::unique_lock<std::mutex> lock(wait_mutex_);
			if (!cv_.wait_until(lock, retry_time, [this]() { return !is_running_.load() || !session_task_queue_.empty(); })) return;
		}
		ProcessSessionTasks();
	}
}

bool DBThread::IsConnectionAlive()
{
	try {
		return connection_ && !connection_->isClosed() && connection_->isValid();
	}
	catch (const sql::SQLException&) {
		return false;
	}
}

bool DBThread::IsExpired(const DBLoginTask& task)
{
	const long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	return now_ms - task.request_time_ms > DB_REQUEST_TIMEOUT_MS;
}

// 큐 앞쪽부터 너무 오래 기다린 요청을 꺼내 시간 초과로 응답한다.
void DBThread::DiscardExpiredTasks()
{
	while (!task_queue_.empty() && IsExpired(task_queue_.front())) {
		HandleResult(task_queue_.front().session_key, { ErrorCode::LOGIN_TIMEOUT });
		task_queue_.pop();
	}
}

bool DBThread::Connect()
{
    try
    {
        // Connector/C++ legacy는 보통 tcp://host:port
        const std::string url = "tcp://" + connection_info_.host + ":" + std::to_string(connection_info_.port);

		sql::ConnectOptionsMap options;
		options["hostName"] = sql::SQLString(url);
		options["userName"] = sql::SQLString(connection_info_.id);
		options["password"] = sql::SQLString(connection_info_.password);
		options[OPT_CONNECT_TIMEOUT] = DB_CONNECT_TIMEOUT_SECONDS;
		options[OPT_READ_TIMEOUT] = DB_IO_TIMEOUT_SECONDS;
		options[OPT_WRITE_TIMEOUT] = DB_IO_TIMEOUT_SECONDS;
		connection_.reset(driver_->connect(options));
		connection_->setSchema(connection_info_.schema);

        return true;
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        // 연결 뒤 스키마 지정에서 실패했을 수도 있으므로 연결을 버려, 다음 시도에서 처음부터 다시 연결하게 한다.
        connection_.reset();
        return false;
    }
}

void DBThread::Disconnect()
{

	if (connection_)
    {
		try { connection_->close(); }
        catch (...) {}
		connection_.reset();
    }
}

void DBThread::PrintErrorLog(const char* func_name, const std::exception& e)
{
    std::cerr << "[ERROR] in " << func_name << "\n"
        << "  type: " << typeid(e).name() << "\n"
        << "  what(): " << e.what() << std::endl;
}

void DBThread::PrintErrorLog(const char* func_name, const sql::SQLException& e)
{
    std::cerr << "[DB][SQLException] in " << func_name << "\n"
        << "  what(): " << e.what() << "\n"
        << "  errorCode: " << e.getErrorCode() << "\n"
        << "  SQLState: " << e.getSQLState() << std::endl;
}

void DBThread::PrintErrorLog(const char* func_name)
{
    std::cerr << "[ERROR] in " << func_name
        << "  (unknown exception)" << std::endl;
}
