#include <iostream>
#include <unistd.h>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    SmallShell& smash = SmallShell::getInstance();
    if (!smash.hasForegroundProcess())
        return;
    pid_t foreground_pid = smash.getForegroundPid();
    if (kill(foreground_pid, SIGKILL) < 0) {
        perror("smash error: kill failed");
        return;
    }
    cout << "smash: process " << foreground_pid << " was killed" << endl;
}