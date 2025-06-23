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
                    else { // FCFS
                        process->execute();
                    }
                }
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
    uniform_int_distribution<> instr_dist(8, 15); // Slightly longer processes
    uniform_int_distribution<> value_dist(1, 100);
    uniform_int_distribution<> type_dist(0, 4);

    while (schedulerRunning) {
        this_thread::sleep_for(chrono::milliseconds(1000));

        auto screenName = "p" + to_string(batch++);
        vector<Instruction> instructions;
        int num_instructions = instr_dist(gen);

        instructions.push_back({ InstructionType::DECLARE, {{true, "x", 0}, {false, "", (uint16_t)value_dist(gen)}}, "" });

        for (int i = 0; i < num_instructions; ++i) {
            InstructionType type = static_cast<InstructionType>(type_dist(gen));
            switch (type) {
            case InstructionType::ADD:
                instructions.push_back({ InstructionType::ADD, {{true, "x", 0}, {true, "x", 0}, {false, "", (uint16_t)value_dist(gen)}}, "" });
                break;
            case InstructionType::SUBTRACT:
                instructions.push_back({ InstructionType::SUBTRACT, {{true, "x", 0}, {true, "x", 0}, {false, "", (uint16_t)(value_dist(gen) % 50)}}, "" });
                break;
            case InstructionType::PRINT:
                instructions.push_back({ InstructionType::PRINT, {{true, "x", 0}}, "Value from " + screenName + ": %x%!" });
                break;
            case InstructionType::SLEEP:
                instructions.push_back({ InstructionType::SLEEP, {{false, "", (uint16_t)(value_dist(gen) % 20 + 10)}}, "" });
                break;
            default:
                instructions.push_back({ InstructionType::PRINT, {}, "Hello world from " + screenName + "!" });
                break;
            }
        }

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

        if (key == "num-cpu")
            numCores = stoi(value);
        else if (key == "scheduler")
            algorithm = value;
        else if (key == "quantum-cycles")
            quantumCycles = stoi(value);
        else if (key == "batch-process-freq")
            batchProcessFreq = stoi(value);
        else if (key == "min-ins")
            minInstructions = stoi(value);
        else if (key == "max-ins")
            maxInstructions = stoi(value);
        else if (key == "delays-per-exec")
            delayPerExec = stoi(value);
    }
}

int Scheduler::getUsedCores() const { return coresUsed; }
int Scheduler::getAvailableCores() const { return coresAvailable; }
int Scheduler::getIdleCpuTicks() { return idleCpuTicks; }
int Scheduler::getCpuCycles() const { return cpuCycles; }
void Scheduler::setCpuCycles(int cycles) { cpuCycles = cycles; }

bool Scheduler::getSchedulerRunning() const { return schedulerRunning.load(); }
void Scheduler::setSchedulerRunning(bool val) { schedulerRunning.store(val); }
