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
using namespace std;


class Scheduler {
public:

	enum STATES {
		READY,
		WAITING,
		RUNNING,
		FINISHED
	};

	Scheduler();

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
	int getIdleCpuTicks();
	int getCpuCycles() const;
	void setCpuCycles(int cpuCycles);
	int getDelayPerExec() const;

	// --- Algorithm Configuration ---
	void setAlgorithm(const string& algo);
	string getAlgorithm() const;



private:

	// --- Config ---
	int numCores;
	int quantumCycles = 1;
	int batchProcessFreq = 1;
	int minInstructions = 1;
	int maxInstructions = 1;
	int delayPerExec = 0;

	// --- Metrics ---
	int coresUsed = 0;
	int coresAvailable;
	int cpuCycles = 0;
	int idleCpuTicks = 0;

	// --- Process Generation ---
	thread processGeneratorThread;
	int generationIntervalTicks = 5;
	int lastGenCycle = 0;
	int generatedProcessCount = 0;

	// --- Scheduler State ---
	atomic<bool> schedulerRunning{ false };
	int activeThreads;
	vector<thread> workerThreads;
	queue<shared_ptr<Screen>> processQueue; 
	mutex processQueueMutex;
	condition_variable processQueueCondition;

	// --- Singleton ---
	static Scheduler* scheduler;
	string algorithm = "";
	static mutex scheduler_init_mutex;

	atomic<bool> generatingProcesses = false;
	void generateDummyProcesses();


};