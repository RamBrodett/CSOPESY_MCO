#include "MemoryManager.h"
#include "CLIController.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <numeric>


MemoryManager* MemoryManager::instance = nullptr;
std::mutex MemoryManager::mutex_;

void MemoryManager::initialize(int totalMemory, int frameSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!instance) {
        instance = new MemoryManager(totalMemory, frameSize);
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

//MemoryManager::MemoryManager(int totalMemory) : totalMemory(totalMemory) {
//    memoryMap.push_back({ "free", 0, totalMemory });
//}
// Constructor
MemoryManager::MemoryManager(int totalMemory, int frameSize)
    : totalMemory(totalMemory), frameSize(frameSize) {
    numFrames = totalMemory / frameSize;
    frame_table.resize(numFrames);
    physical_memory.resize(totalMemory / sizeof(uint16_t), 0);

    for (int i = 0; i < numFrames; ++i) {
        free_frame_list.push_back(i);
    }
    // Clear the backing store on startup
    std::ofstream("csopesy-backing-store.txt", std::ios::trunc).close();
}


bool MemoryManager::setupProcessMemory(const std::string& processId, int size) {
    std::lock_guard<std::mutex> lock(memory_mutex_);
    int num_pages_required = (size + frameSize - 1) / frameSize; // Ceiling division

    process_page_tables[processId] = PageTable(num_pages_required);
    return true;
}


//// Allocates a block of memory using the first-fit algorithm(for now).
//bool MemoryManager::allocate(const std::string& processId, int size) {
//    std::lock_guard<std::mutex> lock(mapMutex_);
//
//    /** this is the logic guys for further development
//    * Iterate through the memory map to find the first available free block
//    * that is large enough to accommodate the requested size. If a suitable
//    * block is found, it is assigned to the process. If the block is larger
//    * than required, it is split into an allocated block and a new free block.
//    */
//
//    for (auto it = memoryMap.begin(); it != memoryMap.end(); ++it) {
//        if (it->processId == "free" && it->size >= size) {
//            int remainingSize = it->size - size;
//            it->size = size;
//            it->processId = processId;
//
//            if (remainingSize > 0) {
//                memoryMap.insert(it + 1, { "free", it->startAddress + size, remainingSize });
//            }
//            return true;
//        }
//    }
//    return false; // No suitable block found
//}

void MemoryManager::deallocate(const std::string& processId) {
    std::lock_guard<std::mutex> lock(memory_mutex_);
    if (process_page_tables.find(processId) == process_page_tables.end()) return;

    for (const auto& pte : process_page_tables.at(processId)) {
        if (pte.valid) {
            // ==============================================================================
            // CHANGE: Corrected member access from snake_case to camelCase.
            // ==============================================================================
            int frame_num = pte.frameNumber;
            frame_table[frame_num].allocated = false;
            frame_table[frame_num].processId = "";
            frame_table[frame_num].pageNumber = -1;
            free_frame_list.push_back(frame_num);
        }
    }
    process_page_tables.erase(processId);
}
//// Merges adjacent free memory blocks to reduce fragmentation.
//void MemoryManager::mergeFreeBlocks() {
//    if (memoryMap.size() < 2) return;
//    // iterate through the memory map and merge adjacent free blocks
//    for (auto it = memoryMap.begin(); it != memoryMap.end() - 1; ) {
//        auto next_it = it + 1;
//        if (it->processId == "free" && next_it->processId == "free") {
//            it->size += next_it->size;
//            memoryMap.erase(next_it);
//        }
//        else {
//            ++it;
//        }
//    }
//}

// ==============================================================================
// CHANGE: 'readMemory' and 'writeMemory' now perform logical-to-physical
// address translation and trigger page faults if a page is not resident.
// ==============================================================================
bool MemoryManager::readMemory(const std::string& processId, uint16_t address, uint16_t& value) {
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

            // ==debugger==
            //std::cout << "[DEBUG] Evicting Frame " << target_frame
            //<< " (Owned by: " << victim_frame_info.processId
            //<< ", Page: " << victim_frame_info.pageNumber
            //<< ", Dirty: " << (victim_pte.dirty ? "Yes" : "No") << ")" << std::endl;
            //// ==============================================================================

            if (victim_pte.dirty) {
                writePageToBackingStore(target_frame);
                pages_paged_out++; // This will now be correctly incremented
            }
            victim_pte.valid = false;
            victim_pte.frameNumber = -1;
        }
    }

    readPageFromBackingStore(processId, pageNumber, target_frame);
    pages_paged_in++; // This will now be correctly incremented

    // ==============================================================================
    // THE FIX: Change 'false' to 'true' to mark the frame as allocated.
    // ==============================================================================
    frame_table[target_frame] = { true, processId, pageNumber }; // Set allocated to TRUE

    // Update the process's page table to reflect the new, valid page
    //process_page_tables.at(processId)[pageNumber] = { target_frame, true, false };
    auto& pte = process_page_tables.at(processId)[pageNumber];
    pte.frameNumber = target_frame;
    pte.valid = true;
    pte.dirty = false;

    return target_frame;
}

