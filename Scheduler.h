// Corrected Scheduler.h
#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <memory>
#include <atomic>
#include "Screen.h"
#include <vector>

class Scheduler {
public:
    Scheduler();
    static Scheduler* getInstance();
    static void initialize();

    void addProcessToQueue(std::shared_ptr<Screen> screen);
    void start();
    void stop();
    void loadConfig();

    // --- Status & Metrics Getters ---
    bool getSchedulerRunning() const;
    int getUsedCores() const;
    int getAvailableCores() const;
    long long getCpuCycles() const;
    int getDelayPerExec() const;
    std::string getAlgorithm() const;
    int getQuantumCycles() const;
    int getPageSize() const;
    long long getIdleCpuTicks() const;
    long long getActiveCpuTicks() const;
    bool getGeneratingProcesses();

    // --- Setters ---
    void setSchedulerRunning(bool val);
    void setGeneratingProcesses(bool shouldGenerate);

    // --- Public Utilities ---
    std::vector<Instruction> generateInstructionsForProcess(const std::string& screenName);
    void startProcessGeneration();
    void incrementCpuCycles();
    void incrementIdleCpuTicks(); // Added this

private:
    // --- Config ---
    int numCores = 0;
    int quantumCycles = 1;
    int batchProcessFreq = 1;
    int minInstructions = 1;
    int maxInstructions = 100;
    int delayPerExec = 0;
    int maxOverallMem = 16384;
    int memPerFrame = 256;
    int memPerProc = 1024;
    std::string algorithm = "rr";

    // --- Metrics ---
    std::atomic<int> coresUsed{ 0 };
    int coresAvailable = 0;
    std::atomic<long long> cpuCycles{ 0 };
    std::atomic<long long> idleCpuTicks{ 0 };
    std::atomic<long long> activeCpuTicks{ 0 };

    // --- Process Generation ---
    std::thread processGeneratorThread;
    int lastGenCycle = 0;
    int generatedProcessCount = 0;
    std::atomic<bool> generatingProcesses{ false };

    // --- Scheduler State ---
    std::atomic<bool> schedulerRunning{ false };
    std::vector<std::thread> workerThreads;
    std::queue<std::shared_ptr<Screen>> processQueue;
    std::mutex processQueueMutex;
    std::condition_variable processQueueCondition;

    // --- Singleton ---
    static Scheduler* scheduler;
    static std::mutex scheduler_init_mutex;
    void generateDummyProcesses();
};