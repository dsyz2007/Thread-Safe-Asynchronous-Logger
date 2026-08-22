#include "Logger.h"
#include <iostream>


int main(){
    
    //Test 1 (expect all 4 logs in debug.log)
    std::cout<<"--- Test 1: minLevel = DEBUG ---\n";
    {
        Logger logger{"debug.log", LogLevel::DEBUG};
        logger.log(LogLevel::DEBUG, "debug message");
        logger.log(LogLevel::INFO, "info message");
        logger.log(LogLevel::WARN, "warn message");
        logger.log(LogLevel::ERROR, "error message");
    }


    //Test 2 (expect 3 logs in info.log -- all except DEBUG)
    std::cout<<"--- Test 2: minLevel = INFO (+ side-test: test that INFO is default too) ---\n";
    {
        Logger logger{"info.log"}; //purposely leaving out LogLevel::INFO to test that INFO is default if no parameter provided
        logger.log(LogLevel::DEBUG, "debug message");
        logger.log(LogLevel::INFO, "info message");
        logger.log(LogLevel::WARN, "warn message");
        logger.log(LogLevel::ERROR, "error message");
    }


    //Test 3 (expect 1 log only in error.log)
    std::cout<<"--- Test 3: minLevel = ERROR (Strict threshold) ---\n";
    {
        Logger logger{"error.log", LogLevel::ERROR};
        logger.log(LogLevel::DEBUG, "debug message");
        logger.log(LogLevel::INFO, "info message");
        logger.log(LogLevel::WARN, "warn message");
        logger.log(LogLevel::ERROR, "error message");
    }


    //Test 4 (trying the failure path)
    std::cout<<"--- Test 4: bad path ---\n";
    {
        try {
            Logger bad{"/no/such/directory/app.log"};
            std::cout<<"BUG: constructor should have thrown!\n";
        } catch(const std::exception& e){
            std::cout<<"Caught as per expected: "<< e.what() <<'\n';
        }
    }

}