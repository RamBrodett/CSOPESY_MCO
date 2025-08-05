#include "Kernel.h"
using namespace std;

// Singleton
Kernel* Kernel::instance = nullptr;

// Constructor (private for singleton pattern).
Kernel::Kernel() {}

// Destructor to clean up resources.
Kernel::~Kernel() {
}

// Initializes the singleton instance of the Kernel.
void Kernel::initialize() {
	if (!instance) instance = new Kernel();
}

// Returns the singleton instance of the Kernel.
Kernel* Kernel::getInstance() {
	return instance;
}

// Gets the running status of the entire application.
bool Kernel::getRunningStatus() const {
	return this->isRunning;
}

// Sets the running status of the application (e.g., to false to exit).
void Kernel::setRunningStatus(bool newStatus) {
	this->isRunning = newStatus;
}

// Destroys the singleton instance to free memory.
void Kernel::destroy() {
	delete instance;
	instance = nullptr;
}

// Returns a reference to the scheduler thread.
thread& Kernel::getSchedulerThread() {
	return this->schedulerThread;
}

// Sets the configuration status to initialized.
void Kernel::setConfigInitialized(bool status) {
	configInitialized = status; 
}

// Checks if the configuration has been initialized.
bool Kernel::isConfigInitialized() const { 
	return configInitialized; 
}