// DBThread.cpp
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <jdbc/cppconn/connection.h>
#include <jdbc/cppconn/driver.h>
#include "DBThread.h"
#include "DBOverlapped.h"

#undef min

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

void DBThread::Init(HANDLE iocp_handle, sql::mysql::MySQL_Driver* driver)
{
	iocp_handle_ = iocp_handle;
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
bool DBThread::Enqueue(DBLoginTask task)
{
	{
		std::lock_guard<std::mutex> lock(wait_mutex_);
		if (!is_running_.load()) return false;
		task_queue_.push(std::move(task));
	}
	cv_.notify_one();
	return true;
}

DBLoginResult DBThread::ProcessTask(const DBLoginTask&)
{
	return {};
}

void DBThread::PostResult(SessionKey session_key, DBLoginResult result)
{
	auto over = std::make_unique<DBOverlapped>();
	over->ex_over.op_type = OPType::DB;
	over->ex_over.session_key = session_key;
	over->result = std::move(result);
	PostQueuedCompletionStatus(iocp_handle_, 0, DB_SESSION_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(over.release()));
}

void DBThread::Run()
{
	LoadDBConfigFromFile("db_config.txt");
	driver_->threadInit();
	if (!Connect()) {
		std::cerr << "[DB] Failed to connect to database." << std::endl;
		{
			std::lock_guard<std::mutex> lock(wait_mutex_);
			is_running_.store(false);
		}
		while (!task_queue_.empty()) {
			PostResult(task_queue_.front().session_key, {});
			task_queue_.pop();
		}
		Disconnect();
		driver_->threadEnd();
		return;
	}
	while (is_running_.load()) {
		DBLoginTask task;
		{
			std::unique_lock<std::mutex> lock(wait_mutex_);
			cv_.wait(lock, [this]() { return !is_running_.load() || !task_queue_.empty(); });
			if (!is_running_.load()) break;
			task = std::move(task_queue_.front());
			task_queue_.pop();
		}
		DBLoginResult result;
		try {
			result = ProcessTask(task);
		}
		catch (const sql::SQLException& e) {
			PrintErrorLog(__func__, e);
			Enqueue(std::move(task));
			continue;
		}
		catch (const std::exception& e) {
			PrintErrorLog(__func__, e);
		}
		PostResult(task.session_key, std::move(result));
	}
	{
		std::lock_guard<std::mutex> lock(wait_mutex_);
		while (!task_queue_.empty()) task_queue_.pop();
	}
	Disconnect();
	driver_->threadEnd();
}

bool DBThread::Connect()
{
    try
    {
        // Connector/C++ legacy는 보통 tcp://host:port
        const std::string url = "tcp://" + connection_info_.host + ":" + std::to_string(connection_info_.port);

		connection_.reset(driver_->connect(url, connection_info_.id, connection_info_.password));
		connection_->setSchema(connection_info_.schema);

        return true;
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
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
