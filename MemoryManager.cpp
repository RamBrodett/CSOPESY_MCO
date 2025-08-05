#include "MemoryManager.h"
#include "CLIController.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <numeric>
using namespace std;

// --- Singleton & Mutex ---
MemoryManager* MemoryManager::instance = nullptr;
mutex MemoryManager::mutex_;

// Initializes the MemoryManager singleton with total memory and frame size details
void MemoryManager::initialize(int totalMemory, int frameSize) {
    lock_guard<mutex> lock(mutex_);
    if (!instance) {
        instance = new MemoryManager(totalMemory, frameSize);
    }
}

// Returns the singleton instance of the MemoryManager.
MemoryManager* MemoryManager::getInstance() {
    return instance;
}

// Destroys the singleton instance to free memory.
void MemoryManager::destroy() {
    lock_guard<mutex> lock(mutex_);
    delete instance;
    instance = nullptr;
}


// Constructor: sets up the physical memory emulation, frame table, and free list.
MemoryManager::MemoryManager(int totalMemory, int frameSize)
    : totalMemory(totalMemory), frameSize(frameSize) {
    numFrames = totalMemory / frameSize;
    frame_table.resize(numFrames);
    physical_memory.resize(totalMemory / sizeof(uint16_t), 0);

    for (int i = 0; i < numFrames; ++i) {
        free_frame_list.push_back(i);
    }
    // Clear the backing store on startup
    ofstream("csopesy-backing-store.txt", ios::trunc).close();
}

// Creates the initial page table for a new process based on its required memory size.
bool MemoryManager::setupProcessMemory(const string& processId, int size) {
    lock_guard<std::mutex> lock(memory_mutex_);
    int num_pages_required = (size + frameSize - 1) / frameSize; // Ceiling division

    process_page_tables[processId] = PageTable(num_pages_required);
    return true;
}

// Releases all memory frames allocated to a specific process.
void MemoryManager::deallocate(const std::string& processId) {
    std::lock_guard<std::mutex> lock(memory_mutex_);
    if (process_page_tables.find(processId) == process_page_tables.end()) return;

    // Iterate the process's page table and release each valid frame.
    for (const auto& pte : process_page_tables.at(processId)) {
        if (pte.valid) {
            int frame_num = pte.frameNumber;
            frame_table[frame_num].allocated = false;
            frame_table[frame_num].processId = "";
            frame_table[frame_num].pageNumber = -1;
            free_frame_list.push_back(frame_num);
        }
    }
    process_page_tables.erase(processId);
}


// Reads a value from a process's logical memory; triggers a page fault if needed.
bool MemoryManager::readMemory(const string& processId, uint16_t address, uint16_t& value) {
    std::lock_guard<std::mutex> lock(memory_mutex_);
    if (process_page_tables.find(processId) == process_page_tables.end()) return false;

    int page_num = address / frameSize;
    int offset = address % frameSize;

    if (page_num >= process_page_tables.at(processId).size()) return false; // Access violation

    if (!process_page_tables.at(processId)[page_num].valid) {
        if (handlePageFault(processId, page_num) == -1) return false;
    }

    int frame_num = process_page_tables.at(processId)[page_num].frameNumber;
    int physical_address = (frame_num * frameSize + offset) / sizeof(uint16_t);
    value = physical_memory[physical_address];
    return true;
}

// Writes a value to a process's logical memory; triggers a page fault if needed.
bool MemoryManager::writeMemory(const std::string& processId, uint16_t address, uint16_t value) {
    std::lock_guard<std::mutex> lock(memory_mutex_);
    if (process_page_tables.find(processId) == process_page_tables.end()) return false;

    int page_num = address / frameSize;
    int offset = address % frameSize;

    if (page_num >= process_page_tables.at(processId).size()) return false; // Access violation

    if (!process_page_tables.at(processId)[page_num].valid) {
        if (handlePageFault(processId, page_num) == -1) return false;
    }

    auto& pte = process_page_tables.at(processId)[page_num];
    int frame_num = pte.frameNumber;
    int physical_address = (frame_num * frameSize + offset) / sizeof(uint16_t);
    physical_memory[physical_address] = value;
    pte.dirty = true;
    return true;
}

// Handles a page fault by finding a frame, evicting if necessary, and loading the required page.
int MemoryManager::handlePageFault(const std::string& processId, int pageNumber) {
    int target_frame;

    if (!free_frame_list.empty()) {
        target_frame = free_frame_list.front();
        free_frame_list.pop_front();
    }
    else {
        target_frame = findVictimFrame();
        const auto& victim_frame_info = frame_table[target_frame];

        // Check if the victim frame is actually associated with a process
        if (process_page_tables.count(victim_frame_info.processId)) {
            auto& victim_pte = process_page_tables.at(victim_frame_info.processId)[victim_frame_info.pageNumber];
            if (victim_pte.dirty) {
                writePageToBackingStore(target_frame);
                pages_paged_out++;
            }
            victim_pte.valid = false;
            victim_pte.frameNumber = -1;
        }
    }

    readPageFromBackingStore(processId, pageNumber, target_frame);
    pages_paged_in++; 

    frame_table[target_frame] = { true, processId, pageNumber }; 
    auto& pte = process_page_tables.at(processId)[pageNumber];
    pte.frameNumber = target_frame;
    pte.valid = true;
    pte.dirty = false;

    return target_frame;
}

// Selects a victim frame to be replaced using a simple FIFO algorithm.
int MemoryManager::findVictimFrame() {
    static int next_victim_frame = 0;
    int victim = next_victim_frame;
    next_victim_frame = (next_victim_frame + 1) % numFrames;
    return victim;
}

