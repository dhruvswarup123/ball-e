#include "logger.h"
#include <nanogui/nanogui.h>
#include <iostream>
#include <sstream>
using namespace nanogui;
using namespace std;

// Warnings
void Logger::warn(const string &s)
{
    ss << "[warning] ";
    ss << s << endl;
    
    if (textbox != nullptr)
    {
        textbox->setValue(ss.str());
    }
}

// Errors
void Logger::error(const string &s)
{
    ss << "[!!! ERROR !!!] ";
    ss << s << endl;
    if (textbox != nullptr)
    {
        textbox->setValue(ss.str());
    }
}

// Any general info / debugging
void Logger::info(const string &s)
{
    ss << "[info] ";
    ss << s << endl;
    if (textbox != nullptr)
    {
        textbox->setValue(ss.str());
    }
}

// Completed tasks
void Logger::done(const string &s)
{
    ss << "[done] ";
    ss << s << endl;
    if (textbox != nullptr)
    {
        textbox->setValue(ss.str());
    }
}