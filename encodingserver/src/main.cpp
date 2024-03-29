#include <iostream>
#include "encodingserver.h"
#include "log.h"
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>
#include "backtr.h"
EncodingServer g_server;

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
                //g_server.Exit();
                _exit(0);
            }
	    break;
        }

}

void init_signals()
{
    struct sigaction new_action, old_action;
    new_action.sa_handler = sig;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;

    sigaction(SIGTERM, NULL, &old_action);
    if(old_action.sa_handler != SIG_IGN)
    {
        sigaction(SIGTERM, &new_action, NULL);
    }

    sigaction(SIGINT, NULL, &old_action);
    if(old_action.sa_handler != SIG_IGN)
    {
        sigaction(SIGINT, &new_action, NULL);
    }
    sigaction(SIGSEGV, NULL, &old_action);
    if(old_action.sa_handler != SIG_IGN)
    {
        sigaction(SIGSEGV, &new_action, NULL);
    }
    sigaction(SIGQUIT, NULL, &old_action);
    if(old_action.sa_handler != SIG_IGN)
    {
        sigaction(SIGQUIT, &new_action, NULL);
    }

}


int main(int argc,  char** argv)
{
    

    if(argc < 2)
    {
        std::cout << "Usage: playbackserver [config file full path]" << std::endl;
        return -1;
    }
    init_signals();
    init_back_trace(argv[0]);

    auto nResult = g_server.Run(argv[1]);
    return nResult;
}
