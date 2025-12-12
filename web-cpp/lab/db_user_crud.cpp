#include "db_user_repo.hpp"

bool DbUserRepository::db_ensure_connection_unlocked() {
#ifdef USE_PQXX
	try {
		if (!g_db || !g_db->is_open()) {
			g_db = std::make_unique<pqxx::connection>(g_conninfo);
		}
		return g_db && g_db->is_open();
	}
	catch (const std::exception& e) {
		std::cerr << "DB connection error: " << e.what() << std::endl;
		g_db.reset();
		return false;
	}
#else
	return true;
#endif
}

bool DbUserRepository::db_ensure_connection() {
	std::lock_guard<std::mutex> lock(g_db_mutex);
	return db_ensure_connection_unlocked();
}

void DbUserRepository::init_db() {
#ifdef USE_PQXX
	std::lock_guard<std::mutex> lock(g_db_mutex);

	if (!db_ensure_connection_unlocked()) {
		std::cerr << "WARNING: PostgreSQL not available at startup. "
			"Endpoints that need DB will return 503 until DB is reachable.\n";
		return;
	}

	try {
		pqxx::work tx(*g_db);


		tx.exec(
			"CREATE TABLE IF NOT EXISTS users ("
			"  id              BIGSERIAL PRIMARY KEY,"
			"  login           TEXT NOT NULL UNIQUE,"
			"  hashed_password TEXT NOT NULL"
			")"
		);

		tx.exec(
			"CREATE TABLE IF NOT EXISTS dots ("
			"  id         BIGSERIAL PRIMARY KEY,"
			"  x          TEXT NOT NULL,"
			"  y          TEXT NOT NULL,"
			"  r          TEXT NOT NULL,"
			"  hit        BOOLEAN NOT NULL,"
			"  exec_time  BIGINT NOT NULL,"
			"  cur_time   TEXT NOT NULL,"
			"  user_id    BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE"
			")"
		);

		tx.commit();
		std::cout << "Connected to PostgreSQL, schema OK\n";
	}
	catch (const std::exception& e) {
		std::cerr << "DB schema init error: " << e.what() << std::endl;
		g_db.reset();
	}
#endif
}


bool DbUserRepository::db_create_user(const std::string& login, const std::string& password) {
#ifdef USE_PQXX
	std::lock_guard<std::mutex> lock(g_db_mutex);

	if (!db_ensure_connection_unlocked()) {
		throw std::runtime_error("DB unavailable");
	}

	try {
		pqxx::work tx(*g_db);
		tx.exec(
			"INSERT INTO users(login, hashed_password) VALUES($1, $2)",
			pqxx::params{ login, password }
		);
		tx.commit();
		return true;
	}
	catch (const pqxx::unique_violation&) {
		
		return false;
	}
	catch (const std::exception& e) {
		std::cerr << "db_create_user error: " << e.what() << std::endl;
		throw;
	}
#else 
	return true;
#endif
}

bool DbUserRepository::db_check_password(const std::string& login, const std::string& password) {
#ifdef USE_PQXX

	std::lock_guard<std::mutex> lock(g_db_mutex);

	if (!db_ensure_connection_unlocked()) {
		throw std::runtime_error("DB unavailable");
	}

	try {
		pqxx::work tx(*g_db);
		auto res = tx.exec(
			"SELECT hashed_password FROM users WHERE login = $1",
			pqxx::params{ login }
		);
		tx.commit();

		if (res.empty()) return false;
		std::string stored = res[0][0].as<std::string>();

		return stored == password;
	}
	catch (const std::exception& e) {
		std::cerr << "db_check_password error: " << e.what() << std::endl;
		throw;
	}
#else
	return true;
#endif
}

bool DbUserRepository::db_delete_user(const std::string& login) {
#ifdef USE_PQXX

	std::lock_guard<std::mutex> lock(g_db_mutex);

	if (!db_ensure_connection_unlocked()) {
		throw std::runtime_error("DB unavailable");
	}

	try {
		pqxx::work tx(*g_db);
		auto res = tx.exec(
			"DELETE FROM users WHERE login = $1",
			pqxx::params{ login }
		);
		tx.commit();
		return res.affected_rows() > 0;
	}
	catch (const std::exception& e) {
		std::cerr << "db_delete_user error: " << e.what() << std::endl;
		throw;
	}
#else
	return true;
#endif
}


void DbUserRepository::db_insert_dot(const std::string& login, const DotView& d) {
#ifdef USE_PQXX

	std::lock_guard<std::mutex> lock(g_db_mutex);

	if (!db_ensure_connection_unlocked()) {
		throw std::runtime_error("DB unavailable");
	}

	try {
		pqxx::work tx(*g_db);
		tx.exec(
			"INSERT INTO dots(x, y, r, hit, exec_time, cur_time, user_id) "
			"VALUES($1, $2, $3, $4, $5, $6, "
			"  (SELECT id FROM users WHERE login = $7)"
			")",
			pqxx::params{
				d.x,
				d.y,
				d.r,
				d.hit,
				d.exec_time_ms,
				d.timestamp, 
				login
			}
		);
		tx.commit();
	}
	catch (const std::exception& e) {
		std::cerr << "db_insert_dot error: " << e.what() << std::endl;
		throw;
	}
#else
	return;
#endif
}

std::vector<DotView> DbUserRepository::db_get_dots(const std::string& login) {
#ifdef USE_PQXX 
	std::lock_guard<std::mutex> lock(g_db_mutex);

	if (!db_ensure_connection_unlocked()) {
		throw std::runtime_error("DB unavailable");
	}

	std::vector<DotView> out;

	try {
		pqxx::work tx(*g_db);
		auto res = tx.exec(
			"SELECT d.x, d.y, d.r, d.hit, d.exec_time, d.cur_time "
			"FROM dots d "
			"JOIN users u ON d.user_id = u.id "
			"WHERE u.login = $1 "
			"ORDER BY d.id",
			pqxx::params{ login }
		);
		tx.commit();

		for (const auto& row : res) {
			DotView d;
			d.x = row[0].as<std::string>();
			d.y = row[1].as<std::string>();
			d.r = row[2].as<std::string>();
			d.hit = row[3].as<bool>();
			d.exec_time_ms = row[4].as<long long>();
			d.timestamp = row[5].as<std::string>(); 
			out.push_back(std::move(d));
		}
	}
	catch (const std::exception& e) {
		std::cerr << "db_get_dots error: " << e.what() << std::endl;
		throw;
	}

	return out;
#else
	return {};
#endif
}

void DbUserRepository::db_clear_dots(const std::string& login) {
#ifdef USE_PQXX

	std::lock_guard<std::mutex> lock(g_db_mutex);

	if (!db_ensure_connection_unlocked()) {
		throw std::runtime_error("DB unavailable");
	}

	try {
		pqxx::work tx(*g_db);
		tx.exec(
			"DELETE FROM dots d "
			"USING users u "
			"WHERE d.user_id = u.id AND u.login = $1",
			pqxx::params{ login }
		);
		tx.commit();
	}
	catch (const std::exception& e) {
		std::cerr << "db_clear_dots error: " << e.what() << std::endl;
		throw;
	}
#else
	return;
#endif
}
