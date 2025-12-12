#pragma once
#include "models.hpp"

#include <condition_variable>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <queue>

//#define USE_PQXX
#ifdef USE_PQXX
#include <pqxx/pqxx>
#endif

class DbUserRepository {
private:
#ifdef USE_PQXX
	std::unique_ptr<pqxx::connection> g_db;
#endif

	std::queue<DbTask> g_db_tasks;
	std::mutex g_db_queue_mutex;
	std::condition_variable g_db_cv;
	bool g_db_stop = false;
	std::mutex g_db_mutex;

	const std::string g_conninfo;

private:

	void init_db();

	bool db_ensure_connection_unlocked();

	void db_worker_loop();

public:

	bool db_ensure_connection();

	bool db_create_user(const std::string& login, const std::string& password);

	bool db_check_password(const std::string& login, const std::string& password);

	bool db_delete_user(const std::string& login);

	void db_insert_dot(const std::string& login, const DotView& d);

	std::vector<DotView> db_get_dots(const std::string& login);

	void db_clear_dots(const std::string& login);

public:

	bool push_task(const DbTask& task);

	DbUserRepository(const std::string coninfo);

	~DbUserRepository();
};