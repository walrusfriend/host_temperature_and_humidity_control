#pragma once

#include <Arduino.h>
#include <vector>
#include <charconv>

namespace Calendar {

    struct Time {
    public:
        uint8_t hour = 0;
        uint8_t min = 0;

        void from_string(const std::string_view& time) {
            auto separator_pos = time.find(":");
            const std::string_view&& hours_str = time.substr(0, separator_pos);

            // Serial.printf("from_string() - hours string: %s\n", hours_str.data());

            const std::string_view&& min_str = time.substr(separator_pos + 1, 2);

            // Serial.printf("from_string() - mins string: %s\n", min_str.data());

            int tmp_hour = 0;
            auto result = std::from_chars(hours_str.data(), hours_str.data() + hours_str.size(), tmp_hour);

            if (result.ec == std::errc::invalid_argument) {
                Serial.println("Calendar::Time::from_string() - Couldn't convert hours string value to integer");
                return;
            }

            int tmp_min = 0;
            result = std::from_chars(min_str.data(), min_str.data() + min_str.size(), tmp_min);
            if (result.ec == std::errc::invalid_argument) {
                Serial.println("Calendar::Time::from_string() - Couldn't convert minutes string value to integer");
                return;
            }

            // If all checks passed save new values
            hour = tmp_hour;
            min = tmp_min;
        }

        friend bool operator==(const Time& t1, const Time& t2) {
            return (t1.hour == t2.hour) and (t1.min == t2.min);
        }

        friend bool operator!=(const Time& t1, const Time& t2) {
            return !(t1 == t2);
        }

        friend bool operator>(const Time& t1, const Time& t2) {
            if (t1.hour == t2.hour)
                return t1.min > t2.min;
            else
                return t1.hour > t2.hour;
        }

        friend bool operator<(const Time& t1, const Time& t2) {
            return (t2 > t1);
        }
        
        friend bool operator>=(const Time& t1, const Time& t2) {
            return (t1 > t2) or (t1 == t2);    
        }
        
        friend bool operator<=(const Time& t1, const Time& t2) {
            return (t1 < t2) or (t1 == t2);
        }
    };

    struct Unit {
    public:
        uint8_t id {0};
        Time start {0, 0};
        Time stop {0, 0};
        std::array<uint8_t, 7> days {0, 0, 0, 0, 0, 0, 0};

        bool consist(const Time& time) const{
            return (start <= time) and (time <= stop);
        }
    };

    static std::vector<Unit> list;
}