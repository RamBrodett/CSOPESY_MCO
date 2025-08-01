#include "MemoryManager.h"
#include "CLIController.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <numeric>

MemoryManager* MemoryManager::instance = nullptr;
std::mutex MemoryManager::mutex_;

void MemoryManager::initialize(int totalMemory) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!instance) {
        instance = new MemoryManager(totalMemory);
    }
}

MemoryManager* MemoryManager::getInstance() {
    return instance;
}

void MemoryManager::destroy() {
    std::lock_guard<std::mutex> lock(mutex_);
    delete instance;
    instance = nullptr;
}

MemoryManager::MemoryManager(int totalMemory) : totalMemory(totalMemory) {
    memoryMap.push_back({ "free", 0, totalMemory });
}

// Allocates a block of memory using the first-fit algorithm(for now).
bool MemoryManager::allocate(const std::string& processId, int size) {
    std::lock_guard<std::mutex> lock(mapMutex_);

	/** this is the logic guys for further development
    * Iterate through the memory map to find the first available free block
    * that is large enough to accommodate the requested size. If a suitable
    * block is found, it is assigned to the process. If the block is larger
    * than required, it is split into an allocated block and a new free block.
    */

    for (auto it = memoryMap.begin(); it != memoryMap.end(); ++it) {
        if (it->processId == "free" && it->size >= size) {
            int remainingSize = it->size - size;
            it->size = size;
            it->processId = processId;

            if (remainingSize > 0) {
                memoryMap.insert(it + 1, { "free", it->startAddress + size, remainingSize });
            }
            return true;
        }
    }
    return false; // No suitable block found
}

//Deallocates the memory assigned to a specific process.
void MemoryManager::deallocate(const std::string& processId) {
    std::lock_guard<std::mutex> lock(mapMutex_);

    /* For cleaning up memory data*/
    processMemoryData.erase(processId);

	/** this is the logic guys for further development
    * Find the memory block associated with the given processId, marks it
    * as "free," and then calls mergeFreeBlocks() to combine any adjacent
    * free blocks to reduce fragmentation.
    */
    for (auto& block : memoryMap) {
        if (block.processId == processId) {
            block.processId = "free";
            break;
        }
    }
    mergeFreeBlocks();
}

// Merges adjacent free memory blocks to reduce fragmentation.
void MemoryManager::mergeFreeBlocks() {
    if (memoryMap.size() < 2) return;
	// iterate through the memory map and merge adjacent free blocks
    for (auto it = memoryMap.begin(); it != memoryMap.end() - 1; ) {
        auto next_it = it + 1;
        if (it->processId == "free" && next_it->processId == "free") {
            it->size += next_it->size;
            memoryMap.erase(next_it);
        }
        else {
            ++it;
        }
    }
}

int MemoryManager::getFragmentation() const {
    //std::lock_guard<std::mutex> lock(mapMutex_);
    int freeMemory = 0;
    for (const auto& block : memoryMap) {
        if (block.processId == "free") {
            freeMemory += block.size;
        }
    }
    return freeMemory;
}

int MemoryManager::getProcessCount() const {
    int count = 0;
    for (const auto& block : memoryMap) {
        if (block.processId != "free") {
            count++;
        }
    }
    return count;
}

void MemoryManager::printMemoryLayout(int cycle) const {
    std::lock_guard<std::mutex> lock(mapMutex_); // This lock is sufficient for both functions
    std::string filename = "memory_stamp_" + std::to_string(cycle) + ".txt";
    std::ofstream outFile(filename);

    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return;
    }

    outFile << "Timestamp: (" << CLIController::getInstance()->getTimestamp() << ")" << std::endl;
    outFile << "Number of processes in memory: " << getProcessCount() << std::endl; // getProcessCount will now execute under the existing lock
    outFile << "Total external fragmentation in KB: " << getFragmentation() << std::endl;
    outFile << std::endl;

    outFile << "---end--- = " << totalMemory << std::endl;
    outFile << std::endl;

    for (auto it = memoryMap.rbegin(); it != memoryMap.rend(); ++it) {
        if (it->processId != "free") {
            outFile << it->startAddress + it->size << std::endl;
            outFile << it->processId << std::endl;
            outFile << it->startAddress << std::endl;
            outFile << std::endl;
        }
    }

    outFile << "---start--- = 0" << std::endl;
    outFile.close();
}

bool MemoryManager::isAllocated(const std::string& processId) const {
    std::lock_guard<std::mutex> lock(mapMutex_);
    for (const auto& block : memoryMap) {
        if (block.processId == processId) {
            return true;
        }
    }
    return false;
}

pair<int, int> MemoryManager::getProcessMemoryBounds(const string& processId) const {
    std::lock_guard<std::mutex> lock(mapMutex_);
    for (const auto& block : memoryMap) {
        if (block.processId == processId) {
            return { block.startAddress, block.startAddress + block.size - 1 };
        }
    }
    return { -1, -1 }; 
}

bool MemoryManager::isValidMemoryAccess(const string& processId, uint16_t address) const {
    auto bounds = getProcessMemoryBounds(processId);
    if (bounds.first == -1) return false;
    return address >= bounds.first && address <= bounds.second;
}

bool MemoryManager::readMemory(const std::string& processId, uint16_t address, uint16_t& value) {
    std::lock_guard<std::mutex> lock(mapMutex_);

    if (!isValidMemoryAccess(processId, address)) {
        return false; 
    }

    // Check if this memory location has been written to before
    if (processMemoryData.find(processId) != processMemoryData.end() &&
        processMemoryData[processId].find(address) != processMemoryData[processId].end()) {
        value = processMemoryData[processId][address];
    }
    else {
        value = 0; 
    }

    return true;
}

bool MemoryManager::writeMemory(const std::string& processId, uint16_t address, uint16_t value) {
    std::lock_guard<std::mutex> lock(mapMutex_);

    if (!isValidMemoryAccess(processId, address)) {
        return false;
    }

    processMemoryData[processId][address] = value;
    return true;
}