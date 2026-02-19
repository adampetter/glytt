#pragma once

#include <sys/time.h> // For settimeofday and timeval
#include <ctime>      // For mktime

struct DateTime
{
private:
    static const char *weekdays[];
    static const char *months[];
    static const char *shortmonths[];

    unsigned short year;
    Byte month;
    Byte day;
    Byte hour;
    Byte minute;
    Byte second;

public:
    // Default constructor sets the DateTime to the epoch start
    DateTime() : year(1970), month(1), day(1), hour(0), minute(0), second(0) {}

    // Constructor for specific date and time
    DateTime(unsigned short year, Byte month, Byte day, Byte hour = 0, Byte minute = 0, Byte second = 0)
        : year(year), month(month), day(day), hour(hour), minute(minute), second(second) {}

    DateTime(const DateTime &other)
        : year(other.year), month(other.month), day(other.day),
          hour(other.hour), minute(other.minute), second(other.second)
    {
    }

    unsigned short Year() const
    {
        return this->year;
    }

    void Year(unsigned short year)
    {
        this->year = year;
    }

    Byte Month() const
    {
        return this->month;
    }

    void Month(Byte month)
    {
        this->month = month;
    }

    Byte Day() const
    {
        return this->day;
    }

    void Day(Byte day)
    {
        this->day = day;
    }

    Byte Hour() const
    {
        return this->hour;
    }

    void Hour(Byte hour)
    {
        if (hour >= 24)
            hour = 0;
        else if (hour < 0)
            hour = 0;

        this->hour = hour;
    }

    Byte Minute() const
    {
        return this->minute;
    }

    void Minute(Byte minute)
    {
        this->minute = minute;
    }

    Byte Second() const
    {
        return this->second;
    }

    void Second(Byte second)
    {
        this->second = second;
    }

    const char *GetWeekdayName() const
    {
        return DateTime::GetWeekdayName(this->day);
    }

    const char *GetMonthName() const
    {
        return DateTime::GetMonthName(this->month);
    }

    const char *GetShortMonthName() const
    {
        return DateTime::GetShortMonthName(this->month);
    }

    void Add(int hours, int minutes, int seconds)
    {
        this->second += seconds;
        if (this->second >= 60)
        {
            this->second -= 60;
            minutes++;
        }

        this->minute += minutes;
        if (this->minute >= 60)
        {
            this->minute -= 60;
            hours++;
        }

        this->hour += hours;
        if (this->hour >= 24)
        {
            this->hour -= 24;
            this->day++;
        }

        Byte days_in_month = GetDaysInMonth(this->month, this->year);

        if (this->day > days_in_month)
        {
            this->day -= days_in_month;
            this->month++;
        }

        if (this->month > 12)
        {
            this->month -= 12;
            this->year++;
        }
    }

    bool operator==(const DateTime &other) const
    {
        return year == other.year && month == other.month && day == other.day &&
               hour == other.hour && minute == other.minute && second == other.second;
    }

    bool operator<(const DateTime &other) const
    {
        if (year != other.year)
            return year < other.year;
        if (month != other.month)
            return month < other.month;
        if (day != other.day)
            return day < other.day;
        if (hour != other.hour)
            return hour < other.hour;
        if (minute != other.minute)
            return minute < other.minute;
        return second < other.second;
    }

    bool operator>(const DateTime &other) const
    {
        return other < *this;
    }

    bool operator<=(const DateTime &other) const
    {
        return !(*this > other);
    }

    bool operator>=(const DateTime &other) const
    {
        return !(*this < other);
    }

    static bool IsLeapYear(int year)
    {
        return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    }

