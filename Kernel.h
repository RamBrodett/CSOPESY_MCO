#pragma once
#include <thread>
using namespace std;

class Kernel {
public:
    // --- Singleton Access and Lifecycle ---
    static void initialize();
    static Kernel* getInstance();
    static void destroy(); 
    // --- Application State Management ---
    bool getRunningStatus() const;
    void setRunningStatus(bool newStatus);
    thread& getSchedulerThread();
    // --- Configuration State ---
    void setConfigInitialized(bool status);
    bool isConfigInitialized() const;
	~Kernel(); // Destructor to clean up resources

private:
    Kernel();
    static Kernel* instance;
    bool isRunning = true;
    thread schedulerThread;
	bool configInitialized = false; //tracker for initialize command
};
