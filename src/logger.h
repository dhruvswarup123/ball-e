#ifndef LOGGER_H
#define	LOGGER_H

#include <iostream>
using namespace std;

struct Logger {
	/******************************
	* Constructor/Destructor     *
	******************************/
	Logger() {};
	~Logger() = default;

	// Warnings
	static void warn(const string& s) {
		cout << "[warning] ";
		cout << s << endl;
	}

	// Errors
	static void error(const string& s) {
		cout << "[!!! ERROR !!!] ";
		cout << s << endl;
	}

	// Any general info / debugging
	static void info(const string& s) {
		cout << "[info] ";
		cout << s << endl;
	}

	// Completed tasks
	void done(const string& s) {
		cout << "[done] ";
		cout << s << endl;
	}
};


#endif	/* LOGGER_H */
