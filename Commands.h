// Ver: 04-11-2025
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <map>
#include <sys/stat.h>

using namespace std;

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
    // TODO: Add your data members
protected:
    std::string cmd_line;
    pid_t pid;
public:
    Command(const char *cmd_line);

    virtual ~Command();

    virtual void execute() = 0;

    std::string getCmdLine() const;

    void setPid(pid_t pid);

    pid_t getPid() const;

    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char *cmd_line);

    virtual ~BuiltInCommand() {
    }
};

class ExternalCommand : public Command { //ready
public:
    ExternalCommand(const char *cmd_line);

    virtual ~ExternalCommand() {}

    void execute() override;
};

class RedirectionCommand : public Command { //ready
    string input;
    string outputFile;
    int flags;
public:
    explicit RedirectionCommand(const char *cmd_line, const string& input, const string& outputFile, const int& flags);

    virtual ~RedirectionCommand() {}

    void execute() override;
};

class RedirectionOverideCommand : public RedirectionCommand { //ready
public:
    explicit RedirectionOverideCommand(const char *cmd_line, const string& inputFile, const string& outputFile);
};

class RedirectionAppendCommand : public RedirectionCommand { //ready
public:
    explicit RedirectionAppendCommand(const char *cmd_line, const string& inputFile, const string& outputFile);
};


class PipeCommand : public Command { //ready
    // TODO: Add your data members
public:
    PipeCommand(const char *cmd_line);

    virtual ~PipeCommand() {}

    void execute() override;
};

class DiskUsageCommand : public Command { //ready
   static int fileSize(const char* input, const struct stat *pStat, int flag, struct FTW *pFtw);
public:
    DiskUsageCommand(const char *cmd_line);

    virtual ~DiskUsageCommand() {}

    void execute() override;
};

class WhoAmICommand : public Command { //ready
public:
    WhoAmICommand(const char *cmd_line);

    virtual ~WhoAmICommand() {}

    void execute() override;
};

class USBInfoCommand : public Command {
    // TODO: Add your data members **BONUS: 10 Points**
public:
    USBInfoCommand(const char *cmd_line);

    virtual ~USBInfoCommand() {}

    void execute() override;
};

class ChpromptCommand : public BuiltInCommand { // ready (chprompt)
public:
    ChpromptCommand(const char *cmd_line);

    virtual ~ChpromptCommand() {}

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand { //ready (pid)
public:
    ShowPidCommand(const char *cmd_line);

    virtual ~ShowPidCommand() {}

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand { //ready (pwd)
public:
    GetCurrDirCommand(const char *cmd_line);

    virtual ~GetCurrDirCommand() {}

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand { //ready (cd)
    // TODO: Add your data members public:
    char **pLastPwd;
public:
    ChangeDirCommand(const char *cmd_line, char **plastPwd);

    virtual ~ChangeDirCommand() {}

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand { //ready
private:
    JobsList* jobs;
public:
    QuitCommand(const char *cmd_line, JobsList *jobs);

    virtual ~QuitCommand() {}

    void execute() override;
};

class JobsList {
public:
    class JobEntry {
    public:
        int job_id;
        pid_t pid;
        std::string cmd_line;
        bool stopped;

        JobEntry(int job_id, pid_t pid, const std::string& cmd_line, bool stopped);
    };
private:
    std::vector<JobEntry> jobs;

    // TODO: Add your data members
public:
    JobsList();

    ~JobsList();

    void addJob(Command *cmd, bool Stopped = false);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob(int *lastJobId);

    JobEntry *getLastStoppedJob(int *jobId);

    // TODO: Add extra methods or modify exisitng ones as needed
};

class JobsCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    JobsCommand(const char *cmd_line, JobsList *jobs);

    virtual ~JobsCommand() {}

    void execute() override;
};

class KillCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    KillCommand(const char *cmd_line, JobsList *jobs);

    virtual ~KillCommand() {}

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs);

    virtual ~ForegroundCommand() {}

    void execute() override;
};

class AliasCommand : public BuiltInCommand { //ready
    string command;
public:
    AliasCommand(const char *cmd_line);

    virtual ~AliasCommand() {}

    void execute() override;
};

class UnAliasCommand : public BuiltInCommand { //ready
public:
    UnAliasCommand(const char *cmd_line);

    virtual ~UnAliasCommand() {}

    void execute() override;
};

class UnSetEnvCommand : public BuiltInCommand { //ready
    bool isSetEnv(const string& command);

    void removeEnv(const string& command);
public:
    UnSetEnvCommand(const char *cmd_line);

    virtual ~UnSetEnvCommand() {}

    void execute() override;
};

class SysInfoCommand : public BuiltInCommand { //ready
public:
    SysInfoCommand(const char *cmd_line);

    virtual ~SysInfoCommand() {}

    void execute() override;
};

class SmallShell {
private:
    // TODO: Add your data members

    std::string currPrompt;
    pid_t pid;
    char* prevWorkDir;
    JobsList jobs;
    pid_t foreground_pid;
    std::string foreground_cmd;

    string savedCommands[15] = {"chprompt", "showpid", "pwd", "cd", "jobs", "fg", "quit", "kill", "alias", "unalias",
                                            "unsetenv", "sysinfo", "du", "whoami", "usbinfo"}; //an array of the forbidden words to use in alias command
    vector<string> commandsByOrder;
    map<string, string> aliasCommands;

    SmallShell();


public:
    Command *CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    void executeCommand(const char *cmd_line);

    string getCurrPrompt() const;

    void setCurrPrompt(const string& prompt);

    pid_t getPID() const;

    void printCommandsByOrder() const;

    void addAliasCommand(const string& aliasCommand ,const string& command);

    bool isSavedCommands(const string& command) const;

    bool isAliasCommand(const string& command);

    void removeAliasCommand(const string& aliasCommand);

    JobsList& getJobsList();

    pid_t getForegroundPid() const;

    string getForegroundCmd() const;

    bool hasForegroundProcess() const;

    void setForegroundProcess(pid_t pid, const string& cmd);

    void clearForegroundProcess();
    // TODO: add extra methods as needed
};

#endif //SMASH_COMMAND_H_
