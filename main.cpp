#include "Logger.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>



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


    //Test 5 (test mixed-type call for template)
    std::cout<<"--- Test 5: Mixed-type call ---\n";
    {
        Logger logger{"warn.log", LogLevel::WARN};
        int orderId = 8812;
        logger.log(LogLevel::WARN, "order ", orderId, " rejected, price=", 192.40);
    }


    //Test 6 (launch several threads and cause data race)
    {
        Logger logger{"race.log", LogLevel::DEBUG};
        std::vector<std::thread> threads;

        for(int t = 0; t < 8; ++t){
            threads.emplace_back(
                [&logger, t]{ //lambda (anonymous inline function)
                    for(int i = 0;i < 2000;i++){
                        logger.log(LogLevel::INFO, "thread ", t, " iteration ", i, " padding-padding-padding");
                    }
                });
        }

        for(auto& th : threads) th.join();
    }


    //Time 10000 single-threaded log() calls [Stage 3 Results: 35884 us total for 10000 calls, 3.5884us/call]
    {
        Logger logger{"time.log"};

        const auto start = std::chrono::steady_clock::now();
        
        for(int i = 0; i < 10000; ++i) logger.log(LogLevel::INFO, "benchmark", i);

        const auto end = std::chrono::steady_clock::now();

        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        std::cout<< us << " us total, " << (double)us / 10000 <<"us/call\n";
    }
}