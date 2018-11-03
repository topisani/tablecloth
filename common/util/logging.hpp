#pragma once

#include <cassert>
#include <iostream>
#include <fmt/format.h>
#include <fmt/ostream.h>

#define LOGD(...) std::cout << __FILE__ << ":" << __LINE__ << "[DEBUG]: " << ::fmt::format(__VA_ARGS__) << std::endl
#define LOGI(...) std::cout << __FILE__ << ":" << __LINE__ << " [INFO]: " << ::fmt::format(__VA_ARGS__) << std::endl
#define LOGE(...) std::cerr << __FILE__ << ":" << __LINE__ << "  [ERR]: " << ::fmt::format(__VA_ARGS__) << std::endl

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/prctl.h>

inline void print_trace() {
    char pid_buf[30];
    sprintf(pid_buf, "%d", getpid());
    char name_buf[512];
    name_buf[readlink("/proc/self/exe", name_buf, 511)]=0;
    prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
    int child_pid = fork();
    if (!child_pid) {           
        dup2(2,1); // redirect output to stderr
        fprintf(stdout,"stack trace for %s pid=%s\n",name_buf,pid_buf);
        execlp("gdb", "gdb", "--batch", "-n", "-ex", "thread", "-ex", "bt", name_buf, pid_buf, NULL);
        abort(); /* If gdb failed to start */
    } else {
        waitpid(child_pid,NULL,0);
    }
}

#define LOG_TRACE() ::print_trace()
