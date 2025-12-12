#include "json/json.hpp"
#include "web/http_server/http_server.hpp"
#include "web/http_server/http_responses.hpp"

#include <string>
#include <chrono>
#include <ctime>
#include <memory>

namespace utils {
    using namespace http;

    inline
    long long current_time_millis() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()
        ).count();
    }

    inline
    std::string current_iso_local_datetime() {
        using namespace std::chrono;
        auto now = system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
        return buf;
    }


    bool parse_json_object(const std::string& body,
        JsonValue& out_val,
        std::unique_ptr<JsonObjectView>& out_view) {
        try {
            JsonParser parser(body);
            out_val = parser.parse();
            out_view = std::make_unique<JsonObjectView>(out_val);
            return true;
        }
        catch (...) {
            return false;
        }
    }

    bool parse_and_require_fields(HttpRequest& req,
        HttpResponse& resp,
        const std::initializer_list<const char*>& required_fields,
        JsonValue& root,
        std::unique_ptr<JsonObjectView>& obj) {
        if (!parse_json_object(req.body, root, obj)) {
            respond::BAD_REQUEST(resp);
            return false;
        }

        for (const char* field : required_fields) {
            if (!obj->has(field)) {
                respond::BAD_REQUEST(resp);
                return false;
            }
        }

        return true;
    }
};