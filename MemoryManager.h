#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <unordered_map>
#include <list>
#include <atomic>
using namespace std;

// Represents a physical memory frame.
struct Frame {
    bool allocated = false;
    std::string processId;
    int pageNumber = -1;
};

// Represents an entry in a process's page table.
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


    // --- Memory Access ---
    bool readMemory(const string& processId, uint16_t address, uint16_t& value);
	bool writeMemory(const string& processId, uint16_t address, uint16_t value);
    
    // --- Statistics ---
    int getTotalMemory() const;
    int getUsedMemory() const;
    int getProcessMemoryUsage(const std::string& processId) const;
    int getPagedInCount() const;
    int getPagedOutCount() const;

    void printFrameTable() const;

private:
    MemoryManager(int totalMemory, int frameSize);

    long long getBackingStoreOffset(const std::string& processId, int pageNumber) const;
    // --- Page Fault and Backing Store Logic (Private) ---
    int handlePageFault(const std::string& processId, int pageNumber);
    int findVictimFrame();
    void writePageToBackingStore(int frameNumber);
    void readPageFromBackingStore(const std::string& processId, int pageNumber, int frameNumber);

    // --- Data Structures ---
    int totalMemory;
    int frameSize;
    int numFrames;

    std::vector<Frame> frame_table;
    std::list<int> free_frame_list;
    std::unordered_map<std::string, PageTable> process_page_tables;
    std::vector<uint16_t> physical_memory; 

    // --- Statistics Counter ---
    std::atomic<int> pages_paged_in{ 0 };
    std::atomic<int> pages_paged_out{ 0 };

    mutable std::mutex memory_mutex_;
    static MemoryManager* instance;
    static std::mutex mutex_;

};



