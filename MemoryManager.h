#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <unordered_map>
#include <list>
#include <atomic>
using namespace std;

//// Represents a block of memory, either allocated or free.
//struct MemoryBlock {
//    std::string processId; // "free" if the block is not allocated
//    int startAddress;
//    int size;
//};
// removed due to incompability with the specification

struct Frame {
    bool allocated = false;
    std::string processId;
    int pageNumber = -1;
};

struct PageTableEntry {
    int frameNumber = -1; // -1 if not in memory
    bool valid = false;
    bool dirty = false;
};

using PageTable = std::vector<PageTableEntry>;

class MemoryManager {
public:

    // --- Singleton Access ---
    static void initialize(int totalMemory, int frameSize); // added 
    static MemoryManager* getInstance();
    static void destroy();

    // --- Memory Operations ---
    bool setupProcessMemory(const std::string& processId, int size);
    void deallocate(const std::string& processId);

    //int getFragmentation() const;
    //void printMemoryLayout(int cycle) const;
    //int getProcessCount() const;
    //bool isAllocated(const std::string& processId) const;

    // Memory Access
    bool readMemory(const string& processId, uint16_t address, uint16_t& value);
	bool writeMemory(const string& processId, uint16_t address, uint16_t value);
    //bool isValidMemoryAccess(const string& processId, uint16_t address) const;

    //int translateAddress(const std::string& processId, uint16_t logicalAddress);
    
    //// --- Paging & Backing Store ---
    //void writePageToBackingStore(const std::string& processId, int pageNumber, const std::vector<uint16_t>& pageData);
    //bool readPageFromBackingStore(const std::string& processId, int pageNumber, std::vector<uint16_t>& pageData);
    //void handlePageFault(const std::string& processId, int pageNumber, int pageSize);

    //getter for statistics
    int getTotalMemory() const;
    int getUsedMemory() const;
    int getProcessMemoryUsage(const std::string& processId) const;
    int getPagedInCount() const;
    int getPagedOutCount() const;

    void printFrameTable() const;

private:
    MemoryManager(int totalMemory, int frameSize);
    static MemoryManager* instance;
    static std::mutex mutex_;

    // --- Page Fault and Backing Store Logic (Private) ---
    int handlePageFault(const std::string& processId, int pageNumber);
    int findVictimFrame();
    void writePageToBackingStore(int frameNumber);
    void readPageFromBackingStore(const std::string& processId, int pageNumber, int frameNumber);

    // --- Data Structures ---
    int totalMemory;
    int frameSize;
    int numFrames;

    // ==============================================================================
    // CHANGE: Replaced the old 'memoryMap' with a proper frame table, free list,
    // page tables, and an emulated physical memory space.
    // ==============================================================================
    std::vector<Frame> frame_table;
    std::list<int> free_frame_list;
    std::unordered_map<std::string, PageTable> process_page_tables;
    std::vector<uint16_t> physical_memory; // Emulated physical RAM

    // --- Statistics ---
    std::atomic<int> pages_paged_in{ 0 };
    std::atomic<int> pages_paged_out{ 0 };

    mutable std::mutex memory_mutex_;



    //std::vector<MemoryBlock> memoryMap;
    //mutable std::mutex mapMutex_;

    //mutable unordered_map<string, unordered_map<uint16_t, uint16_t>> processMemoryData;
    //std::vector<Frame> frameTable;
    //std::unordered_map<std::string, PageTable> processPageTables;
    //int frameSize; // in bytes
    //int numFrames;

    //void mergeFreeBlocks();
    //pair<int, int> getProcessMemoryBounds(const string& processId) const;
};



