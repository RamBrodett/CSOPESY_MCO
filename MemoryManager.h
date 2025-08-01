// Corrected MemoryManager.h
#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <list>
#include <map>
#include <cstdint>
#include <atomic>

// Represents an entry in a process's Page Table.
struct PageTableEntry {
    int frameNumber = -1;
    bool present = false;
    // 'dirty' bit is tracked on the Frame struct itself
};

// Represents a physical memory frame.
struct Frame {
    std::string processId = "";
    int pageNumber = -1;
    bool isFree = true;
    bool isDirty = false; // NEW: To track if a page needs to be written to backing store
};

class MemoryManager {
public:
    // --- Singleton Access ---
    static void initialize(int totalMemory, int pageSize);
    static MemoryManager* getInstance();
    static void destroy();

    // --- Paging Operations ---
    void createProcessPageTable(const std::string& processId, int virtualMemorySize);
    void accessPage(const std::string& processId, int pageNumber);
    void deallocateProcess(const std::string& processId);

    // --- Data Operations ---
    uint16_t readFromMemory(const std::string& processId, int address);
    void writeToMemory(const std::string& processId, int address, uint16_t value);

    // --- Reporting ---
    void printMemoryLayout(int cycle) const;
    int getTotalFrameCount() const;
    int getUsedFrameCount() const;
    int getPagedInCount() const;
    int getPagedOutCount() const;

private:
    MemoryManager(int totalMemory, int pageSize);
    void handlePageFault(const std::string& processId, int pageNumber);
    int findFreeFrame();
    int evictPageFIFO();

    static MemoryManager* instance;
    static std::mutex mutex_;

    int numFrames;
    int pageSize;
    int totalMemory;

    std::vector<Frame> physicalFrames;
    std::map<std::string, std::vector<PageTableEntry>> processPageTables;
    std::map<std::string, std::vector<uint16_t>> backingStore;
    std::list<int> fifoQueue;

    // For vmstat
    std::atomic<int> pagedInCount{ 0 };
    std::atomic<int> pagedOutCount{ 0 };

    mutable std::mutex managerMutex;
};