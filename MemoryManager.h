#pragma once
#include <vector>
#include <string>
#include <mutex>
using namespace std;

// Represents a block of memory, either allocated or free.
struct MemoryBlock {
    std::string processId; // "free" if the block is not allocated
    int startAddress;
    int size;
};

class MemoryManager {
public:

    // --- Singleton Access ---
    static void initialize(int totalMemory);
    static MemoryManager* getInstance();
    static void destroy();

    // --- Memory Operations ---
    bool allocate(const std::string& processId, int size);
    void deallocate(const std::string& processId);
    int getFragmentation() const;
    void printMemoryLayout(int cycle) const;
    int getProcessCount() const;

private:
    MemoryManager(int totalMemory);
    static MemoryManager* instance;
    static std::mutex mutex_;

    int totalMemory;
    std::vector<MemoryBlock> memoryMap;
    mutable std::mutex mapMutex_;

    void mergeFreeBlocks();
};



