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

	static Scheduler* getInstance();
	static void initialize();
	bool getSchedulerRunning() const;
	void setSchedulerRunning(bool val);
	void addToFrontOfProcessQueue(shared_ptr<Screen> process);
	shared_ptr<Screen> selectNextProcess();


	
	void addProcessToQueue(shared_ptr<Screen> screen);

	void start();
	void stop();
	void loadConfig();

	int getUsedCores() const;
	int getAvailableCores() const;
	int getIdleCpuTicks();

	int getCpuCycles() const;
	void setCpuCycles(int cpuCycles);

	void setAlgorithm(const string& algo);
	string getAlgorithm() const;


private:
	thread processGeneratorThread;
	int generationIntervalTicks = 5;

	int numCores;
	int coresUsed = 0;
	int coresAvailable;
	int cpuCycles = 0;
	int idleCpuTicks = 0;

	int quantumCycles = 1;
	int batchProcessFreq = 1;
	int minInstructions = 1;
	int maxInstructions = 1;
	int delayPerExec = 0;

	atomic<bool> schedulerRunning{ false };
	int activeThreads;
	vector<thread> workerThreads;
	queue<shared_ptr<Screen>> processQueue; 
	mutex processQueueMutex;
	condition_variable processQueueCondition;
	static Scheduler* scheduler;
	string algorithm = "";
	static mutex scheduler_init_mutex;

	atomic<bool> generatingProcesses = false;
	void generateDummyProcesses();
	int generatedProcessCount = 0;

};