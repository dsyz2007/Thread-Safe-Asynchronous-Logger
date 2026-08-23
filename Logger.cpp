#include "Logger.h"
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>



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


Logger::Logger(const std::string& filename, LogLevel minLevel) 
    : file_{filename, std::ios::app}, filename_{filename}, minLevel_{minLevel}
{
    if(!file_){
        throw std::runtime_error("Logger failed to open " + filename + "!");
    }
    std::cout<<"Successful logger creation: "<<filename<<" "<<levelToString(minLevel_)<<'\n';
}


Logger::~Logger(){
    std::cout<<"Logger destroyed successfully: "<<filename_<<'\n';
}



void Logger::writeLine(LogLevel level, std::string_view message){

    if(level < minLevel_) return;

    //current time (obtain from wall clock)
    const auto now = std::chrono::system_clock::now();

    //collapse to time_t (a plain integer count of SECONDS since 1970)
    //this is a bridge to C calender functions. sub-second details are lost here.
    const std::time_t t = std::chrono::system_clock::to_time_t(now);

    //break that integer into calender fields: year, month, day, hour, min, sec
    // std::tm is a plain struct holding those. The {} zero-initialises it.
    std::tm tm{};
    localtime_r(&t, &tm); // "&tm" = write the result into my struct

    
    // Format those fields as text. '/n' writes new line to file so that next iteration starts on a new line in file
    // put_time is a stream manipulator: it doesn't return a string, it prints when streamed.
    file_ << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    file_ << " ["<<levelToString(level)<<"] "<<message<<'\n';

}
