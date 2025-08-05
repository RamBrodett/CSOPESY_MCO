#include "Scheduler.h"
#include "ScreenManager.h"
#include "CLIController.h"
#include "Instruction.h"
#include "MemoryManager.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <ctime>
#include <random>
#include <atomic>
#include <functional>

using namespace std;

// Singleton & Mutex
Scheduler* Scheduler::scheduler = nullptr;
std::mutex Scheduler::scheduler_init_mutex;

// Initializes scheduler state variables.
Scheduler::Scheduler()
    : numCores(0), coresUsed(0), coresAvailable(0),
    schedulerRunning(false), activeThreads(0) {
}

// Initializes the singleton instance of the Scheduler and loads its configuration.
void Scheduler::initialize() {
    if (scheduler == nullptr) {
        lock_guard<mutex> lock(scheduler_init_mutex);
        if (scheduler == nullptr) {
            scheduler = new Scheduler();
            scheduler->loadConfig();
        }
    }
}

// Returns the singleton instance of the Scheduler.
Scheduler* Scheduler::getInstance() {
    return scheduler;
}


void Scheduler::setAlgorithm(const string& algo) { algorithm = algo; }
string Scheduler::getAlgorithm() const { return algorithm; }

// Adds a process to the ready queue to be executed.
void Scheduler::addProcessToQueue(shared_ptr<Screen> screen) {
    lock_guard<mutex> lock(processQueueMutex);
    processQueue.push(screen);
    processQueueCondition.notify_one();
}

// Starts the scheduler's worker threads to begin processing the queue.
void Scheduler::start() {
    if (schedulerRunning) return;
    schedulerRunning.store(true);
    generatingProcesses.store(false);

    // Initialize the memory manager with configured values.
    MemoryManager::initialize(maxOverallMem,memPerFrame);
    // Create a pool of worker threads based on the number of CPU cores.
    workerThreads.clear();
    for (int i = 0; i < numCores; i++) {
        workerThreads.emplace_back([this, i]() {
            while (this->schedulerRunning) {
                shared_ptr<Screen> process;
                {
                    unique_lock<mutex> lock(this->processQueueMutex);
                    this->processQueueCondition.wait(lock, [this]() {
                        return !this->processQueue.empty() || !this->schedulerRunning;
                        });

                    if (!this->schedulerRunning) return;
                    if (this->processQueue.empty()) continue;

                    process = this->processQueue.front();
                    this->processQueue.pop();
                }

                if (process) {
                    // If the process has already finished (e.g., memory violation),
                    // just deallocate its resources and continue.
                    if (process->isFinished()) {
                        MemoryManager::getInstance()->deallocate(process->getName());
                        continue; // Skip to the next process
                    }

                    coresUsed++;
                    process->setCoreID(i);

                    // Execute for a quantum (RR) or to completion (FCFS).
                    process->execute(algorithm == "rr" ? quantumCycles : -1);

                    coresUsed--;

                    // If process is finished now, deallocate its memory. Otherwise, requeue it.
                    if (process->isFinished()) {
                        MemoryManager::getInstance()->deallocate(process->getName());
                    }
                    else {
                        // If not finished (must be RR), put it back in the queue.
                        addProcessToQueue(process);
                    }
                }
            }
            });
    }
}

// Starts the automatic generation of processes in a separate thread.
void Scheduler::startProcessGeneration() {
    if (getGeneratingProcesses()) return; // Already running
    setGeneratingProcesses(true);

    int initialBatchSize = numCores > 0 ? numCores : 1;
    for (int i = 0; i < initialBatchSize; ++i) {
        auto screenName = "p" + to_string(generatedProcessCount++);

       int memSize = getRandomPowerOf2(minMemPerProc, maxMemPerProc);

        // Generate instructions and create the screen object.
        auto instructions = generateInstructionsForProcess(screenName, memSize);
        auto screen = make_shared<Screen>(screenName, instructions, CLIController::getInstance()->getTimestamp());

        MemoryManager::getInstance()->setupProcessMemory(screenName, memSize);

        ScreenManager::getInstance()->registerScreen(screenName, screen);
        addProcessToQueue(screen);
    }

    if (!processGeneratorThread.joinable()) {
        processGeneratorThread = thread(&Scheduler::generateDummyProcesses, this);
    }
}

// Stops the scheduler and joins all worker/generator threads.
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
    cout << "Scheduler has finished joining all its threads." << endl;
}

