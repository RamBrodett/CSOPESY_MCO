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

Scheduler* Scheduler::scheduler = nullptr;
std::mutex Scheduler::scheduler_init_mutex;

Scheduler::Scheduler()
    : numCores(0), coresUsed(0), coresAvailable(0),
    schedulerRunning(false), activeThreads(0) {
}


void Scheduler::initialize() {
    if (scheduler == nullptr) {
        lock_guard<mutex> lock(scheduler_init_mutex);
        if (scheduler == nullptr) {
            scheduler = new Scheduler();
            scheduler->loadConfig();
        }
    }
}

Scheduler* Scheduler::getInstance() {
    return scheduler;
}

void Scheduler::setAlgorithm(const string& algo) { algorithm = algo; }
string Scheduler::getAlgorithm() const { return algorithm; }

void Scheduler::addProcessToQueue(shared_ptr<Screen> screen) {
    lock_guard<mutex> lock(processQueueMutex);
    processQueue.push(screen);
    //cout << "DEBUG: Process '" << screen->getName() << "' added to queue. Queue size is now: "
    //    << processQueue.size() << endl;

    processQueueCondition.notify_one();
}

void Scheduler::start() {
    if (schedulerRunning) return;
    schedulerRunning.store(true);
    generatingProcesses.store(false);

    MemoryManager::initialize(maxOverallMem);
    workerThreads.clear();

    // The process generator thread will start, but will be idle
    // until generatingProcesses is set to true.
    //processGeneratorThread = thread(&Scheduler::generateDummyProcesses, this);


    for (int i = 0; i < numCores; i++) {
        workerThreads.emplace_back([this, i]() {
            while (this->schedulerRunning) {
                shared_ptr<Screen> process;
                {
                    unique_lock<mutex> lock(this->processQueueMutex);
                    this->processQueueCondition.wait(lock, [this]() {
                        return !this->processQueue.empty() || !this->schedulerRunning;
                        });
                    //cout << "DEBUG: Worker thread " << i << " AWAKE. Checking queue..." << endl;

                    if (!this->schedulerRunning) return;
                    if (this->processQueue.empty()) continue;

                    process = this->processQueue.front();
                    this->processQueue.pop();
                }

                if (process) {
                    // Check if the process needs memory allocated.
                    bool isInMemory = MemoryManager::getInstance()->isAllocated(process->getName());

                    if (!isInMemory) {
                        // If not in memory, try to allocate it.
                        if (!MemoryManager::getInstance()->allocate(process->getName(), memPerProc)) {
                            // Failed to get memory. Put it at the back of the queue and wait.
                            addProcessToQueue(process);
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                            continue; // Let this thread try another process.
                        }
                    }

                    // --- If we reach here, the process is guaranteed to be in memory ---

                    // A core is now being used to execute it.
                    coresUsed++;
                    process->setCoreID(i);

                    // Execute for a quantum (RR) or to completion (FCFS).
                    process->execute(algorithm == "rr" ? quantumCycles : -1);

                    // This core is now free for another task.
                    coresUsed--;

                    // --- Post-execution clean-up ---
                    if (process->isFinished()) {
                        // If finished, free its memory.
                        MemoryManager::getInstance()->deallocate(process->getName());
                    }
                    else {
                        // If not finished (must be RR), put it back in the queue for its next turn.
                        addProcessToQueue(process);
                    }
                }
            }
        });
    }
}

