#include "http_server.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace {

	inline void ltrim(std::string& s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
			return !std::isspace(ch);
			}));
	}

	inline void rtrim(std::string& s) {
		s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
			return !std::isspace(ch);
			}).base(), s.end());
	}

	inline void trim(std::string& s) {
		ltrim(s);
		rtrim(s);
	}

	inline std::string to_lower(std::string s) {
		std::transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return s;
	}

	inline std::string to_upper(std::string s) {
		std::transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c) { return static_cast<char>(std::toupper(c)); });
		return s;
	}

	inline std::string default_reason_phrase(int code) {
		switch (code) {
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 411: return "Length Required";
		case 413: return "Payload Too Large";
		case 431: return "Request Header Fields Too Large";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		default:  return "Unknown";
		}
	}

	std::string url_decode(const std::string& s) {
		std::string out;
		out.reserve(s.size());

		for (std::size_t i = 0; i < s.size(); ++i) {
			char c = s[i];
			if (c == '+') {
				out.push_back(' ');
			}
			else if (c == '%' && i + 2 < s.size()) {
				char h1 = s[i + 1];
				char h2 = s[i + 2];
				auto hex = [](char h) -> int {
					if (h >= '0' && h <= '9') return h - '0';
					if (h >= 'a' && h <= 'f') return h - 'a' + 10;
					if (h >= 'A' && h <= 'F') return h - 'A' + 10;
					return -1;
					};
				int hi = hex(h1);
				int lo = hex(h2);
				if (hi >= 0 && lo >= 0) {
					out.push_back(static_cast<char>((hi << 4) | lo));
					i += 2;
				}
				else {
					out.push_back(c);
				}
			}
			else {
				out.push_back(c);
			}
		}
		return out;
	}

	void parse_query_string(const std::string& raw, http::QueryParams& out) {
		out.params.clear();
		std::size_t i = 0;
		while (i < raw.size()) {
			std::size_t amp = raw.find('&', i);
			std::string pair = (amp == std::string::npos)
				? raw.substr(i)
				: raw.substr(i, amp - i);

			if (!pair.empty()) {
				std::size_t eq = pair.find('=');
				std::string key;
				std::string value;
				if (eq == std::string::npos) {
					key = url_decode(pair);
					value = "";
				}
				else {
					key = url_decode(pair.substr(0, eq));
					value = url_decode(pair.substr(eq + 1));
				}
				if (!key.empty()) {
					out.params[key].push_back(std::move(value));
				}
			}

			if (amp == std::string::npos) break;
			i = amp + 1;
		}
	}

}

namespace http {


	HttpMethod parse_method(const std::string& s) {
		if (s == "GET")     return HttpMethod::Get;
		if (s == "POST")    return HttpMethod::Post;
		if (s == "PUT")     return HttpMethod::Put;
		if (s == "DELETE")  return HttpMethod::Delete_;
		if (s == "PATCH")   return HttpMethod::Patch;
		if (s == "OPTIONS") return HttpMethod::Options;
		if (s == "HEAD")    return HttpMethod::Head;
		return HttpMethod::Unknown;
	}

	const std::string& HttpRequest::header(const std::string& name) const {
		static const std::string empty;
		auto it = headers.find(to_lower(name));
		if (it == headers.end()) return empty;
		return it->second;
	}

	std::string HttpResponse::to_string() const {
		std::ostringstream oss;

		std::string reason_phrase = reason.empty() ? default_reason_phrase(status_code) : reason;

		oss << "HTTP/1.1 " << status_code << " " << reason_phrase << "\r\n";

		bool has_content_length = false;

		for (const auto& kv : headers) {
			std::string name_lower = to_lower(kv.first);
			if (name_lower == "content-length") has_content_length = true;
			oss << kv.first << ": " << kv.second << "\r\n";
		}

		if (!has_content_length) {
			oss << "Content-Length: " << body.size() << "\r\n";
		}

		oss << "\r\n";
		oss << body;

		return oss.str();
	}

	void Router::add_route(HttpMethod method, const std::string& path_pattern, Handler handler) {
		std::string method_str;
		switch (method) {
		case HttpMethod::Get:     method_str = "GET"; break;
		case HttpMethod::Post:    method_str = "POST"; break;
		case HttpMethod::Put:     method_str = "PUT"; break;
		case HttpMethod::Delete_: method_str = "DELETE"; break;
		case HttpMethod::Patch:   method_str = "PATCH"; break;
		case HttpMethod::Options: method_str = "OPTIONS"; break;
		case HttpMethod::Head:    method_str = "HEAD"; break;
		default:                  method_str = "UNKNOWN"; break;
		}
		routes_.push_back(Route{ method_str, path_pattern, std::move(handler) });
	}

	void Router::add_route(const std::string& method_str, const std::string& path_pattern, Handler handler) {
		routes_.push_back(Route{ to_upper(method_str), path_pattern, std::move(handler) });
	}

