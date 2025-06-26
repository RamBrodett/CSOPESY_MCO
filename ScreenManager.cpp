#include "ScreenManager.h"
#include "CLIController.h"
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>

using namespace std;

ScreenManager* ScreenManager::instance = nullptr;

ScreenManager::ScreenManager() {}

void ScreenManager::initialize() {
    if (!instance) instance = new ScreenManager();
}

ScreenManager* ScreenManager::getInstance() {
    return instance;
}

void ScreenManager::destroy() {
    delete instance;
    instance = nullptr;
}

void ScreenManager::registerScreen(const string& name, shared_ptr<Screen> screen) {
    screens[name] = screen;
}

shared_ptr<Screen> ScreenManager::getScreen(const string& name) {
    return screens.count(name) ? screens[name] : nullptr;
}

unordered_map<string, shared_ptr<Screen>> ScreenManager::getAllScreens() const {
    return screens;
}

void ScreenManager::switchScreen(const string& name) {
    if (screens.count(name)) currentScreen = screens[name];
}

shared_ptr<Screen> ScreenManager::getCurrentScreen() {
    return currentScreen;
}

bool ScreenManager::hasScreen(const string& name) const {
    return screens.find(name) != screens.end();
}

void ScreenManager::displayProcessSMI() {
    shared_ptr<Screen> screenPtr = getCurrentScreen();
    if (!screenPtr || screenPtr->getName() == "main") {
        cout << "No process screen selected." << endl;
        return;
    }

    Screen& screen = *screenPtr;
    bool isFinished = screen.getProgramCounter() >= screen.getTotalInstructions();

    cout << CLIController::COLOR_BLUE << "\n=== process-smi for " << screen.getName() << " ===\n" << CLIController::COLOR_RESET;
    cout << "Status        : " << (screen.getIsRunning() ? "Running" : (isFinished ? "Finished" : "Waiting")) << "\n";
    cout << "Assigned Core : " << (screen.getCoreID() != -1 ? to_string(screen.getCoreID()) : "N/A") << "\n";
    // Use new functions: getProgramCounter() and getTotalInstructions()
    cout << "Progress      : " << screen.getProgramCounter() << " / " << screen.getTotalInstructions() << "\n";
    cout << "Created       : " << screen.getTimestamp() << "\n";
    if (isFinished) {
        cout << "Finished      : " << screen.getTimestampFinished() << "\n";
    }
    cout << CLIController::COLOR_BLUE << "========================\n" << CLIController::COLOR_RESET;
}