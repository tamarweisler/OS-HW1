#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <cerrno>
#include <iomanip>
#include <cstdlib>
#include <signal.h>
#include <cctype>
#include "Commands.h"

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;
    FUNC_EXIT()
}

bool _isBackgroundCommand(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h 

Command::Command(const char* cmd_line): pid(-1){
    if (cmd_line == nullptr)
        this->cmd_line = "";
    else
        this->cmd_line = cmd_line;
}

Command::~Command() {}

std::string Command::getCmdLine() const {return cmd_line;}

void Command::setPid(pid_t pid) {this->pid = pid;}

pid_t Command::getPid() const {return pid;}

BuiltInCommand::BuiltInCommand(const char* cmd_line): Command(cmd_line){}

ExternalCommand::ExternalCommand(const char *cmd_line): Command(cmd_line){}

void ExternalCommand::execute() {
    bool background_flag = _isBackgroundCommand(cmd_line.c_str());
    char* args[COMMAND_MAX_ARGS + 1];
    bool complex_flag = false, free_flag = false, exit_flag = false;
    int count_args = 0;
    std::string cmd_to_exe = cmd_line;
    if (background_flag) {
        std::vector<char> cmd_buffer;
        for (char c:cmd_to_exe)
            cmd_buffer.push_back(c);
        cmd_buffer.push_back('\0');
        _removeBackgroundSign(cmd_buffer.data());
        cmd_to_exe = _trim(std::string(cmd_buffer.data()));
    }
    for (char c: cmd_to_exe)
        if (c == '?' || c == '*')
                complex_flag = true;
    for (int i = 0; i < COMMAND_MAX_ARGS + 1; i++)
        args[i] = nullptr;
    if (!complex_flag) {
        count_args = _parseCommandLine(cmd_to_exe.c_str(), args);
        if (count_args == 0)
            return;
    }
    pid_t child_pid = fork();
    if (child_pid < 0) {
        perror("smash error: fork failed");
        free_flag = true;
    }
    if (child_pid == 0) {
        if (setpgrp() < 0)
            perror("smash error: setpgrp failed");
        else if (complex_flag) {
            execl("/bin/bash", "bash", "-c", cmd_to_exe.c_str(), (char*)nullptr);
            perror("smash error: execl failed");
            exit(1);
        }
        else {
            execvp(args[0], args);
            perror("smash error: execvp failed");
        }
        free_flag = true;
        exit_flag = true;
    }
    SmallShell& smash = SmallShell::getInstance();
    if (free_flag || background_flag) {
        if (background_flag) {
            setPid(child_pid);
            smash.getJobsList().addJob(this, false);
        }
        for (int i = 0; i < count_args; i++) {
            free(args[i]);
            args[i] = nullptr;
        }
        if (exit_flag)
            exit(1);
        return;
    }
    setPid(child_pid);
    smash.setForegroundProcess(child_pid, cmd_line);
    int status = 0;
    while (true) {
        pid_t wait_result = waitpid(child_pid, &status, 0);
        if (wait_result == child_pid)
            break;
        if (wait_result < 0) {
            if (errno == EINTR)
                continue;
            perror("smash error: waitpid failed");
            break;
        }
    }
    smash.clearForegroundProcess();
    for (int i = 0; i < count_args; i++) {
        free(args[i]);
        args[i] = nullptr;
    }
}

PipeCommand::PipeCommand(const char* cmd_line): Command(cmd_line) {}

void PipeCommand::execute() {
    std::string full_cmd = cmd_line;
    int position = 0;
    bool pipe_flag = false;
    for (; position < full_cmd.length(); position++) {
        if (full_cmd[position] == '|') {
            if (full_cmd[position + 1] == '&')
                pipe_flag = true;
            break;
        }
    }
    if (position == full_cmd.length())
        return;
    std::string first_cmd_str;
    std::string second_cmd_str;
    if (pipe_flag) {
        first_cmd_str = full_cmd.substr(0, position);
        second_cmd_str = full_cmd.substr(position + 2);
    } else {
        first_cmd_str = full_cmd.substr(0, position);
        second_cmd_str = full_cmd.substr(position + 1);
    }
    first_cmd_str = _trim(first_cmd_str);
    second_cmd_str = _trim(second_cmd_str);
    int pipe_fds[2];
    int pipe_result = pipe(pipe_fds);
    if (pipe_result == -1) {
        perror("smash error: pipe failed");
        return;
    }
    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("smash error: fork failed");
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return;
    }
    if (pid1 == 0) {
        if (setpgrp() < 0) {
            perror("smash error: setpgrp failed");
            close(pipe_fds[0]);
            close(pipe_fds[1]);
            exit(1);
        }
        int dup_result = -1;
        if (pipe_flag)
            dup_result = dup2(pipe_fds[1], STDERR_FILENO);
        else
            dup_result = dup2(pipe_fds[1], STDOUT_FILENO);
        if (dup_result < 0) {
            perror("smash error: dup2 failed");
            close(pipe_fds[0]);
            close(pipe_fds[1]);
            exit(1);
        }
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        SmallShell& smash = SmallShell::getInstance();
        Command* first_cmd = smash.CreateCommand(first_cmd_str.c_str());
        if (first_cmd != nullptr) {
            first_cmd->execute();
            delete first_cmd;
        }
        exit(0);
    }
    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("smash error: fork failed");
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        int status = 0;
        while (true) {
            pid_t wait_result = waitpid(pid1, &status, 0);
            if (wait_result == pid1)
                break;
            if (wait_result < 0) {
                if (errno == EINTR)
                    continue;
                perror("smash error: waitpid failed");
                break;
            }
        }
        return;
    }
    if (pid2 == 0) {
        if (setpgrp() < 0) {
            perror("smash error: setpgrp failed");
            close(pipe_fds[0]);
            close(pipe_fds[1]);
            exit(1);
        }
        if (dup2(pipe_fds[0], STDIN_FILENO) < 0) {
            perror("smash error: dup2 failed");
            close(pipe_fds[0]);
            close(pipe_fds[1]);
            exit(1);
        }
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        SmallShell& smash = SmallShell::getInstance();
        Command* second_cmd = smash.CreateCommand(second_cmd_str.c_str());
        if (second_cmd != nullptr) {
            second_cmd->execute();
            delete second_cmd;
        }
        exit(0);
    }
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    int status1 = 0;
    while (true) {
        pid_t wait_result = waitpid(pid1, &status1, 0);
        if (wait_result == pid1)
            break;
        if (wait_result < 0) {
            if (errno == EINTR)
                continue;
            perror("smash error: waitpid failed");
            break;
        }
    }
    int status2 = 0;
    while (true) {
        pid_t wait_result = waitpid(pid2, &status2, 0);
        if (wait_result == pid2)
            break;
        if (wait_result < 0) {
            if (errno == EINTR)
                continue;
            perror("smash error: waitpid failed");
            break;
        }
    }
}

