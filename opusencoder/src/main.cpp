#include <iostream>
#include "opusencoder.h"
#include "log.h"
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>
#include "jsonconsts.h"
#include "jsonwriter.h"
#include "opusencoder_version.h"

OpusEncoder theApp;       //The main loop - declared global so we can access it from signal handler and exit gracefully


static void sig(int signo)
{
    switch (signo)
    {
        case SIGSEGV:
        {
            void* arr[10];
            auto nSize = backtrace(arr, 10);
            char** strings = backtrace_symbols(arr, nSize);

            pmlLog(pml::LOG_CRITICAL)  << "Segmentation fault, aborting. " << nSize;
            for(auto i = 0; i < nSize; i++)
            {
                pmlLog(pml::LOG_CRITICAL)  << std::hex << "0x" << reinterpret_cast<long long>(arr[i]) << "[" << strings[i] << "]";
            }
            free(strings); //backtrace_symbols calls malloc so we must free

            _exit(1);
        }
    case SIGTERM:
    case SIGINT:
    case SIGQUIT:
        {
            pmlLog(pml::LOG_WARN) << "User abort";
            theApp.Exit();
            _exit(0);
        }
        break;
    }
}

void init_signals()
{
    struct sigaction new_action;
    struct sigaction old_action;
    new_action.sa_handler = sig;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;

    sigaction(SIGTERM, nullptr, &old_action);
    if(old_action.sa_handler != SIG_IGN)
    {
        sigaction(SIGTERM, &new_action, nullptr);
    }

    sigaction(SIGINT, nullptr, &old_action);
    if(old_action.sa_handler != SIG_IGN)
    {
        sigaction(SIGINT, &new_action, nullptr);
    }
    sigaction(SIGSEGV, nullptr, &old_action);
    if(old_action.sa_handler != SIG_IGN)
    {
        sigaction(SIGSEGV, &new_action, nullptr);
    }
    sigaction(SIGQUIT, nullptr, &old_action);
    if(old_action.sa_handler != SIG_IGN)
    {
        sigaction(SIGQUIT, &new_action, nullptr);
    }

}

void OutputVersion()
{
    Json::Value jsValue;
    jsValue[jsonConsts::git][jsonConsts::rev] = pml::opusencoder::GIT_REV;
    jsValue[jsonConsts::git][jsonConsts::tag] = pml::opusencoder::GIT_TAG;
    jsValue[jsonConsts::git][jsonConsts::branch] = pml::opusencoder::GIT_BRANCH;
    jsValue[jsonConsts::git][jsonConsts::date] = pml::opusencoder::GIT_DATE;
    jsValue[jsonConsts::major] = pml::opusencoder::VERSION_MAJOR;
    jsValue[jsonConsts::minor] = pml::opusencoder::VERSION_MINOR;
    jsValue[jsonConsts::patch] = pml::opusencoder::VERSION_PATCH;
    jsValue[jsonConsts::version] = pml::opusencoder::VERSION_STRING;
    jsValue[jsonConsts::date] = pml::opusencoder::BUILD_TIME;

    JsonWriter::Get().writeToStdOut(jsValue);
}

int main(int argc,  char** argv)
{
    if(argc < 2)
    {
        std::cout << "Usage: OpusEncoder.exe [full path to config file]" << std::endl;
        return -1;
    }
    std::string sArg(argv[1]);
    if(sArg == "-v" || sArg == "--version")
    {
        OutputVersion();
        return 0;
    }
    else
    {
        init_signals();
        theApp.Init(argv[1]);
        return theApp.Run();
    }
}