	bool Router::match_pattern(const std::string& pattern,
		const std::string& path,
		std::unordered_map<std::string, std::string>& out_params) {
		out_params.clear();

		std::size_t i = 0;
		std::size_t j = 0;

		auto next_seg = [](const std::string& s, std::size_t& pos) -> std::string {
			if (pos >= s.size()) return {};
			if (s[pos] == '/') ++pos;
			if (pos >= s.size()) return {};
			std::size_t end = s.find('/', pos);
			if (end == std::string::npos) end = s.size();
			std::string seg = s.substr(pos, end - pos);
			pos = end;
			return seg;
			};

		while (true) {
			std::string pseg = next_seg(pattern, i);
			std::string sseg = next_seg(path, j);

			bool p_end = pseg.empty() && i >= pattern.size();
			bool s_end = sseg.empty() && j >= path.size();

			if (p_end && s_end) {
				return true;
			}

			if (pseg.empty() && !p_end) {
				return false;
			}

			if (!p_end && pseg.size() > 0 && pseg[0] == '*') {
				std::string name = pseg.substr(1);
				std::string rest;
				if (!sseg.empty()) {
					std::size_t start_pos = path.find(sseg, j - sseg.size());
					if (start_pos != std::string::npos) {
						rest = path.substr(start_pos);
					}
					else {
						rest = sseg;
					}
				}
				else {
					rest.clear();
				}
				out_params[name] = rest;
				return true;
			}

			if (s_end && !p_end) {
				return false;
			}

			if (!pseg.empty() && pseg[0] == ':') {
				std::string name = pseg.substr(1);
				out_params[name] = sseg;
			}
			else {
				if (pseg != sseg) {
					return false;
				}
			}

			if (i >= pattern.size() && j >= path.size()) {
				return true;
			}
		}
	}

	bool Router::route(HttpRequest& req, HttpResponse& resp) const {
		if (req.method_str.empty()) {
			resp.status_code = 400;
			resp.reason = "Bad Request";
			resp.body = "Bad Request";
			resp.headers["Content-Type"] = "text/plain";
			return false;
		}

		std::unordered_map<std::string, std::string> tmp_params;
		std::set<std::string> allowed_methods;
		bool handler_found = false;

		for (const auto& r : routes_) {
			if (!match_pattern(r.pattern, req.path, tmp_params)) {
				continue;
			}
			allowed_methods.insert(r.method);
			if (r.method == req.method_str) {
				req.path_params = tmp_params;
				r.handler(req, resp);
				handler_found = true;
				break;
			}
		}

		if (handler_found) {
			return true;
		}

		if (!allowed_methods.empty()) {
			resp.status_code = 405;
			resp.reason = "Method Not Allowed";
			std::string allow;
			bool first = true;
			for (const auto& m : allowed_methods) {
				if (!first) allow += ", ";
				allow += m;
				first = false;
			}
			resp.headers["Allow"] = allow;
			resp.headers["Content-Type"] = "text/plain";
			resp.body = "Method Not Allowed";
			return false;
		}

		resp.status_code = 404;
		resp.reason = "Not Found";
		resp.headers["Content-Type"] = "text/plain";
		resp.body = "Not Found";
		return false;
	}


	bool parse_http_request(const std::string& raw, HttpRequest& req, std::size_t& header_length) {
		const std::string delimiter = "\r\n\r\n";
		auto pos = raw.find(delimiter);
		if (pos == std::string::npos) {
			return false;
		}

		header_length = pos + delimiter.size();

		std::string header_block = raw.substr(0, pos);
		std::istringstream iss(header_block);
		std::string line;

		if (!std::getline(iss, line)) {
			return false;
		}
		if (!line.empty() && line.back() == '\r') line.pop_back();

		{
			std::istringstream rl(line);
			std::string method;
			std::string target;
			std::string version;

			if (!(rl >> method >> target >> version)) {
				return false;
			}

			req.method_str = to_upper(method);
			req.method = parse_method(req.method_str);
			req.http_version = version;

			auto qpos = target.find('?');
			if (qpos == std::string::npos) {
				req.path = target;
				req.query.clear();
				req.query_params.params.clear();
			}
			else {
				req.path = target.substr(0, qpos);
				req.query = target.substr(qpos + 1);
				parse_query_string(req.query, req.query_params);
			}
			req.path_params.clear();
		}

		req.headers.clear();

		while (std::getline(iss, line)) {
			if (!line.empty() && line.back() == '\r') line.pop_back();
			if (line.empty()) {
				break;
			}
			auto colon_pos = line.find(':');
			if (colon_pos == std::string::npos) {
				continue;
			}
			std::string name = line.substr(0, colon_pos);
			std::string value = line.substr(colon_pos + 1);
			trim(name);
			trim(value);
			req.headers[to_lower(name)] = value;
		}

		return true;
	}

	HttpServer::HttpServer(const HttpServerConfig& cfg)
		: config_(cfg)
		, router_()
		, tcp_server_(cfg.bind_address,
			cfg.port,
			[this](std::shared_ptr<net::TcpConnection> conn) {
				this->handle_connection(std::move(conn));
			},
			cfg.thread_count,
			cfg.max_queue_size)
	{
	}

