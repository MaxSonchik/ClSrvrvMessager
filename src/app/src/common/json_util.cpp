#include "common/json_util.h"
#include <sstream>
#include <iomanip>

namespace common {

std::string escape_json(const std::string& s) {
    std::ostringstream oss;
    oss << std::hex;
    for (char c : s) {
        switch (c) {
            case '\"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    oss << "\\u" << std::setw(4) << std::setfill('0') << (int)c;
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

std::string make_json_event(const std::string& event,
                             const std::string& from,
                             const std::string& to,
                             const std::string& text,
                             const std::string& file_name,
                             size_t file_size,
                             int chunk_id,
                             int total_chunks) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"event\":\"" << event << "\"";
    if (!from.empty()) {
        oss << ", \"from\":\"" << escape_json(from) << "\"";
    }
    if (!to.empty()) {
        oss << ", \"to\":\"" << escape_json(to) << "\"";
    }
    if (!text.empty()) {
        oss << ", \"text\":\"" << escape_json(text) << "\"";
    }
    if (!file_name.empty()) {
        oss << ", \"file_name\":\"" << escape_json(file_name) << "\"";
    }
    if (file_size != 0) {
        oss << ", \"file_size\":" << file_size;
    }
    if (chunk_id >= 0) {
        oss << ", \"chunk_id\":" << chunk_id;
    }
    if (total_chunks >= 0) {
        oss << ", \"total_chunks\":" << total_chunks;
    }
    oss << "}";
    return oss.str();
}

} // namespace common
