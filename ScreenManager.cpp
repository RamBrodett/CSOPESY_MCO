#include "ScreenManager.h"
#include "CLIController.h"
#include "MemoryManager.h"
#include "Scheduler.h"
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip> 

using namespace std;

// Singleton
ScreenManager* ScreenManager::instance = nullptr;
// Constructor (private for singleton pattern).
ScreenManager::ScreenManager() {}

// Initializes the singleton instance and creates the main menu screen.
void ScreenManager::initialize() {
    if (!instance) instance = new ScreenManager();
}

ScreenManager* ScreenManager::getInstance() {
    return instance;
}

// Destroys the singleton instance to free memory.
void ScreenManager::destroy() {
    delete instance;
    instance = nullptr;
}

// Registers a new screen (process) with the manager.
void ScreenManager::registerScreen(const string& name, shared_ptr<Screen> screen) {
    screens[name] = screen;
}

// Returns a pointer to a screen by its name.
shared_ptr<Screen> ScreenManager::getScreen(const string& name) {
    return screens.count(name) ? screens[name] : nullptr;
}

// Returns a map of all registered screens.
unordered_map<string, shared_ptr<Screen>> ScreenManager::getAllScreens() const {
    return screens;
}

// Switches the user's current view to the specified screen.
void ScreenManager::switchScreen(const string& name) {
    if (screens.count(name)) currentScreen = screens[name];
}

// Returns a pointer to the currently active screen.
shared_ptr<Screen> ScreenManager::getCurrentScreen() {
    return currentScreen;
}

// Checks if a screen with the given name exists.
bool ScreenManager::hasScreen(const string& name) const {
    return screens.find(name) != screens.end();
}

void ScreenManager::displayProcessSMI() {
    shared_ptr<Screen> screenPtr = getCurrentScreen();
    if (!screenPtr || screenPtr->getName() == "main") {
        cout << "No process screen selected." << endl;
        return;
    }

    Screen& screen = *screenPtr;
    bool isFinished = screen.isFinished();

    cout << "Process name: " << screen.getName() << endl;
    cout << "Logs:" << endl;

    // Use the getter to access logs safely
    for (const auto& line : screen.getOutputBuffer()) {
        cout << line << endl;
    }

    cout << endl;
    if (isFinished) {
        cout << endl << "Finished!" << endl;
    }
    else {
        cout << "Current instruction line: " << screen.getProgramCounter() << endl;
        cout << "Lines of code: " << screen.getTotalInstructions() << endl;
    }

    
}

// Displays the high-level system summary (for the 'process-smi' command).
void ScreenManager::displaySystemSmiSummary() {
    MemoryManager* memManager = MemoryManager::getInstance();
    Scheduler* scheduler = Scheduler::getInstance();

    if (!memManager || !scheduler) {
        cout << "System is not fully initialized. Please use the 'initialize' command." << endl;
        return;
    }

    
    int totalMem = memManager->getTotalMemory(); 
    int usedMem = memManager->getUsedMemory();  
    float memUtilization = (totalMem > 0) ? (static_cast<float>(usedMem) / totalMem) * 100.0f : 0.0f;

   
    int usedCores = scheduler->getUsedCores();
    int availableCores = scheduler->getAvailableCores();

    
    cout << "PROCESS-SMI V1.00 Driver Version: 0.001" << endl;
    cout << "========================================================" << endl;
    cout << "CPU Utilization: " << usedCores << " / " << availableCores << " Cores" << endl;
    cout << "Memory Usage: " << usedMem << "B / " << totalMem << "B" << endl;
    cout << "Memory Util: " << fixed << setprecision(2) << memUtilization << "%" << endl;
    cout << "--------------------------------------------------------" << endl;
    cout << "Running processes and memory usage:" << endl;

    auto allScreens = getAllScreens();
    int runningProcessCount = 0;
    for (const auto& pair : allScreens) {
        if (pair.first != "main" && !pair.second->isFinished()) {
            runningProcessCount++;
            
            int processMem = memManager->getProcessMemoryUsage(pair.first);
            cout << "  - Process: " << left << setw(15) << pair.first
                << "Memory: " << processMem << " bytes" << endl;
        }
    }

    if (runningProcessCount == 0) {
        cout << "  No running processes." << endl;
    }
    cout << "========================================================" << endl;
}

// Displays detailed virtual memory statistics (for the 'vmstat' command).
void ScreenManager::displayVmStat() {
    MemoryManager* memManager = MemoryManager::getInstance();
    Scheduler* scheduler = Scheduler::getInstance();

    if (!memManager || !scheduler) {
        cout << "System is not fully initialized. Please use the 'initialize' command." << endl;
        return;
    }

    
    int totalMem = memManager->getTotalMemory();
    int usedMem = memManager->getUsedMemory();
    int freeMem = totalMem - usedMem;
    int pagedIn = memManager->getPagedInCount();
    int pagedOut = memManager->getPagedOutCount();

    
    int totalTicks = scheduler->getCpuCycles();
    int idleTicks = scheduler->getIdleCpuTicks(); 
    int activeTicks = totalTicks - idleTicks;

    // Display formatted output
    cout << "--------------------- VM STATS ---------------------" << endl;
    cout << " memory" << endl;
    cout << left << setw(25) << " total:" << totalMem << " B" << endl;
    cout << left << setw(25) << " used:" << usedMem << " B" << endl;
    cout << left << setw(25) << " free:" << freeMem << " B" << endl;
    cout << "----------------------------------------------------" << endl;
    cout << " cpu ticks" << endl;
    cout << left << setw(25) << " total:" << totalTicks << endl;
    cout << left << setw(25) << " active:" << activeTicks << endl;
    cout << left << setw(25) << " idle:" << idleTicks << endl;
    cout << "----------------------------------------------------" << endl;
    cout << " paging" << endl;
    cout << left << setw(25) << " paged in:" << pagedIn << endl;
    cout << left << setw(25) << " paged out:" << pagedOut << endl;
    cout << "----------------------------------------------------" << endl;
}