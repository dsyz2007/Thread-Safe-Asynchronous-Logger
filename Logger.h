#pragma once 

#include <fstream>
#include <string>
#include <string_view>


enum class LogLevel {DEBUG, INFO, WARN, ERROR};
                    // 0 ,   1  ,  2  ,   3


class Logger {
    private:
        std::ofstream file_;
        std::string filename_;
        LogLevel minLevel_;

    public:
        //constructor (make it explicit to avoid auto type conversions which can cause hard to debug bugs)
        explicit Logger(const std::string& filename, LogLevel minLevel = LogLevel::INFO);

        //destructor
        ~Logger();

        Logger(const Logger&) = delete; //copying a Logger makes no sense so refuse it by equating it to delete (By default the compiler always writes it unless u override it or refuse it by equating to delete) [Why copying Logger will never be needed: Logger is an object that owns a live connection to the file and writes to a file, why would u want to copy an object that just writes to a file, doesn't cause any errors but makes no sense]
        Logger& operator=(const Logger&) = delete;

        void log(LogLevel level, std::string_view message);
};