#include <iostream>
#include "server.h"
#include "log.h"
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>

Server g_server;

static void sig(int signo)
{
        switch (signo)
        {
            case SIGSEGV:
            {
                void* arr[10];
                size_t nSize = backtrace(arr, 10);
                char** strings = backtrace_symbols(arr, nSize);

                pmlLog(pml::LOG_CRITICAL)  << "Segmentation fault, aborting. " << nSize;
                for(size_t i = 0; i < nSize; i++)
                {
                    pmlLog(pml::LOG_CRITICAL)  << std::hex << "0x" << reinterpret_cast<long long>(arr[i]) << "[" << strings[i] << "]";
                }
                free(strings);

                _exit(1);
            }
        case SIGTERM:
        case SIGINT:
	    case SIGQUIT:
            {
                pmlLog(pml::LOG_WARN) << "User abort";
                //g_server.Exit();
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


int main(int argc,  char** argv)
{
    init_signals();

    if(argc < 2)
    {
        std::cout << "Usage: loggermanager [config file full path]" << std::endl;
        return -1;
    }

    auto nResult = g_server.Run(argv[1]);
    return nResult;
}
