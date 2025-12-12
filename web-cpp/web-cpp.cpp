#include "web/http_server/http_server.hpp"


#include "bigdec/bigdec.hpp"
#include "json/json.hpp"

#include "lab/math.hpp"
#include "lab/user_service.hpp"
#include "utils/utils.hpp"

#include <chrono>

#include <iostream>
#include <memory>
#include <thread>

using namespace http;

DbUserRepository* db = nullptr;
LocalUserRepository g_local_users;
UserService* g_user_service = nullptr;


std::string extract_token(const std::string& auth_header) {
	const std::string prefix = "Bearer ";
	if (auth_header.size() >= prefix.size() &&
		auth_header.compare(0, prefix.size(), prefix) == 0) {

		std::string token = auth_header.substr(prefix.size());
		std::size_t b = token.find_first_not_of(" \t\r\n");
		std::size_t e = token.find_last_not_of(" \t\r\n");
		if (b == std::string::npos) return {};
		return token.substr(b, e - b + 1);
	}
	return {};
}

std::string get_login_from_auth(const HttpRequest& req) {
	const std::string& auth = req.header("Authorization");
	std::string token = extract_token(auth);
	if (token.empty() || !g_user_service) return {};
	return g_user_service->login_from_token(token);
}

void handle_login(HttpRequest& req, HttpResponse& resp) {
	JsonValue root;
	std::unique_ptr<JsonObjectView> obj;

	if (!utils::parse_and_require_fields(req, resp, { "login", "password" }, root, obj)) {
		return;
	}

	std::string login;
	std::string password;
	try {
		login = obj->get<std::string>("login");
		password = obj->get<std::string>("password");
	}
	catch (...) {
		respond::BAD_REQUEST(resp);
		return;
	}

	auto result = g_user_service->login(login, password);
	if (!result.ok()) {
		switch (result.error) {
		case UserError::InvalidCredentials:
			respond::UNAUTHORIZED(resp);
			break;
		case UserError::DbError:
		default:
			respond::SERVICE_UNAVAILABLE(resp);
			break;
		}
		return;
	}

	const auto& auth_res = *result.value;

	JsonValue::object resp_obj;
	resp_obj["token"] = JsonValue(auth_res.token);

	JsonValue::array dots_arr;
	for (const auto& d : auth_res.dots) {
		dots_arr.push_back(d.to_json());
	}
	resp_obj["dots"] = JsonValue(dots_arr);

	respond::OK(resp, JsonValue(resp_obj));
}

void handle_register(HttpRequest& req, HttpResponse& resp) {
	JsonValue root;
	std::unique_ptr<JsonObjectView> obj;

	if (!utils::parse_and_require_fields(req, resp, { "login", "password" }, root, obj)) {
		return;
	}

	std::string login;
	std::string password;
	try {
		login = obj->get<std::string>("login");
		password = obj->get<std::string>("password");
	}
	catch (...) {
		respond::BAD_REQUEST(resp);
		return;
	}

	auto result = g_user_service->register_user(login, password);
	if (!result.ok()) {
		switch (result.error) {
		case UserError::UserAlreadyExists:
			respond::CONFLICT(resp);
			break;
		case UserError::DbError:
		default:
			respond::SERVICE_UNAVAILABLE(resp);
			break;
		}
		return;
	}

	const auto& auth_res = *result.value;

	JsonValue::object resp_obj;
	resp_obj["token"] = JsonValue(auth_res.token);
	resp_obj["dots"] = JsonValue(JsonValue::array{});

	respond::OK(resp, JsonValue(resp_obj));
}

void handle_logout(HttpRequest& req, HttpResponse& resp) {
	const std::string& auth = req.header("Authorization");
	std::string token = extract_token(auth);

	if (!token.empty()) {
		g_user_service->logout(token);
	}

	respond::OK(resp);
}

