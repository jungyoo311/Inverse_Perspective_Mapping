#ifndef LOGGER_H
#define LOGGER_H

#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <string>
#include <mutex>
#include <iomanip>
#include <map>
using namespace std;
using namespace std::chrono;

// Enum to represent log levels
enum LogLevel { DEBUG, INFO, WARNING, ERROR_, CRITICAL};
class Logger {
public:
    // constructor: opens the log file in append mode
    Logger(const string& filename) : logFilename(filename){
        logFile.open(filename, ios::app);
        if (!logFile.is_open()){
            cerr << "Error opening log file: " << filename << endl;
        }
    }
    // Destructor
    ~Logger(){
        if (logFile.is_open()){
            logFile.close();
        }
    }
    void log(LogLevel level, const string& message){
        // thread safe logging
        lock_guard<mutex> lock(logMutex);
        auto now = system_clock::now();
        auto time_t_now = system_clock::to_time_t(now);
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

        tm* timeinfo = localtime(&time_t_now);
        char timestamp[24];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

        //create log entry with mil seconds
        ostringstream logEntry;
        logEntry << "[" << timestamp << "." << setfill('0') << setw(3) << ms.count() << "]"
                << levelToString(level) << ": " << message << endl;
        
        // Output to console
        cout << logEntry.str();
        // Ouput to logFile
        if (logFile.is_open()){
            logFile << logEntry.str();
            logFile.flush(); // Ensure immediate write to file
        }

    }
    void startTimer(const string& operation){
        lock_guard<mutex> lock(timerMutex);
        timers[operation] = high_resolution_clock::now();
    }
    void endTimer(const string& operation){
        auto end_time = high_resolution_clock::now();
        lock_guard<mutex> lock(timerMutex);
        auto it = timers.find(operation);
        if (it != timers.end()){
            auto duration = duration_cast<microseconds>(end_time - it->second);
            double ms = duration.count() / 1000.0;
            ostringstream perfMsg;
            perfMsg << "PERF | " << operation << ": " << fixed << setprecision(3) << ms << "ms";
            log(INFO, perfMsg.str());
            timers.erase(it); // Remove the timer after use
        } else{
            log(WARNING, "Timer not found for operation: " + operation);
        }
    }
    // log performance metrics
    void logPerformance(const string& operation, double value, const string& unit = "ms"){
        ostringstream perfMsg;
        perfMsg << "PERF | " << operation << ": " << fixed << setprecision(3) << value << unit;
        log(INFO, perfMsg.str());
    }
    // log memory usage
    void logMemoryUsage(const string& context, size_t bytes){
        double mb = bytes / (1024.0 * 1024.0);
        ostringstream memMsg;
        memMsg << "MEM | " << context << ": " << fixed << setprecision(2) << mb << "MB";
        log(INFO, memMsg.str());
    }
    void logFrameRate(double fps){
        ostringstream fpsMsg;
        fpsMsg << "FPS | Current frame rate: " << fixed << setprecision(1) << fps << " fps";
        log(INFO, fpsMsg.str());
    }
private:
    string logFilename;
    ofstream logFile;
    mutex logMutex;
    mutex timerMutex;
    map<string, high_resolution_clock::time_point> timers;

    // converts log level to a string for output
    string levelToString(LogLevel level){
        switch (level)
        {
        case DEBUG:
            return "DEBUG";
        case INFO:
            return "INFO";
        case WARNING:
            return "WARN ";
        case ERROR_:
            return "ERROR";
        case CRITICAL:
            return "CRIT ";
        default:
            return "UNK ";
        }
    }
};
// global ptr to a logger instance
extern Logger* g_logger;

// what are macros for easier logging?
#define LOG_DEBUG(msg) if(g_logger) g_logger->log(DEBUG, msg)
#define LOG_INFO(msg) if(g_logger) g_logger->log(INFO, msg)
#define LOG_WARNING(msg) if(g_logger) g_logger->log(WARNING, msg)
#define LOG_ERROR(msg) if(g_logger) g_logger->log(ERROR_, msg)
#define LOG_CRITICAL(msg) if(g_logger) g_logger->log(CRITICAL, msg)

//Perf measurement macros
#define PERF_START(op) if(g_logger) g_logger->startTimer(op)
#define PERF_END(op) if(g_logger) g_logger->endTimer(op)
#define LOG_PERF(op, val, unit) if(g_logger) g_logger->logPerformance(op, val, unit)

#endif // Logger _H