// Generates a set of random instructions for a new process.
std::vector<Instruction> Scheduler::generateInstructionsForProcess(const std::string& screenName, int processMemorySize) {
    // Random number generators
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> instr_dist(minInstructions, maxInstructions);
    static std::uniform_int_distribution<> value_dist(1, 100);

    // Lambda to generate a single random instruction (excluding FOR)
    auto generateRandomInstruction = [&](const string& screenName) -> Instruction {
        uniform_int_distribution<> type_dist(0, 5);
        InstructionType type = static_cast<InstructionType>(type_dist(gen));

        int max_addr = (processMemorySize > 1) ? (processMemorySize - 1) : 0;
        uniform_int_distribution<> addr_dist(0, max_addr);

        switch (type) {
        case InstructionType::READ: {
            uint16_t address = addr_dist(gen);
            Instruction readInstr;
            readInstr.type = InstructionType::READ;
            readInstr.operands = { {true, "var_" + to_string(value_dist(gen) % 5), 0} };
            readInstr.memoryAddress = address;
            return readInstr;
        }
        case InstructionType::WRITE: {
            uint16_t address = addr_dist(gen);
            Instruction writeInstr;
            writeInstr.type = InstructionType::WRITE;
            writeInstr.operands = { {false, "", (uint16_t)value_dist(gen)} };
            writeInstr.memoryAddress = address;
            return writeInstr;
        }
        case InstructionType::ADD:
            return { InstructionType::ADD, {{true, "x", 0}, {true, "x", 0}, {false, "", (uint16_t)value_dist(gen)}} };
        case InstructionType::SUBTRACT:
            return { InstructionType::SUBTRACT, {{true, "x", 0}, {true, "x", 0}, {false, "", (uint16_t)(value_dist(gen) % 50)}} };
        case InstructionType::SLEEP:
            return { InstructionType::SLEEP, {{false, "", (uint16_t)(value_dist(gen) % 20 + 10)}} };
        case InstructionType::PRINT:
        default: // Fallback to PRINT if anything unexpected happens
            return { InstructionType::PRINT, {{true, "x", 0}}, "Value from " + screenName + ": %x%!" };
        }
        };

    vector<Instruction> instructions;
    int target_instruction_count = instr_dist(gen);

    // Add the initial DECLARE instruction. This counts as the first instruction.
    instructions.push_back({ InstructionType::DECLARE, {{true, "x", 0}, {false, "", (uint16_t)value_dist(gen)}}, "" });

    // Loop until the vector of instructions reaches the target size.
    while (instructions.size() < target_instruction_count) {
        // Decide whether to generate a FOR loop (20% chance after the first instruction).
        bool createForLoop = (instructions.size() > 1 && value_dist(gen) % 5 == 0);

        if (createForLoop) {
            // This simplified logic replaces the complex recursive lambda.
            int repeats = value_dist(gen) % 4 + 2;      // 2-5 repeats
            int innerCount = value_dist(gen) % 3 + 2;   // 2-4 inner instructions
            vector<Instruction> innerInstructions;

            for (int j = 0; j < innerCount; ++j) {
                // Generate simple instructions inside the FOR loop.
                innerInstructions.push_back(generateRandomInstruction("FOR"));
            }

            if (!innerInstructions.empty()) {
                instructions.push_back({ InstructionType::FOR, {{false, "", (uint16_t)repeats}}, "", innerInstructions });
            }
            else {
                // Fallback in case inner instructions list is empty for some reason.
                instructions.push_back(generateRandomInstruction(screenName));
            }
        }
        else {
            // Add a regular, simple instruction.
            instructions.push_back(generateRandomInstruction(screenName));
        }
    }

    return instructions;
}

