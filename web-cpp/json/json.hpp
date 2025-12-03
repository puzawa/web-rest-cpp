#include <iostream>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <random>
#include <optional>
#include <type_traits>

namespace tests {
	int RunJsonTests(bool verbose);
};

enum class JsonType {
	Null,
	Bool,
	Number,
	String,
	Array,
	Object
};

struct JsonValue {
	using array = std::vector<JsonValue>;
	using object = std::unordered_map<std::string, JsonValue>;
	using variant_t = std::variant<std::nullptr_t, bool, double, std::string, array, object>;

	variant_t value;

	JsonValue() : value(nullptr) {}
	JsonValue(std::nullptr_t) : value(nullptr) {}
	JsonValue(bool b) : value(b) {}
	JsonValue(double d) : value(d) {}
	JsonValue(const std::string& s) : value(s) {}
	JsonValue(std::string&& s) : value(std::move(s)) {}
	JsonValue(const char* s) : value(std::string(s)) {}
	JsonValue(const array& a) : value(a) {}
	JsonValue(array&& a) : value(std::move(a)) {}
	JsonValue(const object& o) : value(o) {}
	JsonValue(object&& o) : value(std::move(o)) {}

	bool operator==(const JsonValue& other) const {
		return value == other.value;
	}

	JsonType type() const {
		if (std::holds_alternative<std::nullptr_t>(value)) return JsonType::Null;
		if (std::holds_alternative<bool>(value))          return JsonType::Bool;
		if (std::holds_alternative<double>(value))        return JsonType::Number;
		if (std::holds_alternative<std::string>(value))   return JsonType::String;
		if (std::holds_alternative<array>(value))         return JsonType::Array;
		return JsonType::Object;
	}

	bool has_key(const std::string& key) const {
		if (!std::holds_alternative<object>(value)) return false;
		const auto& obj = std::get<object>(value);
		return obj.find(key) != obj.end();
	}

	std::string to_string() const {
		return JsonValue::to_string(*this);
	}

	std::string to_pretty_string(int indent_step = 2) const {
		std::string out;
		JsonValue::to_string_pretty_impl(*this, out, 0, indent_step);
		return out;
	}

	static JsonValue random(std::mt19937& rng, int depth = 0);

	static std::string to_string(const JsonValue& v);

	static void to_string_pretty_impl(const JsonValue& v, std::string& out, int indent, int indent_step);

	static std::string escape_string(const std::string& s);
};

class JsonParser {
public:
	explicit JsonParser(const std::string& text) : s(text), pos(0) {}

	JsonValue parse() {
		skip_ws();
		JsonValue v = parse_value();
		skip_ws();
		if (pos != s.size()) {
			throw std::runtime_error("Extra characters after valid JSON");
		}
		return v;
	}

private:
	const std::string& s;
	std::size_t pos;

	void skip_ws() {
		while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
	}

	char get() {
		if (pos >= s.size()) throw std::runtime_error("Unexpected end of input");
		return s[pos++];
	}

	void expect(const char* literal) {
		for (const char* p = literal; *p; ++p) {
			if (pos >= s.size() || s[pos] != *p) {
				throw std::runtime_error(std::string("Expected '") + literal + "'");
			}
			++pos;
		}
	}

	JsonValue parse_value() {
		if (pos >= s.size()) throw std::runtime_error("Unexpected end of input while parsing value");
		char c = s[pos];
		switch (c) {
		case 'n': return parse_null();
		case 't': return parse_true();
		case 'f': return parse_false();
		case '"': return parse_string();
		case '[': return parse_array();
		case '{': return parse_object();
		default:
			if (c == '-' || (c >= '0' && c <= '9')) {
				return parse_number();
			}
			throw std::runtime_error(std::string("Unexpected character while parsing value: ") + c);
		}
	}

	JsonValue parse_null() {
		expect("null");
		return JsonValue(nullptr);
	}

	JsonValue parse_true() {
		expect("true");
		return JsonValue(true);
	}

	JsonValue parse_false() {
		expect("false");
		return JsonValue(false);
	}

