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
#include <thread>




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

    std::cerr<<"Successful logger creation: "<<filename<<" "<<levelToString(minLevel_)<<'\n';
}


Logger::~Logger(){
    {
        std::lock_guard<std::mutex> lock{mutex_};
        stop_.store(true, std::memory_order_release);
    }
    cv_.notify_one();
    worker_.join(); //makes sure that worker_ is not destroyed until consumerLoop() returns (finish logging all lines) cos worker_ is still needed to finish the remaining logs left in queue_ even after Logger object is destroyed
    std::cerr<<"Logger destroyed successfully: "<<filename_<<'\n';
}


void Logger::rotateIfNeeded(){

    //check if file is full anot, if not full return and don't need rotate file
    if(file_.tellp() <= static_cast<std::streampos>(maxBytes_)) return;

    std::cerr<<"Bytes already written in old file: "<<file_.tellp()<<'\n';
    
    file_.close(); //close file before renaming, renaming an open file doesn't work in Windows so just close first to be safe

    std::string archive_name = filename_ + "." + std::to_string(++rotationCount_);

    //rename the file to its archived name
    std::filesystem::rename(filename_, archive_name);
    std::cerr<<"Archived file name: "<<archive_name<<'\n';
    
    //set file_ to a new file
    file_.open(filename_, std::ios::app); //append mode

    if(!file_){ //if this triggers, output the diagnostics
        std::cerr<<"Logger failed to reopen: "<<filename_<<" after rotation.\n";
    }

    //check that new file is opened (expect output = 0)
    std::cerr<<"Bytes written in new file: "<<file_.tellp()<<'\n';

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
    Node* n = new Node{LogMessage{level, std::chrono::system_clock::now(), std::move(text)}, nullptr};

    n->next = head_.load(std::memory_order_relaxed); // std::memory_order_relaxed: "Just make this single variable atomic. I don't care about anything else: give CPU full freedom to reorder any other surrounding instructions for CPU optimisation."

    //Compare-and-Swap (CAS) single atomic Loop 
    //[Set this variable to X, but only if it still holds the value I last saw. If someone changed it, tell me what it holds now and don't write]
    //head_.compare_exchange_weak(expected, desired, whattodoif_success, whattodoif_failure)
    while(!head_.compare_exchange_weak(n->next, n, std::memory_order_release, std::memory_order_relaxed)){}
    //in above line, "expected" (n->next) is passed by reference, so if fail the function writes current value back to variable, don't need re-read anything, the failed attempt will hand u fresh data automatically.
    //Sudocode: if head_ == expected, set head_ = desired and return true. Else, set expected = head_ and return false.

    // std::memory_order_release: "Gurantee data is updated completely and pack it completely into box and seal the box shut (prevent further updates) and deliver straight to recipients."

    //after lock-free push, producer wakes up consumer if consumer is sleeping
    if(sleeping_.load(std::memory_order_acquire)){
        std::lock_guard<std::mutex> lock{mutex_};
        cv_.notify_one();
    }
}



void Logger::consumerLoop(){
    std::cerr<<"Worker thread started\n";

    for(;;){
        
        // detaches head_'s data from head_ in one CPU instruction, head_ become nullptr, give exclusive ownership of data to "list"
        Node* list = head_.exchange(nullptr, std::memory_order_acquire); //claim everything in one atomic operation
        // std::memory_order_acquire: "Open the box sealed by 'std::memory_order_release' and look inside box."
        // "release" and "acquire" always form a handshake synchronisation pair

        if(!list){
            if(stop_.load(std::memory_order_acquire)){ //.load() on an atomic type variable is an atomic read operation
                list = head_.exchange(nullptr, std::memory_order_acquire); //IMPORTANT: final sweep is compulsory cos there might be last minute changes made by other threads just right before this line
                if(!list) break;
            }else{

                //when lock-free queue is empty, sleep the consumer thread
                std::unique_lock<std::mutex> lock{mutex_};
                sleeping_.store(true, std::memory_order_release);
                cv_.wait(lock, [this]{return head_.load(std::memory_order_acquire) != nullptr || stop_.load(std::memory_order_acquire); });
                sleeping_.store(false, std::memory_order_release);

                continue;
            }
        }

        //the current queue is LIFO so we need to reverse it to make it FIFO
        Node* fifo = nullptr;
        while(list){ //continue while current "list" node is non-nullptr
            Node* nextnode = list->next; //step 1: save where we're going next
            list->next = fifo; //step 2: next-ptr of cur "list" node points to cur front node of "fifo" (reversed version of "list")
            fifo = list; //step 3: ex-last node is now the front of reversed list
            list = nextnode; //step 4: advance to address saved in step 1
        }

        //write and free
        while(fifo){ //continue while cur fifo node is non-nullptr
            writeLine(fifo->msg);
            Node* nextnode = fifo->next; //save BEFORE delete
            delete fifo;
            fifo = nextnode;
        }
    }

}


