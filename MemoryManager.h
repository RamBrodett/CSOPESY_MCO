#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <unordered_map>
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
    bool isAllocated(const std::string& processId) const;

    bool readMemory(const string& processId, uint16_t address, uint16_t& value);
	bool writeMemory(const string& processId, uint16_t address, uint16_t value);
    bool isValidMemoryAccess(const string& processId, uint16_t address) const;
    
    // --- Paging & Backing Store ---
    void writePageToBackingStore(const std::string& processId, int pageNumber, const std::vector<uint16_t>& pageData);
    bool readPageFromBackingStore(const std::string& processId, int pageNumber, std::vector<uint16_t>& pageData);
    void handlePageFault(const std::string& processId, int pageNumber, int pageSize);


private:
    MemoryManager(int totalMemory);
    static MemoryManager* instance;
    static std::mutex mutex_;

    int totalMemory;
    std::vector<MemoryBlock> memoryMap;
    mutable std::mutex mapMutex_;

    mutable unordered_map<string, unordered_map<uint16_t, uint16_t>> processMemoryData;
    
    void mergeFreeBlocks();
    pair<int, int> getProcessMemoryBounds(const string& processId) const;
};



