#pragma once 

#include <fstream>
#include <string>
#include <string_view>
#include <sstream>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <atomic>






enum class LogLevel {DEBUG, INFO, WARN, ERROR};
                    // 0 ,   1  ,  2  ,   3



struct LogMessage {
    LogLevel level;
    std::chrono::system_clock::time_point timestamp;
    std::string text;
};



struct Node {
    LogMessage msg;
    Node* next;
};




class Logger {
    private:
        std::ofstream file_;
        std::string filename_;
        LogLevel minLevel_;
        std::mutex mutex_;
        std::condition_variable cv_;
        std::thread worker_; 
        std::size_t maxBytes_; //threshold to switch to new file if existing file too full
        int rotationCount_ = 0; //used for naming the archived file
        std::atomic<Node*> head_{nullptr};
        std::atomic<bool> stop_{false};
        std::atomic<bool> sleeping_{false};
        void writeLine(const LogMessage& msg);
        void enqueue(LogLevel level, std::string text);
        void consumerLoop();
        void rotateIfNeeded();

    public:
        //constructor (make it explicit to avoid auto type conversions which can cause hard to debug bugs)
        explicit Logger(const std::string& filename, LogLevel minLevel = LogLevel::INFO, std::size_t maxBytes = 1024 * 1024);


        //destructor
        ~Logger();


        Logger(const Logger&) = delete; //copying a Logger makes no sense so refuse it by equating it to delete (By default the compiler always writes it unless u override it or refuse it by equating to delete) [Why copying Logger will never be needed: Logger is an object that owns a live connection to the file and writes to a file, why would u want to copy an object that just writes to a file, doesn't cause any errors but makes no sense]
        Logger& operator=(const Logger&) = delete;


        template <typename... Args>  //declare a pack of types (Args is a Pack: zero or more types. A pack is a compile-time list that gets pasted into generated code, nothing about it survives to runtime)
        void log(LogLevel level, Args&&... args) { // && is forwarding reference (will auto adapt to lvalue reference "&" or rvalue reference "&&" depending on whether parameter is lvalue or rvalue)
            if(level < minLevel_) return; //filter first so that dropped DEBUG calls no need to waste runtime and resources to construct an unused ostringstream
            std::ostringstream oss;

            //format: (init operation ... operation pack)   [Note: () encasing is compulsory as per C++ syntax]
            (void)(oss << ... << args); // (void) is to handle the edge case: no arguments (empty)
            enqueue(level, oss.str()); //defined in Logger.cpp, private, non-template
        }
};