JobsList::JobEntry::JobEntry(int job_id, pid_t pid, const std::string &cmd_line, bool stopped):
    job_id(job_id), pid(pid), cmd_line(cmd_line), stopped(stopped){}

JobsList::JobsList() {}

JobsList::~JobsList() {}

SmallShell::SmallShell(): jobs(), foreground_pid(-1){}

JobsList& SmallShell::getJobsList() {return jobs;}

pid_t SmallShell::getForegroundPid() const {return foreground_pid;}

std::string SmallShell::getForegroundCmd() const {return foreground_cmd;}

bool SmallShell::hasForegroundProcess() const {return (foreground_pid > 0);}

void SmallShell::setForegroundProcess(pid_t pid, const std::string& cmd) {
    foreground_pid = pid;
    foreground_cmd = cmd;
}

void SmallShell::clearForegroundProcess() {
    foreground_pid = -1;
    foreground_cmd = "";
}
void JobsList::removeFinishedJobs() {
    std::vector<JobEntry> active_jobs;
    for (JobEntry& job : jobs) {
        pid_t result = waitpid(job.pid, nullptr, WNOHANG);
        if (result == 0) {
            active_jobs.push_back(job);
            continue;
        }
        else if (result == job.pid || (result == -1 && errno == ECHILD)) {
            continue;
        }
        else if (result == -1) {
            perror("smash error: waitpid failed");
            active_jobs.push_back(job);
            continue;
        }
        active_jobs.push_back(job);
    }
    jobs = active_jobs;
}

void JobsList::addJob(Command *cmd, bool Stopped) {
    removeFinishedJobs();
    if (cmd == nullptr)
        return;
    int new_job_id = 1;
    for (JobEntry& job : jobs)
        if (job.job_id >= new_job_id)
            new_job_id = job.job_id + 1;
    pid_t job_pid = cmd->getPid();
    if (job_pid <= 0)
        return;
    std::string job_cmd = cmd->getCmdLine();
    JobEntry new_job(new_job_id, job_pid, job_cmd, Stopped);
    jobs.push_back(new_job);
}