	JsonValue parse_number() {
		std::size_t start = pos;
		if (s[pos] == '-') ++pos;
		if (pos >= s.size()) throw std::runtime_error("Invalid number");
		if (s[pos] == '0') {
			++pos;
		}
		else if (s[pos] >= '1' && s[pos] <= '9') {
			while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
		}
		else {
			throw std::runtime_error("Invalid number");
		}
		if (pos < s.size() && s[pos] == '.') {
			++pos;
			if (pos >= s.size() || !std::isdigit(static_cast<unsigned char>(s[pos])))
				throw std::runtime_error("Invalid number");
			while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
		}
		if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
			++pos;
			if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) ++pos;
			if (pos >= s.size() || !std::isdigit(static_cast<unsigned char>(s[pos])))
				throw std::runtime_error("Invalid number");
			while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
		}
		double value = std::strtod(s.c_str() + start, nullptr);
		return JsonValue(value);
	}

	JsonValue parse_string() {
		if (get() != '"') throw std::runtime_error("Expected opening quote for string");
		std::string result;
		while (true) {
			if (pos >= s.size()) throw std::runtime_error("Unterminated string");
			char c = get();
			if (c == '"') break;
			if (c == '\\') {
				if (pos >= s.size()) throw std::runtime_error("Unterminated escape sequence");
				char e = get();
				switch (e) {
				case '"': result.push_back('"'); break;
				case '\\': result.push_back('\\'); break;
				case '/': result.push_back('/'); break;
				case 'b': result.push_back('\b'); break;
				case 'f': result.push_back('\f'); break;
				case 'n': result.push_back('\n'); break;
				case 'r': result.push_back('\r'); break;
				case 't': result.push_back('\t'); break;
				case 'u': {
					if (pos + 4 > s.size()) throw std::runtime_error("Invalid unicode escape");
					unsigned code = 0;
					for (int i = 0; i < 4; ++i) {
						char h = s[pos++];
						code <<= 4;
						if (h >= '0' && h <= '9') code |= (h - '0');
						else if (h >= 'a' && h <= 'f') code |= (h - 'a' + 10);
						else if (h >= 'A' && h <= 'F') code |= (h - 'A' + 10);
						else throw std::runtime_error("Invalid unicode escape");
					}
					if (code <= 0x7F) {
						result.push_back(static_cast<char>(code));
					}
					else if (code <= 0x7FF) {
						result.push_back(static_cast<char>(0xC0 | (code >> 6)));
						result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
					}
					else {
						result.push_back(static_cast<char>(0xE0 | (code >> 12)));
						result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
						result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
					}
					break;
				}
				default:
					throw std::runtime_error("Invalid escape character in string");
				}
			}
			else {
				result.push_back(c);
			}
		}
		return JsonValue(std::move(result));
	}

	JsonValue parse_array() {
		if (get() != '[') throw std::runtime_error("Expected '['");
		JsonValue::array arr;
		skip_ws();
		if (pos < s.size() && s[pos] == ']') {
			++pos;
			return JsonValue(std::move(arr));
		}
		while (true) {
			skip_ws();
			arr.push_back(parse_value());
			skip_ws();
			if (pos >= s.size()) throw std::runtime_error("Unterminated array");
			char c = get();
			if (c == ']') break;
			if (c != ',') throw std::runtime_error("Expected ',' or ']' in array");
		}
		return JsonValue(std::move(arr));
	}

	JsonValue parse_object() {
		if (get() != '{') throw std::runtime_error("Expected '{'");
		JsonValue::object obj;
		skip_ws();
		if (pos < s.size() && s[pos] == '}') {
			++pos;
			return JsonValue(std::move(obj));
		}
		while (true) {
			skip_ws();
			if (pos >= s.size() || s[pos] != '"') throw std::runtime_error("Expected string key in object");
			JsonValue key = parse_string();
			if (!std::holds_alternative<std::string>(key.value)) throw std::runtime_error("Key is not string");
			skip_ws();
			if (get() != ':') throw std::runtime_error("Expected ':' after key in object");
			skip_ws();
			JsonValue val = parse_value();
			obj.emplace(std::get<std::string>(key.value), std::move(val));
			skip_ws();
			if (pos >= s.size()) throw std::runtime_error("Unterminated object");
			char c = get();
			if (c == '}') break;
			if (c != ',') throw std::runtime_error("Expected ',' or '}' in object");
		}
		return JsonValue(std::move(obj));
	}
};

template<typename T>
struct always_false : std::false_type {};

class JsonObjectView {
public:
	explicit JsonObjectView(const JsonValue& v) : v_(v) {
		if (!std::holds_alternative<JsonValue::object>(v_.value)) {
			throw std::runtime_error("JsonObjectView: value is not an object");
		}
	}

	bool has(const std::string& key) const {
		const auto& obj = std::get<JsonValue::object>(v_.value);
		return obj.find(key) != obj.end();
	}

	const JsonValue& at(const std::string& key) const {
		const auto& obj = std::get<JsonValue::object>(v_.value);
		auto it = obj.find(key);
		if (it == obj.end()) {
			throw std::runtime_error("JsonObjectView::at: missing key '" + key + "'");
		}
		return it->second;
	}

