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

ScreenManager* ScreenManager::instance = nullptr;

ScreenManager::ScreenManager() {}

void ScreenManager::initialize() {
    if (!instance) instance = new ScreenManager();
}

ScreenManager* ScreenManager::getInstance() {
    return instance;
}

void ScreenManager::destroy() {
    delete instance;
    instance = nullptr;
}

void ScreenManager::registerScreen(const string& name, shared_ptr<Screen> screen) {
    screens[name] = screen;
}

shared_ptr<Screen> ScreenManager::getScreen(const string& name) {
    return screens.count(name) ? screens[name] : nullptr;
}

unordered_map<string, shared_ptr<Screen>> ScreenManager::getAllScreens() const {
    return screens;
}

void ScreenManager::switchScreen(const string& name) {
    if (screens.count(name)) currentScreen = screens[name];
}

shared_ptr<Screen> ScreenManager::getCurrentScreen() {
    return currentScreen;
}

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

void ScreenManager::displaySystemSmiSummary() {
    // This is the implementation for the SYSTEM-WIDE SMI command from the main menu
    MemoryManager* memManager = MemoryManager::getInstance();
    Scheduler* scheduler = Scheduler::getInstance();

    if (!memManager || !scheduler) {
        cout << "System is not fully initialized. Please use the 'initialize' command." << endl;
        return;
    }

    // --- Fetch stats from MemoryManager and Scheduler ---
    // NOTE: You will need to add these getters to your MemoryManager
    int totalMem = memManager->getTotalMemory(); // Assumes a getter for total memory in bytes
    int usedMem = memManager->getUsedMemory();   // Assumes a getter for used memory in bytes
    float memUtilization = (totalMem > 0) ? (static_cast<float>(usedMem) / totalMem) * 100.0f : 0.0f;

    // Fetches core usage directly from the Scheduler
    int usedCores = scheduler->getUsedCores();
    int availableCores = scheduler->getAvailableCores();

    // --- Display formatted output ---
    cout << "PROCESS-SMI V1.00 Driver Version: 0.001" << endl;
    cout << "========================================================" << endl;
    cout << "CPU Utilization: " << usedCores << " / " << availableCores << " Cores" << endl;
    cout << "Memory Usage: " << usedMem / 1024 << "KB / " << totalMem / 1024 << "KB" << endl;
    cout << "Memory Util: " << fixed << setprecision(2) << memUtilization << "%" << endl;
    cout << "--------------------------------------------------------" << endl;
    cout << "Running processes and memory usage:" << endl;

    auto allScreens = getAllScreens();
    int runningProcessCount = 0;
    for (const auto& pair : allScreens) {
        if (pair.first != "main" && !pair.second->isFinished()) {
            runningProcessCount++;
            // NOTE: You will need a getter in MemoryManager for this
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

void ScreenManager::displayVmStat() {
    MemoryManager* memManager = MemoryManager::getInstance();
    Scheduler* scheduler = Scheduler::getInstance();

    if (!memManager || !scheduler) {
        cout << "System is not fully initialized. Please use the 'initialize' command." << endl;
        return;
    }

    // --- Fetch stats from MemoryManager and Scheduler ---
    // NOTE: You will need to add these getters to your MemoryManager
    int totalMem = memManager->getTotalMemory();
    int usedMem = memManager->getUsedMemory();
    int freeMem = totalMem - usedMem;
    int pagedIn = memManager->getPagedInCount();
    int pagedOut = memManager->getPagedOutCount();

    // Fetch tick counts from the Scheduler
    int totalTicks = scheduler->getCpuCycles();
    int idleTicks = scheduler->getIdleCpuTicks(); // Correctly use the new getter
    int activeTicks = totalTicks - idleTicks;

    // --- Display formatted output ---
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