#include "http_server.hpp"
#include <cassert>
#include <iostream>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <cmath>

namespace tests {

	static bool is_unreserved(char c) {
		if ((c >= 'A' && c <= 'Z') ||
			(c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') ||
			c == '-' || c == '_' || c == '.' || c == '~') {
			return true;
		}
		return false;
	}

	static std::string url_encode(const std::string& s) {
		std::string out;
		out.reserve(s.size());
		const char* hex = "0123456789ABCDEF";

		for (unsigned char c : s) {
			if (is_unreserved(static_cast<char>(c))) {
				out.push_back(static_cast<char>(c));
			}
			else if (c == ' ') {
				out.push_back('+');
			}
			else {
				out.push_back('%');
				out.push_back(hex[(c >> 4) & 0x0F]);
				out.push_back(hex[c & 0x0F]);
			}
		}
		return out;
	}


	bool test_parse_simple_get() {
		std::string raw =
			"GET /hello/world?name=John&age=25 HTTP/1.1\r\n"
			"Host: example.com\r\n"
			"User-Agent: TestClient\r\n"
			"\r\n";

		http::HttpRequest req;
		std::size_t header_len = 0;
		bool ok = http::parse_http_request(raw, req, header_len);
		assert(ok);
		assert(header_len == raw.size());

		assert(req.method == http::HttpMethod::Get);
		assert(req.method_str == "GET");
		assert(req.path == "/hello/world");
		assert(req.query == "name=John&age=25");
		assert(req.http_version == "HTTP/1.1");
		assert(req.header("host") == "example.com");
		assert(req.header("user-agent") == "TestClient");

		assert(req.has_query("name"));
		assert(req.query_param("name").value() == "John");
		assert(req.query_param_int("age").value() == 25);
		assert(!req.query_param("missing").has_value());

		return true;
	}

	bool test_parse_get_without_query_and_headers_spaces() {
		std::string raw =
			"GET /just/path HTTP/1.1\r\n"
			"Host:    localhost   \r\n"
			"Content-Type: text/plain; charset=utf-8   \r\n"
			"\r\n";

		http::HttpRequest req;
		std::size_t header_len = 0;
		bool ok = http::parse_http_request(raw, req, header_len);
		assert(ok);
		assert(req.method == http::HttpMethod::Get);
		assert(req.path == "/just/path");
		assert(req.query.empty());

		assert(req.header("host") == "localhost");
		assert(req.header("content-type") == "text/plain; charset=utf-8");

		return true;
	}

	bool test_parse_post_with_body() {
		std::string body = "field1=value1&field2=value2";
		std::string raw =
			"POST /submit HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Type: application/x-www-form-urlencoded\r\n"
			"Content-Length: " + std::to_string(body.size()) + "\r\n"
			"\r\n" +
			body;

		http::HttpRequest req;
		std::size_t header_len = 0;
		bool ok = http::parse_http_request(raw, req, header_len);
		assert(ok);
		assert(header_len < raw.size());

		std::string body_from_raw = raw.substr(header_len);
		assert(body_from_raw == body);

		assert(req.method == http::HttpMethod::Post);
		assert(req.path == "/submit");
		assert(req.header("content-type") == "application/x-www-form-urlencoded");
		assert(req.header("content-length") == std::to_string(body.size()));

		return true;
	}

	bool test_query_bool_double_and_multi() {
		std::string raw =
			"GET /flags?"
			"debug=1&"
			"verbose=false&"
			"pi=3.14159&"
			"tag=hello&"
			"tag=world+wide&"
			"encoded=hello%20world%21"
			" HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"\r\n";

		http::HttpRequest req;
		std::size_t header_len = 0;
		bool ok = http::parse_http_request(raw, req, header_len);
		assert(ok);

		auto dbg = req.query_param_bool("debug");
		assert(dbg.has_value());
		assert(dbg.value() == true);

		auto verb = req.query_param_bool("verbose");
		assert(verb.has_value());
		assert(verb.value() == false);

		// pi ~ 3.14159
		auto pi = req.query_param_double("pi");
		assert(pi.has_value());
		assert(std::fabs(pi.value() - 3.14159) < 1e-9);

		// tag=hello&tag=world+wide
		auto tags = req.query_params.get_all("tag");
		assert(tags.size() == 2);
		assert(tags[0] == "hello");
		assert(tags[1] == "world wide");

		// encoded=hello%20world%21
		auto enc = req.query_param("encoded");
		assert(enc.has_value());
		assert(enc.value() == "hello world!");

		// default
		assert(req.query_param_or("missing", "default") == std::string("default"));

		return true;
	}

	bool test_router_basic_and_path_params() {
		http::Router router;

		bool get_user_called = false;
		bool static_called = false;
		bool complex_called = false;

		router.add_route(http::HttpMethod::Get, "/api/users/:id",
			[&](http::HttpRequest& req, http::HttpResponse& resp) {
				get_user_called = true;
				auto id = req.path_param("id");
				assert(id.has_value());
				assert(*id == "123");

				resp.status_code = 200;
				resp.headers["Content-Type"] = "text/plain";
				resp.body = "user " + *id;
			}
		);

		router.add_route(http::HttpMethod::Get, "/static/*path",
			[&](http::HttpRequest& req, http::HttpResponse& resp) {
				static_called = true;
				auto p = req.path_param("path");
				assert(p.has_value());
				assert(*p == "css/site.css");

				resp.status_code = 200;
				resp.headers["Content-Type"] = "text/plain";
				resp.body = "static " + *p;
			}
		);

		router.add_route(http::HttpMethod::Get, "/api/users/:userId/orders/:orderId",
			[&](http::HttpRequest& req, http::HttpResponse& resp) {
				complex_called = true;
				auto userId = req.path_param("userId");
				auto orderId = req.path_param("orderId");
				assert(userId.has_value());
				assert(orderId.has_value());
				assert(*userId == "42");
				assert(*orderId == "777");

				resp.status_code = 200;
				resp.headers["Content-Type"] = "text/plain";
				resp.body = "user " + *userId + " order " + *orderId;
			}
		);

		// /api/users/123
		{
			http::HttpRequest req;
			req.method = http::HttpMethod::Get;
			req.method_str = "GET";
			req.path = "/api/users/123";

			http::HttpResponse resp;
			bool routed = router.route(req, resp);
			assert(routed);
			assert(get_user_called);
			assert(resp.status_code == 200);
			assert(resp.body == "user 123");
		}

		// /static/css/site.css
		{
			http::HttpRequest req;
			req.method = http::HttpMethod::Get;
			req.method_str = "GET";
			req.path = "/static/css/site.css";

			http::HttpResponse resp;
			bool routed = router.route(req, resp);
			assert(routed);
			assert(static_called);
			assert(resp.status_code == 200);
			assert(resp.body == "static css/site.css");
		}

		// /api/users/42/orders/777
		{
			http::HttpRequest req;
			req.method = http::HttpMethod::Get;
			req.method_str = "GET";
			req.path = "/api/users/42/orders/777";

			http::HttpResponse resp;
			bool routed = router.route(req, resp);
			assert(routed);
			assert(complex_called);
			assert(resp.status_code == 200);
			assert(resp.body == "user 42 order 777");
		}

		// 405: другой метод на тот же паттерн
		{
			http::HttpRequest req;
			req.method = http::HttpMethod::Post;
			req.method_str = "POST";
			req.path = "/api/users/999";

			http::HttpResponse resp;
			bool routed = router.route(req, resp);
			assert(!routed);
			assert(resp.status_code == 405);
			assert(!resp.headers["Allow"].empty());
		}

		// 404: путь вообще не существует
		{
			http::HttpRequest req;
			req.method = http::HttpMethod::Get;
			req.method_str = "GET";
			req.path = "/does/not/exist";

			http::HttpResponse resp;
			bool routed = router.route(req, resp);
			assert(!routed);
			assert(resp.status_code == 404);
			assert(resp.body == "Not Found");
		}

		return true;
	}

	bool test_response_to_string() {
		http::HttpResponse resp;
		resp.status_code = 200;
		resp.reason = "OK";
		resp.headers["Content-Type"] = "application/json";
		resp.body = R"({"ok":true})";

		std::string s = resp.to_string();
		assert(s.find("HTTP/1.1 200 OK\r\n") == 0);
		assert(s.find("Content-Type: application/json\r\n") != std::string::npos);
		assert(s.find("\r\n\r\n") != std::string::npos);
		assert(s.rfind(R"({"ok":true})") != std::string::npos);

		return true;
	}


	bool test_random_query_parsing() {
		std::mt19937 rng(123456);

		std::uniform_int_distribution<int> keys_count_dist(1, 4);
		std::uniform_int_distribution<int> vals_count_dist(1, 3);
		std::uniform_int_distribution<int> key_len_dist(1, 5);
		std::uniform_int_distribution<int> val_len_dist(0, 8);
		std::uniform_int_distribution<int> ch_dist(0, 61); // a-zA-Z0-9

		auto rand_ident = [&](int len) {
			std::string s;
			s.reserve(static_cast<std::size_t>(len));
			for (int i = 0; i < len; ++i) {
				int x = ch_dist(rng);
				char c;
				if (x < 26) c = static_cast<char>('a' + x);
				else if (x < 52) c = static_cast<char>('A' + (x - 26));
				else c = static_cast<char>('0' + (x - 52));
				s.push_back(c);
			}
			return s;
			};

		std::uniform_int_distribution<int> val_type_dist(0, 2); // 0: alnum, 1: space, 2: special

		auto rand_value = [&](int len) {
			std::string s;
			s.reserve(static_cast<std::size_t>(len));
			for (int i = 0; i < len; ++i) {
				int t = val_type_dist(rng);
				char c;
				if (t == 0) {
					int x = ch_dist(rng);
					if (x < 26) c = static_cast<char>('a' + x);
					else if (x < 52) c = static_cast<char>('A' + (x - 26));
					else c = static_cast<char>('0' + (x - 52));
				}
				else if (t == 1) {
					c = ' ';
				}
				else {
					const char special[] = "-_.~!@#$%^&*()";
					std::uniform_int_distribution<int> sp(0, (int)sizeof(special) - 2);
					c = special[sp(rng)];
				}
				s.push_back(c);
			}
			return s;
			};

		for (int iter = 0; iter < 500; ++iter) {
			std::unordered_map<std::string, std::vector<std::string>> original;

			int keys_count = keys_count_dist(rng);
			for (int k = 0; k < keys_count; ++k) {
				std::string key = rand_ident(key_len_dist(rng));
				int vals_count = vals_count_dist(rng);
				auto& vec = original[key];
				for (int v = 0; v < vals_count; ++v) {
					std::string val = rand_value(val_len_dist(rng));
					vec.push_back(val);
				}
			}

			std::string query;
			bool first = true;
			for (const auto& kv : original) {
				const std::string& key = kv.first;
				for (const auto& val : kv.second) {
					if (!first) query.push_back('&');
					first = false;
					query += url_encode(key);
					query.push_back('=');
					query += url_encode(val);
				}
			}

			std::string raw =
				"GET /test?" + query + " HTTP/1.1\r\n"
				"Host: localhost\r\n"
				"\r\n";

			http::HttpRequest req;
			std::size_t header_len = 0;
			bool ok = http::parse_http_request(raw, req, header_len);
			assert(ok);

			for (const auto& kv : original) {
				const std::string& key = kv.first;
				auto it = req.query_params.params.find(key);
				assert(it != req.query_params.params.end());

				const auto& got_vec = it->second;
				const auto& orig_vec = kv.second;
				assert(got_vec.size() == orig_vec.size());
				for (std::size_t i = 0; i < orig_vec.size(); ++i) {
					assert(got_vec[i] == orig_vec[i]);
				}
			}
		}

		return true;
	}

	bool test_random_path_params() {
		std::mt19937 rng(987654);

		std::uniform_int_distribution<int> seg_count_dist(1, 4);
		std::uniform_int_distribution<int> seg_type_dist(0, 1); // 0: fixed, 1: param

		std::uniform_int_distribution<int> name_len_dist(1, 4);
		std::uniform_int_distribution<int> val_len_dist(1, 6);
		std::uniform_int_distribution<int> ch_dist(0, 25); // a-z

		auto rand_name = [&](int len) {
			std::string s;
			s.reserve(static_cast<std::size_t>(len));
			for (int i = 0; i < len; ++i) {
				char c = static_cast<char>('a' + ch_dist(rng));
				s.push_back(c);
			}
			return s;
			};

		const int N = 200;
		for (int iter = 0; iter < N; ++iter) {
			http::Router router;

			int seg_count = seg_count_dist(rng);
			std::vector<std::string> pattern_segs;
			std::unordered_map<std::string, std::string> expected_params;

			for (int s = 0; s < seg_count; ++s) {
				int t = seg_type_dist(rng);
				if (t == 0) {
					pattern_segs.push_back("seg" + std::to_string(s));
				}
				else {
					// параметр
					std::string pname = rand_name(name_len_dist(rng));
					pattern_segs.push_back(":" + pname);
					expected_params[pname] = rand_name(val_len_dist(rng));
				}
			}

			std::string pattern = "/";
			for (std::size_t i = 0; i < pattern_segs.size(); ++i) {
				if (i > 0) pattern.push_back('/');
				pattern += pattern_segs[i];
			}

			std::string path = "/";
			for (std::size_t i = 0; i < pattern_segs.size(); ++i) {
				if (i > 0) path.push_back('/');
				const auto& seg = pattern_segs[i];
				if (!seg.empty() && seg[0] == ':') {
					std::string pname = seg.substr(1);
					path += expected_params[pname];
				}
				else {
					path += seg;
				}
			}

			bool called = false;

			router.add_route(http::HttpMethod::Get, pattern,
				[expected_params, path, &called](http::HttpRequest& req, http::HttpResponse& resp) {
					called = true;

					assert(req.path == path);

					assert(req.path_params.size() == expected_params.size());
					for (const auto& kv : expected_params) {
						auto it = req.path_params.find(kv.first);
						assert(it != req.path_params.end());
						assert(it->second == kv.second);
					}

					resp.status_code = 200;
					resp.body = "ok";
				}
			);

			http::HttpRequest req;
			req.method = http::HttpMethod::Get;
			req.method_str = "GET";
			req.path = path;

			http::HttpResponse resp;
			bool routed = router.route(req, resp);
			assert(routed);
			assert(called);
			assert(resp.status_code == 200);
			assert(resp.body == "ok");
		}

		return true;
	}
	bool test_parse_incomplete_request() {
		std::string raw =
			"GET /incomplete HTTP/1.1\r\n"
			"Host: localhost\r\n";

		http::HttpRequest req;
		std::size_t header_len = 0;
		bool ok = http::parse_http_request(raw, req, header_len);

		assert(!ok);
		assert(header_len == 0);
		return true;
	}

	bool test_parse_malformed_request_line() {
		std::string raw =
			"GET /no_version_here\r\n"
			"Host: localhost\r\n"
			"\r\n";

		http::HttpRequest req;
		std::size_t header_len = 0;
		bool ok = http::parse_http_request(raw, req, header_len);

		assert(!ok);

		return true;
	}


	bool test_query_invalid_bool_and_double() {
		std::string raw =
			"GET /test?"
			"flag=maybe&"
			"enabled=TRUE&"
			"pi=abc123"
			" HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"\r\n";

		http::HttpRequest req;
		std::size_t header_len = 0;
		bool ok = http::parse_http_request(raw, req, header_len);
		assert(ok);

		auto flag = req.query_param_bool("flag");
		assert(!flag.has_value());

		auto enabled = req.query_param_bool("enabled");
		assert(enabled.has_value());
		assert(enabled.value() == true);

		auto pi = req.query_param_double("pi");
		assert(!pi.has_value());

		return true;
	}

	bool test_router_unknown_method() {
		http::Router router;

		router.add_route(http::HttpMethod::Get, "/ok",
			[](http::HttpRequest& req, http::HttpResponse& resp) {
				(void)req;
				resp.status_code = 200;
				resp.body = "ok";
			}
		);

		http::HttpRequest req;
		req.method = http::HttpMethod::Unknown;
		req.method_str.clear();
		req.path = "/ok";

		http::HttpResponse resp;
		bool routed = router.route(req, resp);

		assert(!routed);
		assert(resp.status_code == 400);
		assert(resp.body == "Bad Request");

		return true;
	}

	bool test_header_line_without_colon_is_ignored() {
		std::string raw =
			"GET /test HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"ThisIsNotAHeaderLine\r\n"
			"X-Custom: value\r\n"
			"\r\n";

		http::HttpRequest req;
		std::size_t header_len = 0;
		bool ok = http::parse_http_request(raw, req, header_len);
		assert(ok);

		assert(req.header("host") == "localhost");
		assert(req.header("x-custom") == "value");

		auto it = req.headers.find("ThisIsNotAHeaderLine");
		assert(it == req.headers.end());

		return true;
	}


	static std::string url_decode_ref(const std::string& s) {
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

	bool test_random_broken_query_decoding() {
		std::mt19937 rng(424242);

		std::uniform_int_distribution<int> keys_count_dist(1, 4);
		std::uniform_int_distribution<int> vals_count_dist(1, 3);
		std::uniform_int_distribution<int> len_dist(0, 12);

		const std::string alphabet =
			"abcdefghijklmnopqrstuvwxyz"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"0123456789"
			" %+"
			"-_.~!@#$^*:,/";

		std::uniform_int_distribution<int> ch_dist(0, static_cast<int>(alphabet.size()) - 1);

		auto rand_raw_value = [&](int len) {
			std::string s;
			s.reserve(static_cast<std::size_t>(len));
			for (int i = 0; i < len; ++i) {
				s.push_back(alphabet[ch_dist(rng)]);
			}
			return s;
			};

		for (int iter = 0; iter < 300; ++iter) {
			int keys_count = keys_count_dist(rng);

			std::string query;
			bool first = true;
			for (int k = 0; k < keys_count; ++k) {
				std::string key = "k" + std::to_string(k);
				int vals_count = vals_count_dist(rng);
				for (int v = 0; v < vals_count; ++v) {
					if (!first) query.push_back('&');
					first = false;
					std::string raw_val = rand_raw_value(len_dist(rng));
					query += key;
					query.push_back('=');
					query += raw_val;
				}
			}

			std::string raw_http =
				"GET /test?" + query + " HTTP/1.1\r\n"
				"Host: localhost\r\n"
				"\r\n";

			http::HttpRequest req;
			std::size_t header_len = 0;
			bool ok = http::parse_http_request(raw_http, req, header_len);

			assert(ok);

			assert(req.path == "/test");
		}

		return true;
	}




	bool test_random_path_params_mismatch() {
		std::mt19937 rng(13579);

		std::uniform_int_distribution<int> seg_count_dist(1, 4);
		std::uniform_int_distribution<int> seg_type_dist(0, 1); // 0: fixed, 1: param
		std::uniform_int_distribution<int> name_len_dist(1, 4);
		std::uniform_int_distribution<int> ch_dist(0, 25); // a-z

		auto rand_name = [&](int len) {
			std::string s;
			s.reserve(static_cast<std::size_t>(len));
			for (int i = 0; i < len; ++i) {
				char c = static_cast<char>('a' + ch_dist(rng));
				s.push_back(c);
			}
			return s;
			};

		const int N = 200;
		for (int iter = 0; iter < N; ++iter) {
			http::Router router;

			int seg_count = seg_count_dist(rng);
			std::vector<std::string> pattern_segs;

			for (int s = 0; s < seg_count; ++s) {
				int t = seg_type_dist(rng);
				if (t == 0) {
					pattern_segs.push_back("seg" + std::to_string(s));
				}
				else {
					std::string pname = rand_name(name_len_dist(rng));
					pattern_segs.push_back(":" + pname);
				}
			}

			std::string pattern = "/";
			for (std::size_t i = 0; i < pattern_segs.size(); ++i) {
				if (i > 0) pattern.push_back('/');
				pattern += pattern_segs[i];
			}

			std::string path = "/";
			for (std::size_t i = 0; i < pattern_segs.size(); ++i) {
				if (i > 0) path.push_back('/');
				const auto& seg = pattern_segs[i];
				if (!seg.empty() && seg[0] == ':') {
					path += rand_name(3);
				}
				else {
					path += seg;
				}
			}
			path += "/extra";

			bool called = false;

			router.add_route(http::HttpMethod::Get, pattern,
				[&called](http::HttpRequest& req, http::HttpResponse& resp) {
					(void)req;
					called = true;
					resp.status_code = 200;
					resp.body = "should_not_happen";
				}
			);

			http::HttpRequest req;
			req.method = http::HttpMethod::Get;
			req.method_str = "GET";
			req.path = path;

			http::HttpResponse resp;
			bool routed = router.route(req, resp);

			assert(!routed);
			assert(!called);
			assert(resp.status_code == 404);
			assert(resp.body == "Not Found");
		}

		return true;
	}


	int RunHttpServerTests(bool verbose) {
		try {
			if (verbose) std::cout << "test_parse_simple_get...\n";
			test_parse_simple_get();

			if (verbose) std::cout << "test_parse_get_without_query_and_headers_spaces...\n";
			test_parse_get_without_query_and_headers_spaces();

			if (verbose) std::cout << "test_parse_post_with_body...\n";
			test_parse_post_with_body();

			if (verbose) std::cout << "test_query_bool_double_and_multi...\n";
			test_query_bool_double_and_multi();

			if (verbose) std::cout << "test_router_basic_and_path_params...\n";
			test_router_basic_and_path_params();

			if (verbose) std::cout << "test_response_to_string...\n";
			test_response_to_string();

			if (verbose) std::cout << "test_random_query_parsing...\n";
			test_random_query_parsing();

			if (verbose) std::cout << "test_random_path_params...\n";
			test_random_path_params();

			if (verbose) std::cout << "test_parse_incomplete_request...\n";
			test_parse_incomplete_request();

			if (verbose) std::cout << "test_parse_malformed_request_line...\n";
			test_parse_malformed_request_line();

			if (verbose) std::cout << "test_query_invalid_bool_and_double...\n";
			test_query_invalid_bool_and_double();

			if (verbose) std::cout << "test_router_unknown_method...\n";
			test_router_unknown_method();

			if (verbose) std::cout << "test_header_line_without_colon_is_ignored...\n";
			test_header_line_without_colon_is_ignored();

			if (verbose) std::cout << "test_random_broken_query_decoding...\n";
			test_random_broken_query_decoding();

			if (verbose) std::cout << "test_random_path_params_mismatch...\n";
			test_random_path_params_mismatch();

			std::cout << "All HTTP tests passed.\n";
		}
		catch (const std::exception& ex) {
			std::cerr << "Test threw exception: " << ex.what() << "\n";
			return 1;
		}
		catch (...) {
			std::cerr << "Test threw unknown exception.\n";
			return 1;
		}
		return 0;

	}
};