	template<typename T>
	T get(const std::string& key) const {
		const JsonValue& val = at(key);

		if constexpr (std::is_same_v<T, bool>) {
			if (!std::holds_alternative<bool>(val.value))
				throw std::runtime_error("JsonObjectView::get<bool>: wrong type for '" + key + "'");
			return std::get<bool>(val.value);
		}
		else if constexpr (std::is_same_v<T, double>) {
			if (!std::holds_alternative<double>(val.value))
				throw std::runtime_error("JsonObjectView::get<double>: wrong type for '" + key + "'");
			return std::get<double>(val.value);
		}
		else if constexpr (std::is_same_v<T, std::string>) {
			if (!std::holds_alternative<std::string>(val.value))
				throw std::runtime_error("JsonObjectView::get<string>: wrong type for '" + key + "'");
			return std::get<std::string>(val.value);
		}
		else if constexpr (std::is_same_v<T, JsonValue::array>) {
			if (!std::holds_alternative<JsonValue::array>(val.value))
				throw std::runtime_error("JsonObjectView::get<array>: wrong type for '" + key + "'");
			return std::get<JsonValue::array>(val.value);
		}
		else if constexpr (std::is_same_v<T, JsonValue::object>) {
			if (!std::holds_alternative<JsonValue::object>(val.value))
				throw std::runtime_error("JsonObjectView::get<object>: wrong type for '" + key + "'");
			return std::get<JsonValue::object>(val.value);
		}
		else if constexpr (std::is_same_v<T, JsonValue>) {
			return val;
		}
		else {
			static_assert(always_false<T>::value, "JsonObjectView::get: unsupported type");
		}
	}

	template<typename T>
	std::optional<T> get_optional(const std::string& key) const {
		const auto& obj = std::get<JsonValue::object>(v_.value);
		auto it = obj.find(key);
		if (it == obj.end()) {
			return std::nullopt;
		}
		try {
			return get<T>(key);
		}
		catch (...) {
			return std::nullopt;
		}
	}

private:
	const JsonValue& v_;
};

class JsonObjectViewMut {
public:
	explicit JsonObjectViewMut(JsonValue& v) : v_(v) {
		if (!std::holds_alternative<JsonValue::object>(v_.value)) {
			throw std::runtime_error("JsonObjectViewMut: value is not an object");
		}
	}

	bool has(const std::string& key) const {
		const auto& obj = std::get<JsonValue::object>(v_.value);
		return obj.find(key) != obj.end();
	}

	JsonValue& at(const std::string& key) {
		auto& obj = std::get<JsonValue::object>(v_.value);
		auto it = obj.find(key);
		if (it == obj.end()) {
			throw std::runtime_error("JsonObjectViewMut::at: missing key '" + key + "'");
		}
		return it->second;
	}

	const JsonValue& at(const std::string& key) const {
		const auto& obj = std::get<JsonValue::object>(v_.value);
		auto it = obj.find(key);
		if (it == obj.end()) {
			throw std::runtime_error("JsonObjectViewMut::at: missing key '" + key + "'");
		}
		return it->second;
	}

	void erase(const std::string& key) {
		auto& obj = std::get<JsonValue::object>(v_.value);
		obj.erase(key);
	}

	void set(const std::string& key, bool b) {
		auto& obj = std::get<JsonValue::object>(v_.value);
		obj[key] = JsonValue(b);
	}

	void set(const std::string& key, double d) {
		auto& obj = std::get<JsonValue::object>(v_.value);
		obj[key] = JsonValue(d);
	}

	void set(const std::string& key, const std::string& s) {
		auto& obj = std::get<JsonValue::object>(v_.value);
		obj[key] = JsonValue(s);
	}

	void set(const std::string& key, const char* s) {
		auto& obj = std::get<JsonValue::object>(v_.value);
		obj[key] = JsonValue(s);
	}

	void set(const std::string& key, const JsonValue::array& a) {
		auto& obj = std::get<JsonValue::object>(v_.value);
		obj[key] = JsonValue(a);
	}

	void set(const std::string& key, const JsonValue::object& o) {
		auto& obj = std::get<JsonValue::object>(v_.value);
		obj[key] = JsonValue(o);
	}

	void set(const std::string& key, const JsonValue& v) {
		auto& obj = std::get<JsonValue::object>(v_.value);
		obj[key] = v;
	}

private:
	JsonValue& v_;
};

struct FieldRequirement {
	std::string name;
	JsonType type;
	bool optional;
};


namespace jsonh {

	std::string to_string(const JsonValue& v);

	std::string to_string_pretty(const JsonValue& v, int indent_step = 2);

	JsonType type_of(const JsonValue& v);

	bool has_key(const JsonValue& v, const std::string& key);

	JsonValue random_json(std::mt19937& rng, int depth = 0);

	bool validate_object_schema(const JsonValue& v, const std::vector<FieldRequirement>& schema, std::string& error_out);
};