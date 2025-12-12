#pragma once

#include "json/json.hpp"

#include <string>
#include <vector>

struct DotView {
	std::string x{};
	std::string y{};
	std::string r{};
	bool        hit{};
	long long   exec_time_ms{};
	std::string timestamp = "";

	JsonValue to_json() const {
		JsonValue::object obj;
		obj["x"] = JsonValue(x);
		obj["y"] = JsonValue(y);
		obj["r"] = JsonValue(r);
		obj["hit"] = JsonValue(hit);
		obj["execTime"] = JsonValue(static_cast<double>(exec_time_ms));
		obj["time"] = JsonValue(timestamp);
		return JsonValue(obj);
	}
};

struct User {
	std::string login;
	std::string password;
	std::vector<DotView> dots;
};

struct DbTask {
	std::string login;
	DotView dot;
};