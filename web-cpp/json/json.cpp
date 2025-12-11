#include "json.hpp"


JsonValue JsonValue::random(std::mt19937& rng, int depth) {
	std::uniform_int_distribution<int> type_dist(0, depth >= 3 ? 3 : 5);
	int t = type_dist(rng);
	switch (t) {
	case 0: return JsonValue(nullptr);
	case 1: {
		std::bernoulli_distribution b(0.5);
		return JsonValue(b(rng));
	}
	case 2: {
		std::uniform_real_distribution<double> d(-1e6, 1e6);
		return JsonValue(d(rng));
	}
	case 3: {
		std::uniform_int_distribution<int> len_dist(0, 16);
		int len = len_dist(rng);
		std::string s;
		s.reserve(len);
		std::uniform_int_distribution<int> ch_dist(32, 126);
		for (int i = 0; i < len; ++i) {
			char c = static_cast<char>(ch_dist(rng));
			s.push_back(c);
		}
		return JsonValue(std::move(s));
	}
	case 4: {
		std::uniform_int_distribution<int> len_dist(0, 6);
		int len = len_dist(rng);
		array arr;
		arr.reserve(len);
		for (int i = 0; i < len; ++i) {
			arr.push_back(JsonValue::random(rng, depth + 1));
		}
		return JsonValue(std::move(arr));
	}
	case 5:
	default: {
		std::uniform_int_distribution<int> len_dist(0, 6);
		int len = len_dist(rng);
		object obj;
		for (int i = 0; i < len; ++i) {
			std::uniform_int_distribution<int> klen_dist(1, 8);
			int klen = klen_dist(rng);
			std::string key;
			key.reserve(klen);
			std::uniform_int_distribution<int> kch_dist('a', 'z');
			for (int j = 0; j < klen; ++j)
				key.push_back(static_cast<char>(kch_dist(rng)));
			obj.emplace(std::move(key), JsonValue::random(rng, depth + 1));
		}
		return JsonValue(std::move(obj));
	}
	}
}

std::string JsonValue::to_string(const JsonValue& v) {
	return std::visit([](auto&& arg) -> std::string {
		using T = std::decay_t<decltype(arg)>;
		if constexpr (std::is_same_v<T, std::nullptr_t>) {
			return "null";
		}
		else if constexpr (std::is_same_v<T, bool>) {
			return arg ? "true" : "false";
		}
		else if constexpr (std::is_same_v<T, long long>) {  
			std::ostringstream oss;
			oss << arg;
			return oss.str();
		}
		else if constexpr (std::is_same_v<T, double>) {
			std::ostringstream oss;
			oss << std::setprecision(17) << arg;
			return oss.str();
		}
		else if constexpr (std::is_same_v<T, std::string>) {
			return JsonValue::escape_string(arg);
		}
		else if constexpr (std::is_same_v<T, array>) {
			std::string out = "[";
			bool first = true;
			for (const auto& el : arg) {
				if (!first) out += ",";
				first = false;
				out += JsonValue::to_string(el);
			}
			out += "]";
			return out;
		}
		else if constexpr (std::is_same_v<T, object>) {
			std::string out = "{";
			bool first = true;
			for (const auto& kv : arg) {
				if (!first) out += ",";
				first = false;
				out += JsonValue::escape_string(kv.first);
				out += ":";
				out += JsonValue::to_string(kv.second);
			}
			out += "}";
			return out;
		}
		}, v.value);
}

void JsonValue::to_string_pretty_impl(const JsonValue& v,
	std::string& out,
	int indent,
	int indent_step) {
	std::visit([&](auto&& arg) {
		using T = std::decay_t<decltype(arg)>;

		if constexpr (std::is_same_v<T, std::nullptr_t>) {
			out += "null";
		}
		else if constexpr (std::is_same_v<T, bool>) {
			out += (arg ? "true" : "false");
		}
		else if constexpr (std::is_same_v<T, double>) {
			std::ostringstream oss;
			oss << std::setprecision(17) << arg;
			out += oss.str();
		}
		else if constexpr (std::is_same_v<T, std::string>) {
			out += JsonValue::escape_string(arg);
		}
		else if constexpr (std::is_same_v<T, array>) {
			out += "[";
			if (!arg.empty()) {
				out += "\n";
				bool first = true;
				for (const auto& el : arg) {
					if (!first) out += ",\n";
					first = false;
					out.append(indent + indent_step, ' ');
					JsonValue::to_string_pretty_impl(el, out, indent + indent_step, indent_step);
				}
				out += "\n";
				out.append(indent, ' ');
			}
			out += "]";
		}
		else if constexpr (std::is_same_v<T, object>) {
			out += "{";
			if (!arg.empty()) {
				out += "\n";
				bool first = true;
				for (const auto& kv : arg) {
					if (!first) out += ",\n";
					first = false;
					out.append(indent + indent_step, ' ');
					out += JsonValue::escape_string(kv.first);
					out += ": ";
					JsonValue::to_string_pretty_impl(kv.second, out, indent + indent_step, indent_step);
				}
				out += "\n";
				out.append(indent, ' ');
			}
			out += "}";
		}
		}, v.value);
}

std::string JsonValue::escape_string(const std::string& s) {
	std::string out;
	out.reserve(s.size() + 2);
	out.push_back('"');
	for (char c : s) {
		switch (c) {
		case '"':  out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\b': out += "\\b";  break;
		case '\f': out += "\\f";  break;
		case '\n': out += "\\n";  break;
		case '\r': out += "\\r";  break;
		case '\t': out += "\\t";  break;
		default:
			if (static_cast<unsigned char>(c) < 0x20) {
				std::ostringstream oss;
				oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
					<< static_cast<int>(static_cast<unsigned char>(c));
				out += oss.str();
			}
			else {
				out.push_back(c);
			}
		}
	}
	out.push_back('"');
	return out;
}


namespace jsonh {

	std::string to_string(const JsonValue& v) {
		return JsonValue::to_string(v);
	}

	std::string to_string_pretty(const JsonValue& v, int indent_step) {
		return v.to_pretty_string(indent_step);
	}

	JsonType type_of(const JsonValue& v) {
		return v.type();
	}

	bool has_key(const JsonValue& v, const std::string& key) {
		return v.has_key(key);
	}

	JsonValue random_json(std::mt19937& rng, int depth) {
		return JsonValue::random(rng, depth);
	}

	bool validate_object_schema(const JsonValue& v,
		const std::vector<FieldRequirement>& schema,
		std::string& error_out) {
		if (!std::holds_alternative<JsonValue::object>(v.value)) {
			error_out = "Value is not an object";
			return false;
		}
		const auto& obj = std::get<JsonValue::object>(v.value);

		for (const auto& field : schema) {
			auto it = obj.find(field.name);
			if (it == obj.end()) {
				if (!field.optional) {
					error_out = "Missing required field: " + field.name;
					return false;
				}
				continue;
			}
			JsonType actual = type_of(it->second);
			if (actual != field.type) {
				error_out = "Field '" + field.name + "' has wrong type";
				return false;
			}
		}
		return true;
	}
};