	void HttpServer::start() {
		tcp_server_.start();
	}

	void HttpServer::stop() {
		tcp_server_.stop();
	}

	bool HttpServer::is_running() const {
		return tcp_server_.is_running();
	}

	void HttpServer::handle_connection(std::shared_ptr<net::TcpConnection> conn) {
		try {
			if (config_.socket_timeout_ms > 0) {
				conn->set_timeout_ms(config_.socket_timeout_ms);
			}

			std::string buffer;
			buffer.reserve(8192);

			char temp[4096];

			while (true) {
				HttpRequest req;
				std::size_t header_length = 0;

				while (true) {
					if (buffer.size() > config_.max_header_size) {
						HttpResponse resp;
						resp.set_status(431, "Request Header Fields Too Large");
						resp.headers["Content-Type"] = "text/plain";
						resp.body = "Request headers too large";
						auto out = resp.to_string();
						conn->send(out.data(), out.size());
						return;
					}

					if (parse_http_request(buffer, req, header_length)) {
						break;
					}

					std::size_t n = conn->recv(temp, sizeof(temp));
					if (n == 0) {
						return;
					}
					buffer.append(temp, temp + n);
				}

				{
					auto te_it = req.headers.find("transfer-encoding");
					if (te_it != req.headers.end()) {
						std::string te_lower = to_lower(te_it->second);
						if (te_lower.find("chunked") != std::string::npos) {
							HttpResponse resp;
							resp.set_status(501, "Not Implemented");
							resp.headers["Content-Type"] = "text/plain";
							resp.body = "Chunked transfer encoding not supported";
							auto out = resp.to_string();
							conn->send(out.data(), out.size());
							return;
						}
					}
				}

				std::size_t content_length = 0;
				{
					auto it = req.headers.find("content-length");
					if (it != req.headers.end()) {
						try {
							unsigned long long v = std::stoull(it->second);
							if (v > config_.max_body_size) {
								HttpResponse resp;
								resp.set_status(413, "Payload Too Large");
								resp.headers["Content-Type"] = "text/plain";
								resp.body = "Payload Too Large";
								auto out = resp.to_string();
								conn->send(out.data(), out.size());
								return;
							}
							content_length = static_cast<std::size_t>(v);
						}
						catch (...) {
							HttpResponse resp;
							resp.set_status(400, "Bad Request");
							resp.headers["Content-Type"] = "text/plain";
							resp.body = "Invalid Content-Length";
							auto out = resp.to_string();
							conn->send(out.data(), out.size());
							return;
						}
					}
				}

				std::size_t already_have = buffer.size() - header_length;
				std::string body;
				if (content_length > 0) {
					body.reserve(content_length);
					if (already_have >= content_length) {
						body = buffer.substr(header_length, content_length);
					}
					else {
						body.assign(buffer.begin() + static_cast<std::ptrdiff_t>(header_length),
							buffer.end());
						std::size_t remaining = content_length - body.size();
						while (remaining > 0) {
							std::size_t n = conn->recv(temp, std::min<std::size_t>(sizeof(temp), remaining));
							if (n == 0) {
								break;
							}
							body.append(temp, temp + n);
							remaining -= n;
						}
						if (body.size() < content_length) {
							HttpResponse resp;
							resp.set_status(400, "Bad Request");
							resp.headers["Content-Type"] = "text/plain";
							resp.body = "Incomplete request body";
							auto out = resp.to_string();
							conn->send(out.data(), out.size());
							return;
						}
					}
				}
				req.body = std::move(body);

				std::string conn_hdr = to_lower(req.header("connection"));
				std::string version_lower = to_lower(req.http_version);
				bool keep_alive = false;

				if (version_lower == "http/1.0") {
					keep_alive = (conn_hdr == "keep-alive");
				}
				else {
					keep_alive = (conn_hdr != "close");
				}

				HttpResponse resp;

				if (config_.enable_cors) {
					resp.headers["Access-Control-Allow-Origin"] = config_.cors_allow_origin;
					resp.headers["Access-Control-Allow-Methods"] = config_.cors_allow_methods;
					resp.headers["Access-Control-Allow-Headers"] = config_.cors_allow_headers;
				}

				if (req.method == HttpMethod::Options) {
					resp.status_code = 204;
					resp.reason = "No Content";
					resp.body.clear();
				}
				else {
					try {
						router_.route(req, resp);
					}
					catch (...) {
						resp.status_code = 500;
						resp.reason = "Internal Server Error";
						resp.headers["Content-Type"] = "text/plain";
						resp.body = "Internal Server Error";
					}
				}

				resp.headers["Connection"] = keep_alive ? "keep-alive" : "close";

				auto out = resp.to_string();
				conn->send(out.data(), out.size());

				std::size_t consumed = header_length + content_length;
				if (buffer.size() > consumed) {
					buffer.erase(0, consumed);
				}
				else {
					buffer.clear();
				}

				if (!keep_alive) {
					return;
				}
			}
		}
		catch (...) {
			return;
		}
	}

}