void JobsList::printJobsList() {
    removeFinishedJobs();
    for (JobEntry& job : jobs)
        cout << "[" << job.job_id << "] " << job.cmd_line << endl;
}

JobsList::JobEntry *JobsList::getJobById(int jobId) {
    removeFinishedJobs();
    for (JobEntry& job : jobs)
        if (job.job_id == jobId)
            return &job;
    return nullptr;
}

void JobsList::removeJobById(int jobId) {
    std::vector<JobEntry> new_jobs;
    for (JobEntry& job : jobs) {
        if (job.job_id == jobId) {
            continue;
        }
        new_jobs.push_back(job);
    }
    jobs = new_jobs;
}

JobsList::JobEntry *JobsList::getLastJob(int *lastJobId) {
    removeFinishedJobs();
    if (jobs.empty()) {
        if (lastJobId != nullptr)
            *lastJobId = -1;
        return nullptr;
    }
    JobEntry* last_job = nullptr;
    int max_job_id = -1;
    for (JobEntry& job : jobs){
        if (job.job_id > max_job_id) {
            max_job_id = job.job_id;
            last_job = &job;
        }
    }
    if (lastJobId != nullptr)
        *lastJobId = max_job_id;
    return last_job;
}

JobsList::JobEntry *JobsList::getLastStoppedJob(int *jobId) {
    removeFinishedJobs();
    JobEntry* last_stopped_job = nullptr;
    int max_stopped_id = -1;
    for (JobEntry& job : jobs) {
        if (!job.stopped)
            continue;
        if (job.job_id > max_stopped_id) {
            max_stopped_id = job.job_id;
            last_stopped_job = &job;
        }
    }
    if (jobId != nullptr)
        *jobId = max_stopped_id;
    return last_stopped_job;
}

void JobsList::killAllJobs() {
    removeFinishedJobs();
    cout << "smash: sending SIGKILL signal to " << jobs.size() << " jobs:" << endl;
    for (JobEntry& job : jobs) {
        cout << job.pid << ": " << job.cmd_line << endl;
        int kill_result = kill(job.pid, SIGKILL);
        if (kill_result < 0)
            perror("smash error: kill failed");
    }
    jobs.clear();
}

JobsCommand::JobsCommand(const char *cmd_line, JobsList *jobs): BuiltInCommand(cmd_line), jobs(jobs) {}

void JobsCommand::execute() {
    if (jobs == nullptr)
        return;
    jobs->printJobsList();
}

ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs): BuiltInCommand(cmd_line), jobs(jobs) {}

void ForegroundCommand::execute() {
    if (jobs == nullptr)
        return;
    std::stringstream stream(cmd_line);
    std::string cmd_name;
    std::string arg1;
    std::string arg2;
    stream >> cmd_name;
    stream >> arg1;
    stream >> arg2;
    if (arg2 != "") {
        cerr << "smash error: fg: invalid arguments" << endl;
        return;
    }
    JobsList::JobEntry* job = nullptr;
    int job_id = -1;
    if (arg1 == "") {
        job = jobs->getLastJob(&job_id);
        if (job == nullptr) {
            cerr << "smash error: fg: jobs list is empty" << endl;
            return;
        }
    } else {
        for (char c : arg1) {
            if (!isdigit(c)) {
                cerr << "smash error: fg: invalid arguments" << endl;
                return;
            }
        }
        job_id = std::stoi(arg1);
        job = jobs->getJobById(job_id);
        if (job == nullptr) {
            cerr << "smash error: fg: job-id " << job_id << " does not exist" << endl;
            return;
        }
    }
    pid_t job_pid = job->pid;
    std::string job_cmd = job->cmd_line;
    cout << job_cmd << " " << job_pid << endl;
    jobs->removeJobById(job_id);
    SmallShell& smash = SmallShell::getInstance();
    smash.setForegroundProcess(job_pid, job_cmd);
    int status = 0;
    while (true) {
        pid_t wait_result = waitpid(job_pid, &status, 0);
        if (wait_result == job_pid)
            break;
        if (wait_result < 0) {
            if (errno == EINTR)
                continue;
            perror("smash error: waitpid failed");
            break;
        }
    }
    smash.clearForegroundProcess();
}

