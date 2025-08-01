#include "MemoryManager.h"
#include "CLIController.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <numeric>

MemoryManager* MemoryManager::instance = nullptr;
std::mutex MemoryManager::mutex_;

// --- Singleton and Initialization ---
void MemoryManager::initialize(int totalMemory, int pageSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!instance) {
        instance = new MemoryManager(totalMemory, pageSize);
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

MemoryManager::MemoryManager(int totalMemory, int pageSize)
    : totalMemory(totalMemory), pageSize(pageSize) {
    if (pageSize <= 0) pageSize = 1;
    this->numFrames = totalMemory / pageSize;
    this->physicalFrames.resize(numFrames);
}

// --- Public Interface for Demand Paging ---
void MemoryManager::createProcessPageTable(const std::string& processId, int virtualMemorySize) {
    std::lock_guard<std::mutex> lock(managerMutex);
    if (pageSize <= 0) return;
    int numPages = (virtualMemorySize + pageSize - 1) / pageSize;
    processPageTables[processId] = std::vector<PageTableEntry>(numPages);
    backingStore[processId].resize((size_t)virtualMemorySize / 2, 0); // For uint16_t
}

void MemoryManager::accessPage(const std::string& processId, int pageNumber) {
    std::lock_guard<std::mutex> lock(managerMutex);
    if (processPageTables.count(processId) && pageNumber < processPageTables[processId].size()) {
        if (!processPageTables[processId][pageNumber].present) {
            handlePageFault(processId, pageNumber);
        }
    }
}

void MemoryManager::deallocateProcess(const std::string& processId) {
    std::lock_guard<std::mutex> lock(managerMutex);
    for (int i = 0; i < numFrames; ++i) {
        if (physicalFrames[i].processId == processId) {
            physicalFrames[i].isFree = true;
            physicalFrames[i].processId = "";
            physicalFrames[i].pageNumber = -1;
        }
    }
    processPageTables.erase(processId);
    backingStore.erase(processId); // Also clear from backing store map
    fifoQueue.remove_if([&](int frameIndex) { return physicalFrames[frameIndex].processId == processId || physicalFrames[frameIndex].isFree; });
}

uint16_t MemoryManager::readFromMemory(const std::string& processId, int address) {
    // This function assumes the page is already in memory.
    // Address is the virtual address in bytes.
    return backingStore[processId][address / 2];
}

void MemoryManager::writeToMemory(const std::string& processId, int address, uint16_t value) {
    // This function assumes the page is already in memory.
    int pageNumber = address / pageSize;
    int frameNumber = processPageTables[processId][pageNumber].frameNumber;

    physicalFrames[frameNumber].isDirty = true; // Mark the frame as dirty
    backingStore[processId][address / 2] = value;
}


// --- Private Helper Methods ---
void MemoryManager::handlePageFault(const std::string& processId, int pageNumber) {
    pagedInCount++; // For vmstat
    int frameIndex = findFreeFrame();
    if (frameIndex == -1) {
        frameIndex = evictPageFIFO();
    }
    physicalFrames[frameIndex].isFree = false;
    physicalFrames[frameIndex].processId = processId;
    physicalFrames[frameIndex].pageNumber = pageNumber;
    physicalFrames[frameIndex].isDirty = false;
    processPageTables[processId][pageNumber].present = true;
    processPageTables[processId][pageNumber].frameNumber = frameIndex;
    fifoQueue.push_back(frameIndex);
}

int MemoryManager::findFreeFrame() {
    for (int i = 0; i < numFrames; ++i) {
        if (physicalFrames[i].isFree) return i;
    }
    return -1;
}

int MemoryManager::evictPageFIFO() {
    pagedOutCount++; // For vmstat
    int victimFrameIndex = fifoQueue.front();
    fifoQueue.pop_front();

    const auto& victimFrame = physicalFrames[victimFrameIndex];
    if (processPageTables.count(victimFrame.processId)) {
        // In a real system, if victimFrame.isDirty, we'd write to a file here.
        // For this simulation, the write happens immediately in writeToMemory.
        processPageTables[victimFrame.processId][victimFrame.pageNumber].present = false;
        processPageTables[victimFrame.processId][victimFrame.pageNumber].frameNumber = -1;
    }
    return victimFrameIndex;
}

// --- NEW: Corrected memory layout printing for demand paging ---
void MemoryManager::printMemoryLayout(int cycle) const {
    std::lock_guard<std::mutex> lock(managerMutex);
    std::string filename = "memory_stamp_" + std::to_string(cycle) + ".txt";
    std::ofstream outFile(filename);

    outFile << "--- Physical Memory Frames at Cycle " << cycle << " ---" << std::endl;
    outFile << "Timestamp: (" << CLIController::getInstance()->getTimestamp() << ")" << std::endl;
    outFile << "Total Frames: " << numFrames << " | Page Size: " << pageSize << " KB" << std::endl;
    outFile << std::left << std::setw(10) << "Frame #" << std::setw(15) << "Status" << std::setw(20) << "Process ID" << std::setw(15) << "Page #" << std::endl;
    outFile << "----------------------------------------------------------" << std::endl;

    for (int i = 0; i < numFrames; ++i) {
        outFile << std::left << std::setw(10) << i;
        if (physicalFrames[i].isFree) {
            outFile << std::setw(15) << "Free" << std::endl;
        }
        else {
            outFile << std::setw(15) << "Used" << std::setw(20) << physicalFrames[i].processId << std::setw(15) << physicalFrames[i].pageNumber << std::endl;
        }
    }
    outFile.close();
}

// --- Getters for vmstat and process-smi ---
int MemoryManager::getTotalFrameCount() const { return numFrames; }
int MemoryManager::getUsedFrameCount() const {
    int count = 0;
    for (const auto& frame : physicalFrames) if (!frame.isFree) count++;
    return count;
}
int MemoryManager::getPagedInCount() const { return pagedInCount.load(); }
int MemoryManager::getPagedOutCount() const { return pagedOutCount.load(); }