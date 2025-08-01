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

//void ScreenManager::displayProcessSMI() {
//    shared_ptr<Screen> screenPtr = getCurrentScreen();
//    if (!screenPtr || screenPtr->getName() == "main") {
//        cout << "No process screen selected." << endl;
//        return;
//    }
//
//    Screen& screen = *screenPtr;
//    bool isFinished = screen.isFinished();
//
//    cout << "Process name: " << screen.getName() << endl;
//    cout << "Logs:" << endl;
//
//    // Use the getter to access logs safely
//    for (const auto& line : screen.getOutputBuffer()) {
//        cout << line << endl;
//    }
//
//    cout << endl;
//    if (isFinished) {
//        cout << endl << "Finished!" << endl;
//    }
//    else {
//        cout << "Current instruction line: " << screen.getProgramCounter() << endl;
//        cout << "Lines of code: " << screen.getTotalInstructions() << endl;
//    }
//
//    
//}