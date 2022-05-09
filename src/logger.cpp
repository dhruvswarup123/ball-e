#include "logger.h"
#include <nanogui/nanogui.h>
#include <iostream>
#include <sstream>
using namespace nanogui;
using namespace std;

// Warnings
void Logger::warn(const string &s)
{
    cout << "[warning] ";
    cout << s << endl;
    
    if (textbox != nullptr)
    {
        textbox->setValue("[warning] " + s);
    }
}

// Errors
void Logger::error(const string &s)
{
    cout << "[!!! ERROR !!!] ";
    cout << s << endl;
    if (textbox != nullptr)
    {
        textbox->setValue("[!!! ERROR !!!]" + s);
    }
}

// Any general info / debugging
void Logger::info(const string &s)
{
    cout << "[info] ";
    cout << s << endl;
    if (textbox != nullptr)
    {
        textbox->setValue("[info] " + s);
    }
}

// Completed tasks
void Logger::done(const string &s)
{
    cout << "[done] ";
    cout << s << endl;
    if (textbox != nullptr)
    {
        textbox->setValue("[done] " + s);
    }
}