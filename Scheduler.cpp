#include "Scheduler.h"
#include "ScreenManager.h"
#include "CLIController.h"
#include "Instruction.h"
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
    processQueueCondition.notify_one();
}

void Scheduler::start() {
    if (schedulerRunning) return;
    schedulerRunning.store(true);

    workerThreads.clear();

    processGeneratorThread = thread(&Scheduler::generateDummyProcesses, this);


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
                    process->setCoreID(i);
                    if (algorithm == "rr") {
                        process->execute(quantumCycles);
                        if (!process->isFinished()) {
                            addProcessToQueue(process);
                        }
                    }
                    else { //default fcfs
                        process->execute();
                    }
                }
                cpuCycles++;
            }
        });
    }
}

void Scheduler::stop() {
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

void Scheduler::generateDummyProcesses() {
    int batch = 0;
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> instr_dist(minInstructions, maxInstructions); 
    uniform_int_distribution<> value_dist(1, 100);
    uniform_int_distribution<> type_dist(0, 4);

	// helper: generate a random instruction (excluding FOR)
    auto generateRandomInstruction = [&](const string& screenName) -> Instruction {
        InstructionType type = static_cast<InstructionType>(type_dist(gen));
        switch (type) {
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

    // helper: generate a FOR loop with nested instructions (max 3)
    function<void(int, vector<Instruction>&, int&, int)> generateForInstruction;
    generateForInstruction = [&](int nestLevel, vector<Instruction>& instructions, int& currentCount, int maxCount) {
        if (currentCount >= maxCount) return;

        // Ensure there's enough space for the FOR instruction itself and at least one inner instruction.
        if (currentCount + 2 > maxCount) return;

        int repeats = value_dist(gen) % 4 + 2; // 2-5 repeats
        int innerCount = value_dist(gen) % 3 + 2; // 2-4 inner instructions
        vector<Instruction> innerInstructions;

        int tempInstructionCount = 0; // Use a temporary counter for inner instructions

        for (int j = 0; j < innerCount && (currentCount + tempInstructionCount < maxCount); ++j) {
            if (nestLevel < 3 && value_dist(gen) % 4 == 0) {
                // Pass innerInstructions to the recursive call
                generateForInstruction(nestLevel + 1, innerInstructions, tempInstructionCount, innerCount);
            }
            else {
                innerInstructions.push_back(generateRandomInstruction("FOR"));
                tempInstructionCount++;
            }
        }

        // Only add the FOR instruction if it contains inner instructions
        if (!innerInstructions.empty()) {
            Instruction forInstr = { InstructionType::FOR, {{false, "", (uint16_t)repeats}}, "", innerInstructions };
            instructions.push_back(forInstr); // Add the single, nested FOR instruction
            currentCount += tempInstructionCount + 1; // Update the main counter
        }
    };

	// helper: generate a process with random instructions
    auto generateProcess = [&](const string& screenName) {
        vector<Instruction> instructions;
        int num_instructions = instr_dist(gen);
        int currentCount = 0;
        instructions.push_back({ InstructionType::DECLARE, {{true, "x", 0}, {false, "", (uint16_t)value_dist(gen)}}, "" });
        currentCount++;

        for (int i = 0; i < num_instructions && currentCount < maxInstructions; ++i) {
            if (i > 0 && value_dist(gen) % 5 == 0) {
                generateForInstruction(1, instructions, currentCount, maxInstructions);
            } else {
                if (currentCount < maxInstructions) {
                    instructions.push_back(generateRandomInstruction(screenName));
                    currentCount++;
                }
            }
        }
        return instructions;
    };

//// initial dummy process generation
 //   {
 //       auto screenName = "p" + to_string(batch++);
 //       auto instructions = generateProcess(screenName);
 //       auto screen = make_shared<Screen>(screenName, instructions, CLIController::getInstance()->getTimestamp());
 //       ScreenManager::getInstance()->registerScreen(screenName, screen);
 //       addProcessToQueue(screen);
 //   }
    int initialBatchSize = numCores; //start with batch equal number to cores
    for (int i = 0; i < initialBatchSize; ++i) {
        auto screenName = "p" + to_string(batch++);
        auto instructions = generateProcess(screenName);
        auto screen = make_shared<Screen>(screenName, instructions, CLIController::getInstance()->getTimestamp());
        ScreenManager::getInstance()->registerScreen(screenName, screen);
        addProcessToQueue(screen);
    }

    while (schedulerRunning) {
        if (cpuCycles - lastGenCycle < batchProcessFreq) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        lastGenCycle = cpuCycles;

        auto screenName = "p" + to_string(batch++);
        auto instructions = generateProcess(screenName);
        auto screen = make_shared<Screen>(screenName, instructions, CLIController::getInstance()->getTimestamp());
        ScreenManager::getInstance()->registerScreen(screenName, screen);
        addProcessToQueue(screen);
    }
}

void Scheduler::loadConfig() {
    ifstream config("config.txt");
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
    }
}

int Scheduler::getUsedCores() const { return coresUsed; }
int Scheduler::getAvailableCores() const { return coresAvailable; }
int Scheduler::getIdleCpuTicks() { return idleCpuTicks; }
int Scheduler::getCpuCycles() const { return cpuCycles; }
void Scheduler::setCpuCycles(int cycles) { cpuCycles = cycles; }
int Scheduler::getDelayPerExec() const { return delayPerExec; }

bool Scheduler::getSchedulerRunning() const { return schedulerRunning.load(); }
void Scheduler::setSchedulerRunning(bool val) { schedulerRunning.store(val); }