/*/ Writes the content of a frame to the backing store file.
void MemoryManager::writePageToBackingStore(int frameNumber) {
    fstream file("csopesy-backing-store.txt", ios::in | ios::out | ios::binary);
    if (!file) {
        file.open("csopesy-backing-store.txt", ios::out | ios::binary);
    }
    file.seekp(frameNumber * frameSize, ios::beg);
    int physical_address = (frameNumber * frameSize) / sizeof(uint16_t);
    file.write(reinterpret_cast<const char*>(&physical_memory[physical_address]), frameSize);
}*/

// Helper function to calculate a unique file offset for a page
long long MemoryManager::getBackingStoreOffset(const std::string& processId, int pageNumber) const {
    long long pageSlot = 0;
    bool processFound = false;

    // This loop calculates a unique, stable position for the page in the file.
    for (const auto& pair : process_page_tables) {
        if (pair.first == processId) {
            pageSlot += pageNumber;
            processFound = true;
            break; // Stop after finding the process
        }
        pageSlot += pair.second.size(); // Add the total number of pages for the preceding process
    }

    if (!processFound) {
        // This case should ideally not be hit if the function is called correctly
        return -1;
    }

    return pageSlot * frameSize;
}

// Corrected function to write a page to its unique location
void MemoryManager::writePageToBackingStore(int frameNumber) {
    const auto& frameInfo = frame_table[frameNumber];
    if (frameInfo.processId.empty()) return;

    long long fileOffset = getBackingStoreOffset(frameInfo.processId, frameInfo.pageNumber);
    if (fileOffset == -1) return; // Process not found, cannot write

    // Use fstream for robust read/write operations
    fstream file("csopesy-backing-store.txt", ios::in | ios::out | ios::binary);
    if (!file) {
        // If the file doesn't exist, create it
        file.open("csopesy-backing-store.txt", ios::out | ios::binary);
        file.close();
        file.open("csopesy-backing-store.txt", ios::in | ios::out | ios::binary);
    }

    file.seekp(fileOffset, ios::beg);
    int physical_address = (frameNumber * frameSize) / sizeof(uint16_t);
    file.write(reinterpret_cast<const char*>(&physical_memory[physical_address]), frameSize);
}

// Corrected function to read a page from its unique location
void MemoryManager::readPageFromBackingStore(const std::string& processId, int pageNumber, int frameNumber) {
    long long fileOffset = getBackingStoreOffset(processId, pageNumber);
    if (fileOffset == -1) {
        // If we can't determine an offset, we must zero out the memory to prevent data corruption.
        int physical_address = (frameNumber * frameSize) / sizeof(uint16_t);
        fill(physical_memory.begin() + physical_address, physical_memory.begin() + physical_address + (frameSize / sizeof(uint16_t)), 0);
        return;
    }

    fstream file("csopesy-backing-store.txt", ios::in | ios::binary);
    int physical_address = (frameNumber * frameSize) / sizeof(uint16_t);

    if (file) {
        file.seekg(fileOffset, ios::beg);
        file.read(reinterpret_cast<char*>(&physical_memory[physical_address]), frameSize);
        // If the read was short (gcount() < frameSize), it means we read past the end
        // of the file, which implies this page was never written out. So, we zero it.
        if (file.gcount() < frameSize) {
            fill(physical_memory.begin() + physical_address, physical_memory.begin() + physical_address + (frameSize / sizeof(uint16_t)), 0);
        }
    }
    else {
        // If the file doesn't exist at all, the page is new; zero it out.
        fill(physical_memory.begin() + physical_address, physical_memory.begin() + physical_address + (frameSize / sizeof(uint16_t)), 0);
    }
}

// Returns the total configured memory of the system.
int MemoryManager::getTotalMemory() const {
    return totalMemory;
}

// Returns the current amount of used memory in bytes.
int MemoryManager::getUsedMemory() const {
    return (numFrames - static_cast<int>(free_frame_list.size())) * frameSize;
}

// Returns the memory usage for a single process.
int MemoryManager::getProcessMemoryUsage(const std::string& processId) const {
    if (process_page_tables.find(processId) == process_page_tables.end()) {
        return 0;
    }
    int valid_pages = 0;
    for (const auto& pte : process_page_tables.at(processId)) {
        if (pte.valid) {
            valid_pages++;
        }
    }
    return valid_pages * frameSize;
}

// Returns the total count of pages paged in from the backing store.
int MemoryManager::getPagedInCount() const {
    return pages_paged_in.load();
}

// Returns the total count of pages paged out to the backing store.
int MemoryManager::getPagedOutCount() const {
    return pages_paged_out.load();
}

// Prints the current status of the frame table for debugging.
void MemoryManager::printFrameTable() const {
    lock_guard<std::mutex> lock(memory_mutex_);
    cout << "--- Frame Table Status ---" << endl;
    cout << "Frame | Allocated | Process ID | Page Num" << endl;
    cout << "--------------------------" << endl;
    for (int i = 0; i < numFrames; ++i) {
        const auto& frame = frame_table[i];
        cout << setw(5) << i << " | "
            << setw(9) << (frame.allocated ? "Yes" : "No") << " | "
            << setw(10) << (frame.allocated ? frame.processId : "N/A") << " | "
            << setw(8) << (frame.allocated ? to_string(frame.pageNumber) : "N/A")
            << endl;
    }
    cout << "Free frames left: " << free_frame_list.size() << endl;
    cout << "--------------------------" << endl;
}