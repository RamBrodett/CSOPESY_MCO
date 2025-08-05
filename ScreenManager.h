#pragma once
#include "Screen.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

using namespace std;

class ScreenManager {
public:
	// Singleton Access
	static void initialize();
	static ScreenManager* getInstance();
	static void destroy();

	// Screen Management
	void registerScreen(const string& name, shared_ptr<Screen> screen);
	void switchScreen(const string& name);
	shared_ptr<Screen> getCurrentScreen();
	bool hasScreen(const string& name) const;
	shared_ptr<Screen> getScreen(const string& name);

	// Display Commands
	void displayProcessSMI();
	void displaySystemSmiSummary();
	void displayVmStat();

	std::unordered_map<std::string, shared_ptr<Screen>> getAllScreens() const;

private:
	ScreenManager();
	static ScreenManager* instance;

	// Data Structures
	unordered_map<string, shared_ptr<Screen>> screens;
	unordered_map<string, vector<std::string>> processLogs;
	shared_ptr<Screen> currentScreen;
};