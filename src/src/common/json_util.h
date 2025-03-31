#ifndef COMMON_JSON_UTIL_H
#define COMMON_JSON_UTIL_H

#include <string>

namespace common {
    // Escapes special characters in a string for JSON output
    std::string escape_json(const std::string& s);

    // Constructs a JSON-formatted event string with given fields (unused fields can be left default)
    std::string make_json_event(const std::string& event,
                                 const std::string& from = "",
                                 const std::string& to = "",
                                 const std::string& text = "",
                                 const std::string& file_name = "",
                                 size_t file_size = 0,
                                 int chunk_id = -1,
                                 int total_chunks = -1);
}

#endif
