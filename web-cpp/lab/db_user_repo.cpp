#include "db_user_repo.hpp"

void DbUserRepository::db_worker_loop() {
	for (;;) {
		DbTask task;

		{
			std::unique_lock<std::mutex> lock(g_db_queue_mutex);
			g_db_cv.wait(lock, [&] {
				return g_db_stop || !g_db_tasks.empty();
				});

			if (g_db_stop && g_db_tasks.empty()) {
				return;
			}

			task = std::move(g_db_tasks.front());
			g_db_tasks.pop();
		}

		try {
			db_insert_dot(task.login, task.dot);
		}
		catch (const std::exception& e) {
			std::cerr << "Async DB insert failed for user " << task.login
				<< ": " << e.what() << "\n";
		}
	}
}


bool DbUserRepository::push_task(const DbTask& task) {
	{
		std::lock_guard<std::mutex> qlock(g_db_queue_mutex);
		g_db_tasks.push(task);
	}
	g_db_cv.notify_one();
	return true;
}

DbUserRepository::DbUserRepository(const std::string coninfo) : g_conninfo(coninfo) {


	this->init_db();
	std::thread worker(&DbUserRepository::db_worker_loop, this);
	worker.detach();
}

DbUserRepository::~DbUserRepository() {
	this->g_db_stop = true;
	this->g_db_cv.notify_all();
}