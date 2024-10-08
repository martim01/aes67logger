#include <iostream>
#include "flacencoder.h"
#include "log.h"
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>
#include "jsonconsts.h"
#include "jsonwriter.h"
#include "flacencoder_version.h"
#include "backtr.h"
FlacEncoder theApp;       //The main loop - declared global so we can access it from signal handler and exit gracefully


static void sig(int signo)
{
    switch (signo)
    {
        case SIGSEGV:
        {
            print_back_trace();

            _exit(1);
        }
    case SIGTERM:
    case SIGINT:
    case SIGQUIT:
        {
            pmlLog(pml::LOG_WARN, "aes67") << "User abort";
            theApp.Exit();
            _exit(0);
        }
        break;
    default:
        pmlLog(pml::LOG_DEBUG, "aes67") << "Signal: " <<signo;
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
    jsValue[jsonConsts::git][jsonConsts::rev] = pml::flacencoder::GIT_REV;
    jsValue[jsonConsts::git][jsonConsts::tag] = pml::flacencoder::GIT_TAG;
    jsValue[jsonConsts::git][jsonConsts::branch] = pml::flacencoder::GIT_BRANCH;
    jsValue[jsonConsts::git][jsonConsts::date] = pml::flacencoder::GIT_DATE;
    jsValue[jsonConsts::major] = pml::flacencoder::VERSION_MAJOR;
    jsValue[jsonConsts::minor] = pml::flacencoder::VERSION_MINOR;
    jsValue[jsonConsts::patch] = pml::flacencoder::VERSION_PATCH;
    jsValue[jsonConsts::version] = pml::flacencoder::VERSION_STRING;
    jsValue[jsonConsts::date] = pml::flacencoder::BUILD_TIME;

    JsonWriter::Get().writeToStdOut(jsValue);
}

int main(int argc,  char** argv)
{
    if(argc == 1)
    {
        std::string sArg(argv[1]);

        if(sArg == "-v" || sArg == "--version")
        {
            OutputVersion();
            return 0;
        }
    }

    if(argc < 4)
    {
        std::cout << "Usage: flacencoder.exe [name wav_path encoded_path socket_path]" << std::endl;
        return -1;
    }
        
        
    init_signals();
    init_back_trace(argv[0]);
    theApp.Init(argv[1], argv[2], argv[3], argv[4], -1);    //@todo think about the logging
    return theApp.Run();
}


