#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <memory>
#include <atomic>
#include "MemoryManager.h"
#include "Screen.h"
#include <vector>
using namespace std;


class Scheduler {
public:

	// --- Singleton Access ---
	static Scheduler* getInstance();
	static void initialize();

	// --- Process Queue Management ---
	void addProcessToQueue(shared_ptr<Screen> screen);

	// --- Scheduler ---
	void start();
	void stop();
	void loadConfig();
	bool getSchedulerRunning() const;
	void setSchedulerRunning(bool val);

	// --- CPU and Core Status ---
	int getUsedCores() const;
	int getAvailableCores() const;
	int getCpuCycles() const;
	void setCpuCycles(int cpuCycles);
	int getDelayPerExec() const;

	// --- Algorithm Configuration ---
	void setAlgorithm(const string& algo);
	string getAlgorithm() const;

	// --- Memory Config ---
	int getRandomPowerOf2(int minVal, int maxVal);

	void setGeneratingProcesses(bool shouldGenerate);
	bool getGeneratingProcesses();

	std::vector<Instruction> generateInstructionsForProcess(const std::string& screenName, int processMemorySize);
	void startProcessGeneration();
	void incrementCpuCycles();
	int getQuantumCycles() const;

	void incrementIdleCpuTicks();
	int getIdleCpuTicks() const;
	size_t getProcessQueueSize() const;

private:
	Scheduler();

	// --- State & Config ---
	int numCores;
	int quantumCycles = 1;
	int batchProcessFreq = 1;
	int minInstructions = 1;
	int maxInstructions = 1;
	int delayPerExec = 0;
	int maxOverallMem = 16384;
	int memPerFrame = 16;
	int memPerProc = 4096;
	int minMemPerProc = 64;
	int maxMemPerProc = 65536;
	atomic<bool> schedulerRunning{ false };

	// --- Metrics ---
	std::atomic<int> coresUsed = 0;
	int coresAvailable;
	std::atomic<int> cpuCycles = 0;

	// --- Process Generation ---
	thread processGeneratorThread;
	int generationIntervalTicks = 5;
	int lastGenCycle = 0;
	int generatedProcessCount = 0;
	

	// --- Singleton ---
	static Scheduler* scheduler;
	string algorithm = "";
	static mutex scheduler_init_mutex;
	atomic<bool> generatingProcesses{ false };
	void generateDummyProcesses();

	// --- Queues & Threads ---
	std::atomic<int> idleCpuTicks{ 0 };
	std::queue<std::shared_ptr<Screen>> processQueue;
	mutable std::mutex processQueueMutex;
	condition_variable processQueueCondition;
	int activeThreads;
	vector<thread> workerThreads;
};