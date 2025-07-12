#include "Kernel.h"
using namespace std;

Kernel* Kernel::instance = nullptr;

Kernel::Kernel() {}

Kernel::~Kernel() {//destructor 
}

void Kernel::initialize() {
	if (!instance) instance = new Kernel();
}

Kernel* Kernel::getInstance() {
	return instance;
}

bool Kernel::getRunningStatus() const {
	return this->isRunning;
}

void Kernel::setRunningStatus(bool newStatus) {
	this->isRunning = newStatus;
}

void Kernel::destroy() {
	delete instance;
	instance = nullptr;
}

thread& Kernel::getSchedulerThread() {
	return this->schedulerThread;
}

void Kernel::setConfigInitialized(bool status) {
	configInitialized = status; 
}
bool Kernel::isConfigInitialized() const { 
	return configInitialized; 
}