void Scheduler::startProcessGeneration() {
    if (getGeneratingProcesses()) return; // Already running
    setGeneratingProcesses(true);

    // --- FIX: Add this block ---
    // Generate an initial batch of processes immediately to kickstart the cycle.
    // This gives the worker threads something to do and breaks the deadlock.
    //cout << "Generating initial batch of processes..." << endl;
    int initialBatchSize = numCores > 0 ? numCores : 1; // Ensure at least 1 process
    for (int i = 0; i < initialBatchSize; ++i) {
        auto screenName = "p" + to_string(generatedProcessCount++);
        auto instructions = generateInstructionsForProcess(screenName);
        auto screen = make_shared<Screen>(screenName, instructions, CLIController::getInstance()->getTimestamp());
        ScreenManager::getInstance()->registerScreen(screenName, screen);
        addProcessToQueue(screen); // Add directly to the queue
    }
    // --- End of fix ---

    // Start the thread for continuous, ongoing generation.
    if (!processGeneratorThread.joinable()) {
        processGeneratorThread = thread(&Scheduler::generateDummyProcesses, this);
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
    cout << "Scheduler has finished joining all its threads." << endl;
}

std::vector<Instruction> Scheduler::generateInstructionsForProcess(const std::string& screenName) {
    // --- Random number generators ---
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> instr_dist(minInstructions, maxInstructions);
    uniform_int_distribution<> value_dist(1, 100);
    uniform_int_distribution<> type_dist(0, 4);

    // --- Lambda to generate a single random instruction (excluding FOR) ---
    auto generateRandomInstruction = [&](const string& screenName) -> Instruction {
        uniform_int_distribution<> type_dist(0, 6); // Include READ/WRITE
        InstructionType type = static_cast<InstructionType>(type_dist(gen));
        switch (type) {
        case InstructionType::READ: {
            // Generate memory address - some will be invalid to trigger violations
            uniform_int_distribution<> addr_dist(0x800, 0x3000); // Range that may exceed process bounds
            uint16_t address = addr_dist(gen);

            Instruction readInstr;
            readInstr.type = InstructionType::READ;
            readInstr.operands = { {true, "var_" + to_string(value_dist(gen) % 10), 0} };
            readInstr.memoryAddress = address;
            return readInstr;
        }
        case InstructionType::WRITE: {
            // Generate memory address - some will be invalid to trigger violations
            uniform_int_distribution<> addr_dist(0x800, 0x3000); // Range that may exceed process bounds
            uint16_t address = addr_dist(gen);

            Instruction writeInstr;
            writeInstr.type = InstructionType::WRITE;
            writeInstr.operands = { {false, "", (uint16_t)value_dist(gen)} };
            writeInstr.memoryAddress = address;
            return writeInstr;
        }
        case InstructionType::ADD:
            return { InstructionType::ADD, {{true, "x", 0}, {true, "x", 0}, {false, "", (uint16_t)value_dist(gen)}}, "" };
        case InstructionType::SUBTRACT:
            return { InstructionType::SUBTRACT, {{true, "x", 0}, {true, "x", 0}, {false, "", (uint16_t)(value_dist(gen) % 50)}}, "" };
        case InstructionType::PRINT:
            return { InstructionType::PRINT, {{true, "x", 0}}, "Value from " + screenName + ": %x%!" };
        case InstructionType::SLEEP:
            return { InstructionType::SLEEP, {{false, "", (uint16_t)(value_dist(gen) % 20 + 10)}}, "" };
        default:
            return { InstructionType::PRINT, {}, "Hello world from " + screenName + "!" };
        }
        };

    // --- Lambda to generate a FOR loop with nested instructions (max 3 levels) ---
    function<void(int, vector<Instruction>&, int&, int)> generateForInstruction;
    generateForInstruction = [&](int nestLevel, vector<Instruction>& instructions, int& currentCount, int maxCount) {
        if (currentCount >= maxCount || nestLevel > 3) return;

        if (currentCount + 2 > maxCount) return;

        int repeats = value_dist(gen) % 4 + 2; // 2-5 repeats
        int innerCount = value_dist(gen) % 3 + 2; // 2-4 inner instructions
        vector<Instruction> innerInstructions;
        int tempInstructionCount = 0;

        for (int j = 0; j < innerCount && (currentCount + tempInstructionCount < maxCount); ++j) {
            if (nestLevel < 3 && value_dist(gen) % 4 == 0) {
                generateForInstruction(nestLevel + 1, innerInstructions, tempInstructionCount, innerCount);
            }
            else {
                innerInstructions.push_back(generateRandomInstruction("FOR"));
                tempInstructionCount++;
            }
        }

        if (!innerInstructions.empty()) {
            Instruction forInstr = { InstructionType::FOR, {{false, "", (uint16_t)repeats}}, "", innerInstructions };
            instructions.push_back(forInstr);
            currentCount += tempInstructionCount + 1;
        }
        };

    // --- Main logic to generate the full set of instructions ---
    vector<Instruction> instructions;
    int num_instructions = instr_dist(gen);
    int currentCount = 0;
    instructions.push_back({ InstructionType::DECLARE, {{true, "x", 0}, {false, "", (uint16_t)value_dist(gen)}}, "" });
    currentCount++;

    for (int i = 0; i < num_instructions && currentCount < maxInstructions; ++i) {
        if (i > 0 && value_dist(gen) % 5 == 0) {
            generateForInstruction(1, instructions, currentCount, maxInstructions);
        }
        else {
            if (currentCount < maxInstructions) {
                instructions.push_back(generateRandomInstruction(screenName));
                currentCount++;
            }
        }
    }
    return instructions;
}

void Scheduler::generateDummyProcesses() {
 //   int batch = 0;
 //   random_device rd;
 //   mt19937 gen(rd());
 //   uniform_int_distribution<> instr_dist(minInstructions, maxInstructions); 
 //   uniform_int_distribution<> value_dist(1, 100);
 //   uniform_int_distribution<> type_dist(0, 4);

	////generate a random instruction (excluding FOR)
 //   auto generateRandomInstruction = [&](const string& screenName) -> Instruction {
 //       InstructionType type = static_cast<InstructionType>(type_dist(gen));
 //       switch (type) {
 //       case InstructionType::ADD:
 //           return { InstructionType::ADD, {{true, "x", 0}, {true, "x", 0}, {false, "", (uint16_t)value_dist(gen)}}, "" };
 //       case InstructionType::SUBTRACT:
 //           return { InstructionType::SUBTRACT, {{true, "x", 0}, {true, "x", 0}, {false, "", (uint16_t)(value_dist(gen) % 50)}}, "" };
 //       case InstructionType::PRINT:
 //           return { InstructionType::PRINT, {{true, "x", 0}}, "Value from " + screenName + ": %x%!" };
 //       case InstructionType::SLEEP:
 //           return { InstructionType::SLEEP, {{false, "", (uint16_t)(value_dist(gen) % 20 + 10)}}, "" };
 //       default:
 //           return { InstructionType::PRINT, {}, "Hello world from " + screenName + "!" };
 //       }
 //   };

 //   //generate a FOR loop with nested instructions-(max 3)
 //   function<void(int, vector<Instruction>&, int&, int)> generateForInstruction;
 //   generateForInstruction = [&](int nestLevel, vector<Instruction>& instructions, int& currentCount, int maxCount) {
 //       if (currentCount >= maxCount || nestLevel > 3) return;//maximum of 3 nest levels only

 //       //ensure there's enough space for the FOR instruction itself and at least one inner instruction.
 //       if (currentCount + 2 > maxCount) return;

 //       int repeats = value_dist(gen) % 4 + 2; // 2-5 repeats
 //       int innerCount = value_dist(gen) % 3 + 2; // 2-4 inner instructions
 //       vector<Instruction> innerInstructions;

 //       int tempInstructionCount = 0; //use a temporary counter for inner instructions

 //       for (int j = 0; j < innerCount && (currentCount + tempInstructionCount < maxCount); ++j) {
 //           if (nestLevel < 3 && value_dist(gen) % 4 == 0) {
 //               //pass innerInstructions to the recursive call
 //               generateForInstruction(nestLevel + 1, innerInstructions, tempInstructionCount, innerCount);
 //           }
 //           else {
 //               innerInstructions.push_back(generateRandomInstruction("FOR"));
 //               tempInstructionCount++;
 //           }
 //       }

 //       //only add the FOR instruction if it contains inner instructions
 //       if (!innerInstructions.empty()) {
 //           Instruction forInstr = { InstructionType::FOR, {{false, "", (uint16_t)repeats}}, "", innerInstructions };
 //           instructions.push_back(forInstr); //add it
 //           currentCount += tempInstructionCount + 1; //update main counter
 //       }
 //   };

	////generate a process with random instructions
 //   auto generateProcess = [&](const string& screenName) {
 //       vector<Instruction> instructions;
 //       int num_instructions = instr_dist(gen);
 //       int currentCount = 0;
 //       instructions.push_back({ InstructionType::DECLARE, {{true, "x", 0}, {false, "", (uint16_t)value_dist(gen)}}, "" });
 //       currentCount++;

 //       for (int i = 0; i < num_instructions && currentCount < maxInstructions; ++i) {
 //           if (i > 0 && value_dist(gen) % 5 == 0) {
 //               generateForInstruction(1, instructions, currentCount, maxInstructions);
 //           } else {
 //               if (currentCount < maxInstructions) {
 //                   instructions.push_back(generateRandomInstruction(screenName));
 //                   currentCount++;
 //               }
 //           }
 //       }
 //       return instructions;
 //   };

    //int initialBatchSize = numCores;
    //for (int i = 0; i < initialBatchSize; ++i) {
    //    auto screenName = "p" + to_string(generatedProcessCount++);
    //    // Call the new reusable method
    //    auto instructions = generateInstructionsForProcess(screenName);
    //    auto screen = make_shared<Screen>(screenName, instructions, CLIController::getInstance()->getTimestamp());
    //    ScreenManager::getInstance()->registerScreen(screenName, screen);
    //    addProcessToQueue(screen);
    //}
    while (schedulerRunning.load()) {
        // Only generate processes if the flag is set to true
        if (generatingProcesses.load()) {
            // Check if it's time to generate a new process based on frequency
            if (cpuCycles - lastGenCycle >= batchProcessFreq) {
                lastGenCycle = cpuCycles;

                auto screenName = "p" + to_string(generatedProcessCount++);
                auto instructions = generateInstructionsForProcess(screenName);
                auto screen = make_shared<Screen>(screenName, instructions, CLIController::getInstance()->getTimestamp());
                ScreenManager::getInstance()->registerScreen(screenName, screen);
                addProcessToQueue(screen);
            }
        }
        // Sleep briefly to prevent the loop from consuming 100% CPU when idle
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Scheduler::loadConfig() {
    ifstream config("config.txt");
    //error checker
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
		maxOverallMem = 16384; //default 16MB
		memPerFrame = 16; //default 16KB
		memPerProc = 4096; //default 4KB
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
                algorithm = "fcfs"; //default value
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
        else if (key == "mem-per-proc") {
            memPerProc = stoi(value);
			if (memPerProc < 1) memPerProc = 1;
        }
    }
    //assign cores available
    coresAvailable = numCores;
}

int Scheduler::getUsedCores() const {
    return coresUsed.load(); // Use .load() for explicit atomic read
}
int Scheduler::getAvailableCores() const { return coresAvailable; }
int Scheduler::getIdleCpuTicks() { return idleCpuTicks; }
int Scheduler::getCpuCycles() const {
    // FIX: Use .load() for atomic reads
    return cpuCycles.load();
}
void Scheduler::setCpuCycles(int cycles) {
    // FIX: Use .store() for atomic writes
    cpuCycles.store(cycles);
}
int Scheduler::getDelayPerExec() const { return delayPerExec; }

int Scheduler::getMemPerProc() const { return memPerProc; }
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