int MemoryManager::findVictimFrame() {
    // Simple FIFO: the first frame allocated is the first to be replaced.
    static int next_victim_frame = 0;
    int victim = next_victim_frame;
    next_victim_frame = (next_victim_frame + 1) % numFrames;
    return victim;
}

void MemoryManager::writePageToBackingStore(int frameNumber) {
    fstream file("csopesy-backing-store.txt", ios::in | ios::out | ios::binary);
    if (!file) {
        file.open("csopesy-backing-store.txt", ios::out | ios::binary);
    }
    file.seekp(frameNumber * frameSize, ios::beg);
    int physical_address = (frameNumber * frameSize) / sizeof(uint16_t);
    file.write(reinterpret_cast<const char*>(&physical_memory[physical_address]), frameSize);
}

void MemoryManager::readPageFromBackingStore(const std::string& processId, int pageNumber, int frameNumber) {
    fstream file("csopesy-backing-store.txt", ios::in | ios::binary);
    int physical_address = (frameNumber * frameSize) / sizeof(uint16_t);

    if (file) {
        file.seekg(frameNumber * frameSize, ios::beg);
        file.read(reinterpret_cast<char*>(&physical_memory[physical_address]), frameSize);
        if (file.gcount() < frameSize) {
            // If read fails or is incomplete, zero out the memory
            fill(physical_memory.begin() + physical_address, physical_memory.begin() + physical_address + (frameSize / sizeof(uint16_t)), 0);
        }
    }
    else {
        // If file doesn't exist, the page is new; zero it out
        fill(physical_memory.begin() + physical_address, physical_memory.begin() + physical_address + (frameSize / sizeof(uint16_t)), 0);
    }
}

// ==============================================================================
// CHANGE: Added getter functions to provide stats to ScreenManager.
// ==============================================================================
int MemoryManager::getTotalMemory() const {
    return totalMemory;
}

int MemoryManager::getUsedMemory() const {
    return (numFrames - static_cast<int>(free_frame_list.size())) * frameSize;
}

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

int MemoryManager::getPagedInCount() const {
    return pages_paged_in.load();
}

int MemoryManager::getPagedOutCount() const {
    return pages_paged_out.load();
}


//int MemoryManager::getFragmentation() const {
//    //std::lock_guard<std::mutex> lock(mapMutex_);
//    int freeMemory = 0;
//    for (const auto& block : memoryMap) {
//        if (block.processId == "free") {
//            freeMemory += block.size;
//        }
//    }
//    return freeMemory;
//}

void MemoryManager::printFrameTable() const {
    std::lock_guard<std::mutex> lock(memory_mutex_);
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