    static long Between(const DateTime &start, const DateTime &end)
    {
        struct tm tm1 = {0}, tm2 = {0};

        // Fill tm structure for the first DateTime (dt1)
        tm1.tm_year = start.year - 1900; // year since 1900
        tm1.tm_mon = start.month - 1;    // month is 0-indexed
        tm1.tm_mday = start.day;         // day of the month
        tm1.tm_hour = start.hour;
        tm1.tm_min = start.minute;
        tm1.tm_sec = start.second;

        // Fill tm structure for the second DateTime (dt2)
        tm2.tm_year = end.year - 1900;
        tm2.tm_mon = end.month - 1;
        tm2.tm_mday = end.day;
        tm2.tm_hour = end.hour;
        tm2.tm_min = end.minute;
        tm2.tm_sec = end.second;

        // Convert both tm structures to time_t (seconds since epoch)
        time_t time1 = mktime(&tm1);
        time_t time2 = mktime(&tm2);

        // Return the difference in seconds
        return difftime(time2, time1); // difftime returns the difference in seconds
    }

    static const char *GetWeekdayName(int dayOfWeek)
    {
        if (dayOfWeek >= 1 && dayOfWeek <= 7)
        {
            return weekdays[dayOfWeek - 1];
        }
        else
        {
            return "Invalid day";
        }
    }

    static const char *GetMonthName(int month)
    {
        if (month >= 1 && month <= 12)
        {
            return months[month - 1];
        }
        else
        {
            return "Invalid month";
        }
    }

    static const char *GetShortMonthName(int month)
    {
        if (month >= 1 && month <= 12)
        {
            return shortmonths[month - 1];
        }
        else
        {
            return "Invalid month";
        }
    }

    static Byte GetDaysInMonth(int month, int year)
    {
        if (month == 2)
            return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;

        if (month == 4 || month == 6 || month == 9 || month == 11)
            return 30;
        return 31;
    }

    static DateTime Now()
    {
        // Get the current system time
        struct timeval tv;
        gettimeofday(&tv, NULL);

        // Convert the seconds part of timeval to a tm struct (local time)
        time_t now_time = tv.tv_sec;
        struct tm *timeinfo = localtime(&now_time);

        // Create a DateTime object using the current time
        return DateTime(
            timeinfo->tm_year + 1900, // Year since 1900, convert to full year
            timeinfo->tm_mon + 1,     // Month is zero-indexed (0 = January), add 1
            timeinfo->tm_mday,        // Day of the month
            timeinfo->tm_hour,        // Hour
            timeinfo->tm_min,         // Minute
            timeinfo->tm_sec          // Second
        );
    }

    static void Set(const DateTime &now)
    {
        struct tm t = {0};

        // Set time
        t.tm_year = now.year - 1900; // Set the year (tm_year is years since 1900)
        t.tm_mon = now.month - 1;    // Set the month (tm_mon is zero-indexed, 0 = January)
        t.tm_mday = now.day;
        t.tm_hour = now.hour;
        t.tm_min = now.minute;
        t.tm_sec = now.second;

        // Convert tm structure to time_t (seconds since epoch)
        time_t epoch_time = mktime(&t);

        if (epoch_time == -1)
        {
            // Handle error if mktime fails
            return;
        }

        // Create a timeval structure
        struct timeval tv;
        tv.tv_sec = epoch_time; // Seconds since epoch
        tv.tv_usec = 0;         // Microseconds (set to 0)

        // Set the system time
        settimeofday(&tv, NULL);
    }

    static unsigned long Id()
    {
        // Get the current time in seconds and microseconds
        struct timeval tv;
        gettimeofday(&tv, nullptr);

        // Convert seconds and microseconds to unsigned long
        unsigned long seconds = static_cast<unsigned long>(tv.tv_sec); // Seconds since epoch (Jan 1, 1970)
        // unsigned long microseconds = static_cast<unsigned long>(tv.tv_usec); // Microseconds

        // Create a unique ID by combining seconds and microseconds
        return seconds; //(seconds << 20) + microseconds;
    }
};

inline const char *DateTime::weekdays[] = {
    "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

inline const char *DateTime::months[] = {
    "January", "February", "March", "April", "May", "June", "July",
    "August", "September", "October", "November", "December"};

inline const char *DateTime::shortmonths[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
    "Aug", "Sep", "Oct", "Nov", "Dec"};