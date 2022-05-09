#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <sstream>
#include <nanogui/nanogui.h>
using namespace std;
using namespace nanogui;

struct Logger
{
	/******************************
	 * Constructor/Destructor     *
	 ******************************/
	Logger(TextBox *textbox) : textbox(textbox) {};
	~Logger() = default;
	TextBox* textbox = nullptr;
	stringstream ss;

	
	// Warnings
	void warn(const string &s);

	// Errors
	void error(const string &s);

	// Any general info / debugging
	void info(const string &s);

	// Completed tasks
	void done(const string &s);
};

#endif /* LOGGER_H */
