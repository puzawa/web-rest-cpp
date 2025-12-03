#pragma once

#include "../tcp_server/tcp_server.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

namespace tests {
	int RunHttpServerTests(bool verbose);
};

namespace http {

	enum class HttpMethod {
		Get,
		Post,
		Put,
		Delete_,
		Patch,
		Options,
		Head,
		Unknown
	};

	HttpMethod parse_method(const std::string& s);


	struct QueryParams {
		using Map = std::unordered_map<std::string, std::vector<std::string>>;

		Map params;

		bool has(const std::string& key) const {
			return params.find(key) != params.end();
		}

		std::optional<std::string> get(const std::string& key) const {
			auto it = params.find(key);
			if (it == params.end() || it->second.empty()) {
				return std::nullopt;
			}
			return it->second.front();
		}

		std::vector<std::string> get_all(const std::string& key) const {
			auto it = params.find(key);
			if (it == params.end()) return {};
			return it->second;
		}

		std::optional<int> get_int(const std::string& key) const {
			auto v = get(key);
			if (!v) return std::nullopt;
			try {
				return std::stoi(*v);
			}
			catch (...) {
				return std::nullopt;
			}
		}

		std::optional<double> get_double(const std::string& key) const {
			auto v = get(key);
			if (!v) return std::nullopt;
			try {
				return std::stod(*v);
			}
			catch (...) {
				return std::nullopt;
			}
		}

		std::optional<bool> get_bool(const std::string& key) const {
			auto v = get(key);
			if (!v) return std::nullopt;
			std::string s = *v;
			for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			if (s == "1" || s == "true" || s == "yes" || s == "on")  return true;
			if (s == "0" || s == "false" || s == "no" || s == "off") return false;
			return std::nullopt;
		}
	};

	struct HttpRequest {
		HttpMethod method = HttpMethod::Unknown;
		std::string method_str;
		std::string path;
		std::string query;
		QueryParams query_params;
		std::string http_version;
		std::unordered_map<std::string, std::string> headers;
		std::string body;

		std::unordered_map<std::string, std::string> path_params;

		const std::string& header(const std::string& name) const;


		bool has_query(const std::string& key) const {
			return query_params.has(key);
		}

		std::optional<std::string> query_param(const std::string& key) const {
			return query_params.get(key);
		}

		std::string query_param_or(const std::string& key,
			const std::string& default_value) const {
			auto v = query_params.get(key);
			return v ? *v : default_value;
		}

		std::optional<int> query_param_int(const std::string& key) const {
			return query_params.get_int(key);
		}

		std::optional<double> query_param_double(const std::string& key) const {
			return query_params.get_double(key);
		}

		std::optional<bool> query_param_bool(const std::string& key) const {
			return query_params.get_bool(key);
		}


		bool has_path_param(const std::string& key) const {
			return path_params.find(key) != path_params.end();
		}

		std::optional<std::string> path_param(const std::string& key) const {
			auto it = path_params.find(key);
			if (it == path_params.end()) return std::nullopt;
			return it->second;
		}
	};

	struct HttpResponse {
		int status_code = 200;
		std::string reason = "OK";
		std::unordered_map<std::string, std::string> headers;
		std::string body;

		std::string to_string() const;

		void set_status(int code, const std::string& reason_phrase) {
			status_code = code;
			reason = reason_phrase;
		}
	};

	class Router {
	public:
		using Handler = std::function<void(HttpRequest&, HttpResponse&)>;

		void add_route(HttpMethod method, const std::string& path_pattern, Handler handler);
		void add_route(const std::string& method_str, const std::string& path_pattern, Handler handler);


		bool route(HttpRequest& req, HttpResponse& resp) const;

	private:
		struct Route {
			std::string method;
			std::string pattern;
			Handler handler;
		};

		std::vector<Route> routes_;
		static bool match_pattern(const std::string& pattern,
			const std::string& path,
			std::unordered_map<std::string, std::string>& out_params);
	};

	bool parse_http_request(const std::string& raw, HttpRequest& req, std::size_t& header_length);

	struct HttpServerConfig {
		std::string bind_address = "::";
		std::uint16_t port = 8080;

		std::size_t thread_count = std::thread::hardware_concurrency();
		std::size_t max_queue_size = 1024;

		std::size_t max_header_size = 64 * 1024;        // 64 KB
		std::size_t max_body_size = 10 * 1024 * 1024; // 10 MB

		int socket_timeout_ms = 10000; // 10 sec

		// Simple CORS
		bool enable_cors = false;
		std::string cors_allow_origin = "*";
		std::string cors_allow_methods = "GET, POST, PUT, DELETE, OPTIONS, PATCH";
		std::string cors_allow_headers = "Content-Type, Authorization";
	};

	class HttpServer {
	public:
		using Handler = Router::Handler;

		explicit HttpServer(const HttpServerConfig& cfg);

		~HttpServer() = default;

		HttpServer(const HttpServer&) = delete;
		HttpServer& operator=(const HttpServer&) = delete;

		void start();
		void stop();
		bool is_running() const;

		void add_route(HttpMethod method, const std::string& path_pattern, Handler handler) {
			router_.add_route(method, path_pattern, std::move(handler));
		}

		void add_route(const std::string& method_str, const std::string& path_pattern, Handler handler) {
			router_.add_route(method_str, path_pattern, std::move(handler));
		}

		Router& router() { return router_; }
		const Router& router() const { return router_; }

		const HttpServerConfig& config() const { return config_; }

	private:
		void handle_connection(std::shared_ptr<net::TcpConnection> conn);

		HttpServerConfig config_;
		Router router_;
		net::TcpServer tcp_server_;
	};

}
