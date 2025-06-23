#pragma once

#include "Screen.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

class ScreenManager {
public:
	ScreenManager();
	void registerScreen(const std::string& name, std::shared_ptr<Screen> screen);
	void switchScreen(const std::string& name);
	std::shared_ptr<Screen> getCurrentScreen();
	bool hasScreen(const std::string& name) const;

	std::shared_ptr<Screen> getScreen(const std::string& name);
	void displayProcessSMI();

	static void initialize();
	static ScreenManager* getInstance();
	static void destroy();

	std::unordered_map<std::string, std::shared_ptr<Screen>> getAllScreens() const;



private:
	static ScreenManager* instance;
	std::unordered_map<std::string, std::shared_ptr<Screen>> screens;
	std::unordered_map<std::string, std::vector<std::string>> processLogs;
	std::shared_ptr<Screen> currentScreen;
};