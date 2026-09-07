#include "Logger.h"
#include <vector>
#include <algorithm>   //for std::sort
#include <iostream>



int main(){

    constexpr int kThreads = 8;
    constexpr int kPerThread = 50000;
    constexpr int kWarmup = 1000;


    Logger logger{"bench.log", LogLevel::INFO, 1ull << 30}; //we specifically set the max size to be very large to avoid file rotation during this testing

    std::vector<std::vector<long long>> samples(kThreads); //one vector PER THREAD

    for(auto& v : samples) v.reserve(kPerThread); //pre-allocate memory so that allocation of memory doesn't affect tests timings later on 

    const auto wallStart = std::chrono::steady_clock::now();

    std::vector<std::thread> ts;
    for(int t = 0;t < kThreads; ++t){
        ts.emplace_back(
            [&, t]{ // passing "&" to lamda gives lamda access to all local variables in current scope
                auto& out = samples[t];

                for(int i = 0;i < kWarmup; ++i){
                    logger.log(LogLevel::INFO, "warmup ", i);
                }

                for(int i = 0;i < kPerThread; ++i){
                    const auto a = std::chrono::steady_clock::now();
                    logger.log(LogLevel::INFO, "thread ", t, " msg ", i);
                    const auto b = std::chrono::steady_clock::now();
                    out.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count());
                }
            }
        );
    }


    for(auto& th : ts){
        th.join();
    }

    const auto wallEnd = std::chrono::steady_clock::now();


    std::vector<long long> all;
    all.reserve(kThreads * kPerThread);
    for(auto& v : samples) all.insert(all.end(), v.begin(), v.end());
    std::sort(all.begin(), all.end());

    auto pct = [&](double p){ //what is the timing of the xth percentile call of log (among 240,000 timed calls)
        size_t i = static_cast<size_t>(all.size() * p / 100.0);
        return all[std::min(i, all.size() - 1)]; //clamp to prevent p=100 overrunning
    };


    const double numofseconds = std::chrono::duration<double>(wallEnd - wallStart).count();
    std::cout<<"Throughput: "<<(kThreads * kPerThread) / numofseconds<<" msgs/sec\n"; //throughput


    //[Stage 5] mutex + conditional_variable: 8 producers x 50k messages, -O2
    std::cout<<"p50:   "<<pct(50)<<" ns\n"; //50th percentile
    std::cout<<"p99:   "<<pct(99)<<" ns\n"; //99th percentile (at least 99% of calls faster than this timing)
    std::cout<<"p99.9:   "<<pct(99.9)<<" ns\n"; //99.9th percentile
    std::cout<<"max (p100):   "<<pct(100)<<" ns\n"; //the slowest call recorded (100th percentile)

}



/* Stage 5's (mutex + conditional_variable) performance

Throughput: 993875 msgs/sec
p50:   1353 ns
p99:   108291 ns
p99.9:   797130 ns
max (p100):   11743311 ns

*/