#include "Scheduler.h"
#include "ScreenManager.h"
#include "CLIController.h"
#include "Instruction.h"
#include "MemoryManager.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <random>
#include <functional>

Scheduler* Scheduler::scheduler = nullptr;
std::mutex Scheduler::scheduler_init_mutex;

Scheduler::Scheduler() {
    // Member variables are initialized in the header file using modern C++ standards.
}

void Scheduler::initialize() {
    if (scheduler == nullptr) {
        std::lock_guard<std::mutex> lock(scheduler_init_mutex);
        if (scheduler == nullptr) {
            scheduler = new Scheduler();
            scheduler->loadConfig();
        }
    }
}

Scheduler* Scheduler::getInstance() {
    return scheduler;
}

void Scheduler::addProcessToQueue(std::shared_ptr<Screen> screen) {
    std::lock_guard<std::mutex> lock(processQueueMutex);
    processQueue.push(screen);
    processQueueCondition.notify_one();
}

void Scheduler::start() {
    if (schedulerRunning) return;
    schedulerRunning.store(true);
    generatingProcesses.store(false);

    MemoryManager::initialize(maxOverallMem, memPerFrame);
    workerThreads.clear();

    for (int i = 0; i < numCores; i++) {
        workerThreads.emplace_back([this, i]() {
            while (this->schedulerRunning) {
                std::shared_ptr<Screen> process;
                {
                    std::unique_lock<std::mutex> lock(this->processQueueMutex);
                    this->processQueueCondition.wait(lock, [this]() {
                        return !this->processQueue.empty() || !this->schedulerRunning;
                        });

                    if (!this->schedulerRunning || this->processQueue.empty()) {
                        continue;
                    }

                    process = this->processQueue.front();
                    this->processQueue.pop();
                }

                if (process) {
                    coresUsed++;
                    process->setCoreID(i);
                    activeCpuTicks.fetch_add(1); // Increment for active cycle

                    process->execute(algorithm == "rr" ? quantumCycles : -1);

                    coresUsed--;

                    if (process->isFinished()) {
                        MemoryManager::getInstance()->deallocateProcess(process->getName());
                    }
                    else {
                        addProcessToQueue(process);
                    }
                }
            }
            });
    }
}

void Scheduler::stop() {
    generatingProcesses.store(false);
    schedulerRunning.store(false);
    processQueueCondition.notify_all();

    if (processGeneratorThread.joinable()) {
        processGeneratorThread.join();
    }

    for (auto& t : workerThreads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void Scheduler::startProcessGeneration() {
    if (getGeneratingProcesses()) return;
    setGeneratingProcesses(true);
    if (!processGeneratorThread.joinable()) {
        processGeneratorThread = std::thread(&Scheduler::generateDummyProcesses, this);
    }
}

void Scheduler::generateDummyProcesses() {
    while (schedulerRunning.load()) {
        if (generatingProcesses.load()) {
            if (cpuCycles.load() - lastGenCycle >= batchProcessFreq) {
                lastGenCycle = cpuCycles.load();

                auto screenName = "p" + std::to_string(generatedProcessCount++);
                auto instructions = generateInstructionsForProcess(screenName);

                // Correctly create process with virtual memory
                int memSize = memPerProc;
                MemoryManager::getInstance()->createProcessPageTable(screenName, memSize);
                auto screen = std::make_shared<Screen>(screenName, memSize, instructions, CLIController::getInstance()->getTimestamp());

                ScreenManager::getInstance()->registerScreen(screenName, screen);
                addProcessToQueue(screen);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Prevent busy-waiting
    }
}

std::vector<Instruction> Scheduler::generateInstructionsForProcess(const std::string& screenName) {
    // This is your existing random instruction generation logic, which is fine.
    // For a simple placeholder:
    std::vector<Instruction> instructions;
    instructions.push_back({ InstructionType::PRINT, {}, "Hello from placeholder process " + screenName });
    return instructions;
}

void Scheduler::loadConfig() {
    std::ifstream config("config.txt");
    if (!config) {
        std::cerr << "Error: config.txt not found. Using default values." << std::endl;
        numCores = 2;
        algorithm = "rr";
        quantumCycles = 4;
        batchProcessFreq = 1;
        minInstructions = 5;
        maxInstructions = 20;
        delayPerExec = 0;
        maxOverallMem = 16384;
        memPerFrame = 256;
        memPerProc = 1024;
        coresAvailable = numCores;
        return;
    }

    std::string key, value;
    while (config >> key >> std::ws && getline(config, value)) {
        if (key == "num-cpu") numCores = std::stoi(value);
        else if (key == "scheduler") algorithm = value;
        else if (key == "quantum-cycles") quantumCycles = std::stoi(value);
        else if (key == "batch-process-freq") batchProcessFreq = std::stoi(value);
        else if (key == "min-ins") minInstructions = std::stoi(value);
        else if (key == "max-ins") maxInstructions = std::stoi(value);
        else if (key == "delays-per-exec") delayPerExec = std::stoi(value);
        else if (key == "max-overall-mem") maxOverallMem = std::stoi(value);
        else if (key == "page-size") memPerFrame = std::stoi(value); // Changed to page-size
        else if (key == "mem-per-proc") memPerProc = std::stoi(value);
    }
    coresAvailable = numCores;
}

// --- Getter and Setter Implementations ---

bool Scheduler::getSchedulerRunning() const { return schedulerRunning.load(); }
void Scheduler::setSchedulerRunning(bool val) { schedulerRunning.store(val); }

int Scheduler::getUsedCores() const { return coresUsed.load(); }
int Scheduler::getAvailableCores() const { return coresAvailable; }

long long Scheduler::getCpuCycles() const { return cpuCycles.load(); }
void Scheduler::incrementCpuCycles() { cpuCycles.fetch_add(1); }

int Scheduler::getDelayPerExec() const { return delayPerExec; }
std::string Scheduler::getAlgorithm() const { return algorithm; }
int Scheduler::getQuantumCycles() const { return quantumCycles; }
int Scheduler::getPageSize() const { return memPerFrame; }

bool Scheduler::getGeneratingProcesses() { return generatingProcesses.load(); }
void Scheduler::setGeneratingProcesses(bool shouldGenerate) { generatingProcesses.store(shouldGenerate); }

// --- Functions for vmstat ---

void Scheduler::incrementIdleCpuTicks() { idleCpuTicks.fetch_add(1); }
long long Scheduler::getIdleCpuTicks() const { return idleCpuTicks.load(); }
long long Scheduler::getActiveCpuTicks() const { return activeCpuTicks.load(); }