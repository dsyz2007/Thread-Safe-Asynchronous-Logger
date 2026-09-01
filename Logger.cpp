#include "Logger.h"
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <filesystem>



namespace { //a namespace with no name: Everything inside is visible only in this one Logger.cpp file. The purpose is to prevent clashes of functions of same name in a massive codebase (e.g. 100+ seperate .cpp files)
    const char* levelToString(LogLevel level){
        switch(level){
            case LogLevel::DEBUG : return "DEBUG";
            case LogLevel::INFO : return "INFO";
            case LogLevel::WARN : return "WARN";
            case LogLevel::ERROR : return "ERROR";
        }
        return "UNKNOWN"; //to alert u of a weird case that doesn't fall under the 4 mentioned cases
    }
}


Logger::Logger(const std::string& filename, LogLevel minLevel, std::size_t maxBytes) 
    : file_{filename, std::ios::app}, filename_{filename}, minLevel_{minLevel}, maxBytes_{maxBytes}
{ //constructor
    if(!file_){
        throw std::runtime_error("Logger failed to open " + filename + "!");
    }

    worker_ = std::thread{&Logger::consumerLoop, this}; //this line must be after the throw, not before, cos if constructor threw with a running joinable thread, the cleanup will destroy the thread and call std::terminate to the program

    std::cout<<"Successful logger creation: "<<filename<<" "<<levelToString(minLevel_)<<'\n';
}


Logger::~Logger(){
    {  // {   ...   } is a scope
        std::lock_guard<std::mutex> lock{mutex_};
        stop_ = true; //the stop_ variable is shared by multiple threads and mutated by at least one of them so need mutex and lock
    } //lock released at "}" here
    cv_.notify_one(); //wakes up the sleeping worker thread to write into log file
    worker_.join(); //makes sure that worker_ is not destroyed until consumerLoop() returns (finish logging all lines) cos worker_ is still needed to finish the remaining logs left in queue_ even after Logger object is destroyed
    std::cout<<"Logger destroyed successfully: "<<filename_<<'\n';
}


void Logger::rotateIfNeeded(){

    //check if file is full anot, if not full return and don't need rotate file
    if(file_.tellp() <= static_cast<std::streampos>(maxBytes_)) return;

    std::cerr<<"Bytes already written in old file: "<<file_.tellp()<<'\n';
    
    file_.close(); //close file before renaming, renaming an open file doesn't work in Windows so just close first to be safe

    std::string archive_name = filename_ + "." + std::to_string(++rotationCount_);

    //rename the file to its archived name
    std::filesystem::rename(filename_, archive_name);
    std::cout<<"Archived file name: "<<archive_name<<'\n';
    
    //set file_ to a new file
    file_.open(filename_, std::ios::app); //append mode

    if(!file_){ //if this triggers, output the diagnostics
        std::cerr<<"Logger failed to reopen: "<<filename_<<" after rotation.\n";
    }

    //check that new file is opened (expect output = 0)
    std::cout<<"Bytes written in new file: "<<file_.tellp()<<'\n';

}


void Logger::writeLine(const LogMessage& msg){

    //collapse to time_t (a plain integer count of SECONDS since 1970)
    //this is a bridge to C calender functions. sub-second details are lost here.
    const std::time_t t = std::chrono::system_clock::to_time_t(msg.timestamp);

    //break that integer into calender fields: year, month, day, hour, min, sec
    // std::tm is a plain struct holding those. The {} zero-initialises it.
    std::tm tm{};
    localtime_r(&t, &tm); // "&tm" = write the result into my struct

    
    // Format those fields as text. '/n' writes new line to file so that next iteration starts on a new line in file
    // put_time is a stream manipulator: it doesn't return a string, it prints when streamed.
    file_ << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    file_ << " ["<<levelToString(msg.level)<<"] "<<msg.text<<'\n';

    //check if need to switch to new file if current file is too big
    rotateIfNeeded();

}



void Logger::enqueue(LogLevel level, std::string text){
    {
        std::lock_guard<std::mutex> lock{mutex_};
        queue_.push(LogMessage{level, std::chrono::system_clock::now(), std::move(text)});
    }
    cv_.notify_one();
}



void Logger::consumerLoop(){
    std::cout<<"Worker thread started\n";

    for(;;){
        std::unique_lock<std::mutex> uLock{mutex_};
        cv_.wait(uLock, [this]{return !queue_.empty() || stop_;});

        if(queue_.empty() && stop_){ //need check queue_.empty() on top of stop_=true cos need to prevent the edge case of queued messages being silently forgone when logging is stopped
            std::cout<<"Worker thread stopping\n";
            return;
        }

        LogMessage msg = std::move(queue_.front()); // .front() returns a reference to the element still inside queue
        queue_.pop(); 

        uLock.unlock(); //unique_lock allows manual release (lock_guard doesn't) so disk write can occur with no lock held, no need wait for I/O

        writeLine(msg);
    }

}