// A loop that periodically creates new "dummy" processes for testing.
void Scheduler::generateDummyProcesses() {
    while (schedulerRunning.load()) {
        if (generatingProcesses.load()) {
            if (cpuCycles - lastGenCycle >= batchProcessFreq) {
                lastGenCycle = cpuCycles;

                auto screenName = "p" + to_string(generatedProcessCount++);

                int memSize = getRandomPowerOf2(minMemPerProc, maxMemPerProc);

                auto instructions = generateInstructionsForProcess(screenName, memSize);
                auto screen = make_shared<Screen>(screenName, instructions, CLIController::getInstance()->getTimestamp());

                MemoryManager::getInstance()->setupProcessMemory(screenName, memSize);

                ScreenManager::getInstance()->registerScreen(screenName, screen);
                addProcessToQueue(screen);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Loads and parses configuration parameters from the "config.txt" file.
void Scheduler::loadConfig() {
    ifstream config("config.txt");
    // Error checker
    if (!config) {
        cerr << "Error: config.txt not found. Using default values." << endl;
        numCores = 2;
        algorithm = "rr";
        quantumCycles = 4;
        batchProcessFreq = 1;
        minInstructions = 100;
        maxInstructions = 100;
        delayPerExec = 1;
        coresAvailable = numCores;
		maxOverallMem = 16384; // Default 16MB
		memPerFrame = 16; // Default 16KB
        minMemPerProc = 64;    // Default 64 bytes
        maxMemPerProc = 65536; // Default 65536 bytes (64 KB)
        return;
    }

    string key, value;

    while (config >> key >> std::ws) {
        getline(config, value);
        if (!value.empty() && value.front() == '"' && value.back() == '"')
            value = value.substr(1, value.size() - 2);

        if (key == "num-cpu") {
            numCores = stoi(value);
            if (numCores < 1) numCores = 1;
            if (numCores > 128) numCores = 128;
        }
        else if (key == "scheduler") {
            if (value == "fcfs" || value == "rr") {
                algorithm = value;
            }
            else {
                algorithm = "fcfs"; // Default value
            }
        }
        else if (key == "quantum-cycles") {
            quantumCycles = stoi(value);
            if (quantumCycles < 1) quantumCycles = 1;
            if (quantumCycles > std::numeric_limits<int>::max()) quantumCycles = std::numeric_limits<int>::max();
        }
        else if (key == "batch-process-freq") {
            batchProcessFreq = stoi(value);
            if (batchProcessFreq < 1) batchProcessFreq = 1;
            if (batchProcessFreq > std::numeric_limits<int>::max()) batchProcessFreq = std::numeric_limits<int>::max();
        }
        else if (key == "min-ins") {
            minInstructions = stoi(value);
            if (minInstructions < 1) minInstructions = 1;
            if (minInstructions > std::numeric_limits<int>::max()) minInstructions = std::numeric_limits<int>::max();
        }
        else if (key == "max-ins") {
            maxInstructions = stoi(value);
            if (maxInstructions < 1) maxInstructions = 1;
            if (maxInstructions > std::numeric_limits<int>::max()) maxInstructions = std::numeric_limits<int>::max();
        }
        else if (key == "delays-per-exec") {
            delayPerExec = stoi(value);
            if (delayPerExec < 0) delayPerExec = 0;
        }
        else if (key == "max-overall-mem") {
            maxOverallMem = stoi(value);
			if (maxOverallMem < 1) maxOverallMem = 1;
        }
        else if (key == "mem-per-frame") {
            memPerFrame = stoi(value);
			if (memPerFrame < 1) memPerFrame = 1;
        }
		else if (key == "min-mem-per-proc") {
            minMemPerProc = stoi(value);
            if (minMemPerProc < 64) minMemPerProc = 64;
        }
        else if (key == "max-mem-per-proc") {
            maxMemPerProc = stoi(value);
            if (maxMemPerProc > 65536) maxMemPerProc = 65536;
        }
    }
    // Assign cores available
    coresAvailable = numCores;
}

int Scheduler::getUsedCores() const {
    return coresUsed.load(); 
}
int Scheduler::getAvailableCores() const { return coresAvailable; }
int Scheduler::getCpuCycles() const {
    
    return cpuCycles.load();
}
void Scheduler::setCpuCycles(int cycles) {
    
    cpuCycles.store(cycles);
}
int Scheduler::getDelayPerExec() const { return delayPerExec; }
bool Scheduler::getSchedulerRunning() const { return schedulerRunning.load(); }
void Scheduler::setSchedulerRunning(bool val) { schedulerRunning.store(val); }
void Scheduler::setGeneratingProcesses(bool shouldGenerate) {
    generatingProcesses.store(shouldGenerate);
}
bool Scheduler::getGeneratingProcesses() {
	return generatingProcesses.load();
}
void Scheduler::incrementCpuCycles() {
    cpuCycles.fetch_add(1); 
}
int Scheduler::getQuantumCycles() const {
    return quantumCycles;
}

// helper function to get a random power of 2 within a range (used primarily for min/max mem proc)
int Scheduler::getRandomPowerOf2(int minVal, int maxVal) {
    std::vector<int> powers;
    for (int v = minVal; v <= maxVal; v <<= 1) {
        if (v > 0) { // Safety check to avoid infinite loops if minVal is 0 or negative
            powers.push_back(v);
        }
    }
    if (powers.empty()) return minVal;

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, static_cast<int>(powers.size() - 1));

    return powers[dist(gen)];
}


void Scheduler::incrementIdleCpuTicks() {
    idleCpuTicks.fetch_add(1);
}

int Scheduler::getIdleCpuTicks() const {
    return idleCpuTicks.load();
}

size_t Scheduler::getProcessQueueSize() const {
    std::lock_guard<std::mutex> lock(processQueueMutex);
    return processQueue.size();
}