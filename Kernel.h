#pragma once
#include <thread>
using namespace std;

class Kernel {
public:
    static void initialize();
    static Kernel* getInstance();
    static void destroy();

    bool getRunningStatus() const;
    void setRunningStatus(bool newStatus);
    thread& getSchedulerThread();

    void setConfigInitialized(bool status);
    bool isConfigInitialized() const;



private:
    Kernel();
    static Kernel* instance;
    bool isRunning = true;
    thread schedulerThread;
	bool configInitialized = false;
};
