#pragma once
#include "http_server.hpp"
#include "json/json.hpp"

#include <optional>

namespace respond
{
	using namespace http;

	inline void send(HttpResponse& resp,
		int status,
		const std::string& reason,
		std::optional<JsonValue> body = std::nullopt)
	{
		resp.set_status(status, reason);
		resp.headers["Content-Type"] = "application/json; charset=utf-8";
		resp.body = body ? body->to_string() : "";
	}

	inline void OK(HttpResponse& resp, std::optional<JsonValue> body = std::nullopt) {
		send(resp, 200, "OK", body);
	}

	inline void CREATED(HttpResponse& resp, std::optional<JsonValue> body = std::nullopt) {
		send(resp, 201, "Created", body);
	}

	inline void NO_CONTENT(HttpResponse& resp) {
		send(resp, 204, "No Content");
	}

	inline void BAD_REQUEST(HttpResponse& resp) {
		send(resp, 400, "Bad Request");
	}

	inline void UNAUTHORIZED(HttpResponse& resp) {
		send(resp, 401, "Unauthorized");
	}

	inline void FORBIDDEN(HttpResponse& resp) {
		send(resp, 403, "Forbidden");
	}

	inline void NOT_FOUND(HttpResponse& resp) {
		send(resp, 404, "Not Found");
	}

	inline void CONFLICT(HttpResponse& resp) {
		send(resp, 409, "Conflict");
	}

	inline void SERVICE_UNAVAILABLE(HttpResponse& resp) {
		send(resp, 503, "Service Unavailable");
	}

};