void handle_remove(HttpRequest& req, HttpResponse& resp) {
	const std::string& auth = req.header("Authorization");
	std::string token = extract_token(auth);
	if (token.empty()) {
		respond::UNAUTHORIZED(resp);
		return;
	}

	std::string login = g_user_service->login_from_token(token);
	if (login.empty()) {
		respond::UNAUTHORIZED(resp);
		return;
	}

	auto res = g_user_service->remove_user_by_login(login);
	if (!res.ok()) {
		switch (res.error) {
		case UserError::UserNotFound:
			respond::NOT_FOUND(resp);
			break;
		case UserError::DbError:
		default:
			respond::SERVICE_UNAVAILABLE(resp);
			break;
		}
		return;
	}

	respond::NO_CONTENT(resp);
}

void handle_time(HttpRequest&, HttpResponse& resp) {
	long long ms = utils::current_time_millis();
	JsonValue v(static_cast<long long>(ms));
	respond::OK(resp, v);
}

void handle_add_dot(HttpRequest& req, HttpResponse& resp) {
	std::string login = get_login_from_auth(req);
	if (login.empty()) {
		respond::UNAUTHORIZED(resp);
		return;
	}

	JsonValue root;
	std::unique_ptr<JsonObjectView> obj;

	if (!utils::parse_and_require_fields(req, resp, { "x", "y", "r" }, root, obj)) {
		return;
	}

	std::string x;
	std::string y;
	std::string r;
	try {
		x = obj->get<std::string>("x");
		y = obj->get<std::string>("y");
		r = obj->get<std::string>("r");
	}
	catch (std::exception&) {
		respond::BAD_REQUEST(resp);
		return;
	}

	long long start = utils::current_time_millis();
	bool hit = HitChecker().hit_check(x, y, r);
	long long exec_time = utils::current_time_millis() - start;
	DotView dot{ x, y, r, hit, exec_time, utils::current_iso_local_datetime() };

	auto res = g_user_service->add_dot(login, dot);
	if (!res.ok()) {
		respond::SERVICE_UNAVAILABLE(resp);
		return;
	}

	respond::OK(resp, dot.to_json());
}

void handle_clear_dots(HttpRequest& req, HttpResponse& resp) {
	std::string login = get_login_from_auth(req);
	if (login.empty()) {
		respond::UNAUTHORIZED(resp);
		return;
	}

	auto res = g_user_service->clear_dots(login);
	if (!res.ok()) {
		respond::SERVICE_UNAVAILABLE(resp);
		return;
	}

	respond::OK(resp);
}

void handle_get_dots(HttpRequest& req, HttpResponse& resp) {
	std::string login = get_login_from_auth(req);
	if (login.empty()) {
		respond::UNAUTHORIZED(resp);
		return;
	}

	auto res = g_user_service->get_dots(login);
	if (!res.ok()) {
		respond::SERVICE_UNAVAILABLE(resp);
		return;
	}

	JsonValue::array arr;
	for (const auto& d : *res.value) {
		arr.push_back(d.to_json());
	}

	respond::OK(resp, JsonValue(arr));
}

void setup_routes(Router& r) {
	r.add_route(HttpMethod::Post, "/api/auth/login", handle_login);
	r.add_route(HttpMethod::Post, "/api/auth/register", handle_register);
	r.add_route(HttpMethod::Post, "/api/auth/logout", handle_logout);
	r.add_route(HttpMethod::Post, "/api/auth/remove", handle_remove);

	r.add_route(HttpMethod::Get, "/api/main/time", handle_time);
	r.add_route(HttpMethod::Post, "/api/main/add", handle_add_dot);
	r.add_route(HttpMethod::Post, "/api/main/clear", handle_clear_dots);
	r.add_route(HttpMethod::Get, "/api/main/dots", handle_get_dots);
}

int main() {
	tests::RunBigDecimalTests(false);
	tests::RunJsonTests(true);
	tests::RunHttpServerTests(true);

	db = new DbUserRepository(
		"host=localhost "
		"port=44401 "
		"dbname=studs "
		"user=s413039 "
		"password=cUjGdh3up1srj9Po"
	);

	static UserService user_service(*db, g_local_users);
	g_user_service = &user_service;

	HttpServerConfig cfg;
	cfg.port = 8080;
	cfg.enable_cors = true;

	HttpServer server(cfg);
	setup_routes(server.router());

	std::cout << "Starting HTTP server on port " << cfg.port << "...\n";
	server.start();

	while (true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	return 0;
}
