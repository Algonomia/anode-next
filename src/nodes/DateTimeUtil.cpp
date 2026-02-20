#include "nodes/DateTimeUtil.hpp"
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <map>
#include <cctype>
#include <ctime>

namespace nodes {

int64_t convertDateToTimestamp(const std::string& dateString) {
    // 1. Numeric: raw Unix timestamp
    try {
        size_t pos;
        int64_t timestamp = std::stoll(dateString, &pos);
        if (pos == dateString.size()) {
            return timestamp;
        }
    } catch (...) {}

    int day = 0, month = 0, year = 0;
    int hour = 0, min = 0, sec = 0;

    // 2. ISO/PostgreSQL: yyyy-mm-dd [HH:MM:SS[.ffffff]]
    if (dateString.size() >= 10 && dateString[4] == '-') {
        year = std::stoi(dateString.substr(0, 4));
        month = std::stoi(dateString.substr(5, 2)) - 1;
        day = std::stoi(dateString.substr(8, 2));

        // Optional time part
        if (dateString.size() >= 19 && (dateString[10] == ' ' || dateString[10] == 'T')) {
            hour = std::stoi(dateString.substr(11, 2));
            min = std::stoi(dateString.substr(14, 2));
            sec = std::stoi(dateString.substr(17, 2));
            // Ignore fractional seconds (.ffffff) if present
        }
    }
    // 3. Slash: dd/mm/yyyy or dd/mm/yy
    else if (dateString.find('/') != std::string::npos) {
        size_t pos1 = dateString.find('/');
        size_t pos2 = dateString.find('/', pos1 + 1);
        if (pos2 != std::string::npos) {
            day = std::stoi(dateString.substr(0, pos1));
            month = std::stoi(dateString.substr(pos1 + 1, pos2 - pos1 - 1)) - 1;
            year = std::stoi(dateString.substr(pos2 + 1));
            if (year < 100) {
                year += (year < 70) ? 2000 : 1900;
            }
        }
    }
    // 4. Text: dd month yyyy (FR/EN with abbreviations)
    else if (dateString.find(' ') != std::string::npos) {
        static const std::map<std::string, int> months = {
            {"janvier", 0}, {"january", 0}, {"jan", 0},
            {"février", 1}, {"february", 1}, {"feb", 1}, {"fevrier", 1},
            {"mars", 2}, {"march", 2}, {"mar", 2},
            {"avril", 3}, {"april", 3}, {"apr", 3},
            {"mai", 4}, {"may", 4},
            {"juin", 5}, {"june", 5}, {"jun", 5},
            {"juillet", 6}, {"july", 6}, {"jul", 6},
            {"août", 7}, {"august", 7}, {"aug", 7}, {"aout", 7},
            {"septembre", 8}, {"september", 8}, {"sep", 8},
            {"octobre", 9}, {"october", 9}, {"oct", 9},
            {"novembre", 10}, {"november", 10}, {"nov", 10},
            {"décembre", 11}, {"december", 11}, {"dec", 11}, {"decembre", 11}
        };

        std::istringstream iss(dateString);
        std::string dayStr, monthStr, yearStr;
        iss >> dayStr >> monthStr >> yearStr;

        if (!dayStr.empty() && !monthStr.empty() && !yearStr.empty()) {
            day = std::stoi(dayStr);
            year = std::stoi(yearStr);

            std::string monthLower;
            for (char c : monthStr) {
                monthLower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }

            auto it = months.find(monthLower);
            if (it != months.end()) {
                month = it->second;
            } else {
                throw std::runtime_error("Unknown month: " + monthStr);
            }
        }
    }

    if (day == 0 || year == 0) {
        throw std::runtime_error("Unable to parse date: " + dateString);
    }

    struct tm timeinfo = {};
    timeinfo.tm_mday = day;
    timeinfo.tm_mon = month;
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = min;
    timeinfo.tm_sec = sec;

    time_t timestamp = timegm(&timeinfo);
    return static_cast<int64_t>(timestamp);
}

} // namespace nodes