KillCommand::KillCommand(const char *cmd_line, JobsList *jobs): BuiltInCommand(cmd_line), jobs(jobs) {}

void KillCommand::execute() {
    if (jobs == nullptr)
        return;
    std::stringstream stream(cmd_line);
    std::string cmd_name;
    std::string signal_arg;
    std::string id_arg;
    std::string leftover_arg;
    stream >> cmd_name;
    stream >> signal_arg;
    stream >> id_arg;
    stream >> leftover_arg;
    if (signal_arg == "" || id_arg == "" || leftover_arg != "" ||
        signal_arg[0] != '-' || signal_arg.length() == 1) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    for (int i = 1; i < signal_arg.length(); i++) {
        if (!isdigit(signal_arg[i])) {
            cerr << "smash error: kill: invalid arguments" << endl;
            return;
        }
    }
    for (char c : id_arg) {
        if (!isdigit(c)) {
            cerr << "smash error: kill: invalid arguments" << endl;
            return;
        }
    }
    int signal_num = std::stoi(signal_arg.substr(1));
    int job_id = std::stoi(id_arg);
    JobsList::JobEntry* job = jobs->getJobById(job_id);
    if (job == nullptr) {
        cerr << "smash error: kill: job-id " << job_id << " does not exist" << endl;
        return;
    }
    pid_t job_pid = job->pid;
    int kill_result = kill(job_pid, signal_num);
    if (kill_result < 0) {
        perror("smash error: kill failed");
        return;
    }
    cout << "signal number " << signal_num << " was sent to pid " << job_pid << endl;
    if (signal_num == SIGKILL) {
        int status = 0;
        while (true) {
            pid_t wait_result = waitpid(job_pid, &status, 0);
            if (wait_result == job_pid)
                break;
            if (wait_result < 0) {
                if (errno == EINTR)
                    continue;
                if (errno != ECHILD)
                    perror("smash error: waitpid failed");
                break;
            }
        }
        jobs->removeJobById(job_id);
        return;
    }
    if (signal_num == SIGSTOP)
        job->stopped = true;
    if (signal_num == SIGCONT)
        job->stopped = false;
}

QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs): BuiltInCommand(cmd_line), jobs(jobs) {}

void QuitCommand::execute() {
    std::stringstream stream(cmd_line);
    std::string cmd_name;
    std::string arg1;
    stream >> cmd_name;
    stream >> arg1;
    if (arg1 == "kill" && jobs != nullptr)
            jobs->killAllJobs();
    exit(0);
}

SmallShell::~SmallShell() {
    // TODO: add your implementation
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    std::string cmd_trimmed = _trim(std::string(cmd_line));
    for (char c: cmd_trimmed) {
        if (c == '|')
            return new PipeCommand(cmd_line);
    }
    if (_isBackgroundCommand(cmd_trimmed.c_str())) {
        std::vector<char> cmd_buffer;
        for (char c : cmd_trimmed)
            cmd_buffer.push_back(c);
        cmd_buffer.push_back('\0');
        _removeBackgroundSign(cmd_buffer.data());
        cmd_trimmed = _trim(std::string(cmd_buffer.data()));
    }
    size_t first_space = cmd_trimmed.find_first_of(WHITESPACE);
    std::string first_word;
    if (first_space == std::string::npos)
        first_word = cmd_trimmed;
    else
        first_word = cmd_trimmed.substr(0, first_space);
    if (first_word == "jobs")
        return new JobsCommand(cmd_trimmed.c_str(), &jobs);
    if (first_word == "fg")
        return new ForegroundCommand(cmd_trimmed.c_str(), &jobs);
    if (first_word == "kill")
        return new KillCommand(cmd_trimmed.c_str(), &jobs);
    if (first_word == "quit")
        return new QuitCommand(cmd_trimmed.c_str(), &jobs);
    return new ExternalCommand(cmd_line);
}

void SmallShell::executeCommand(const char *cmd_line) {
    if (cmd_line == nullptr)
        return;
    std::string cmd_trimmed = _trim(std::string(cmd_line));
    if (cmd_trimmed.empty())
        return;
    jobs.removeFinishedJobs();
    Command* cmd = CreateCommand(cmd_line);
    if (cmd == nullptr)
        return;
    cmd->execute();
    delete cmd;
}
