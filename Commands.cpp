#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <fcntl.h>
#include <regex>
#include <sys/utsname.h>

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

bool _isBackgroundComamnd(const char *cmd_line) {
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
    // if the command line does not end with and then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h

Command::Command(const char *cmd_line) : cmd_line(cmd_line){}

std::string Command::getCmdLine() const {
    return this->cmd_line;
}

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line){}

ChpromptCommand::ChpromptCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, char **plastPwd) : BuiltInCommand(cmd_line), pLastPwd(plastPwd) {}

AliasCommand::AliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

UnAliasCommand::UnAliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

UnSetEnvCommand::UnSetEnvCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

SysInfoCommand::SysInfoCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void ChpromptCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int numArgs = _parseCommandLine(this->getCmdLine().c_str(), args);

    if (numArgs == 0) {
        return;
    }
    if (numArgs == 1) {
        SmallShell::getInstance().setCurrPrompt("smash");
    }else {
        SmallShell::getInstance().setCurrPrompt(args[1]);
    }
}

void ShowPidCommand::execute() {
    pid_t pid = SmallShell::getInstance().getPid();
    if (pid != -1) {
        std::cout << "smash pid is " << pid << std::endl ;
    }
}

void GetCurrDirCommand::execute() {
    char buff[PATH_MAX];

    if (getcwd(buff, PATH_MAX) != nullptr) {
        std::cout << buff << std::endl ;
    }
}

void ChangeDirCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int numArgs = _parseCommandLine(this->getCmdLine().c_str(), args);

    if (numArgs < 2) {
        return;
    }

    if (numArgs > 2) {
        std::cerr << "smash error: cd: too many arguments" << std::endl ;
        return;
    }

    if (strcmp(args[1], "-") == 0 && *this->pLastPwd == nullptr) {
        std::cerr << "smash error: cd: OLDPWD not set" << std::endl ;
        return;
    }

    char* buff = new char[PATH_MAX];
    char* temp = getcwd(buff, PATH_MAX);

    if (temp != nullptr) {
        if (strcmp(args[1], "-") == 0) {
            if (*this->pLastPwd != nullptr && chdir(*this->pLastPwd) != -1) { //here we take chdir of the prev dir
                *this->pLastPwd = temp;
                return;
            }
        }else {
            if (chdir(args[1]) != -1) { //here we take chdir of the excepted arg
                *this->pLastPwd = temp;
                return;
            }
        }
    }

    perror("smash error: chdir failed");
}

///////////////////////////////////////////////////////////////////////////////////////////

void AliasCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int numArgs = _parseCommandLine(this->getCmdLine().c_str(), args);

    if (numArgs == 1) { // printing all the alias commands from the list
        SmallShell::getInstance().printCommandsByOrder();
        return;
    }

    std::smatch vars;
    string cmdLineClean = _trim(this->getCmdLine());

    if (!std::regex_match(cmdLineClean, vars, std::regex(R"(^alias ([a-zA-Z0-9_]+)='([^']*)'$)"))) {
        std::cerr << "smash error: alias: invalid alias format" << std::endl ;
        return;
    }


    if (SmallShell::getInstance().isSavedCommands(vars[1].str()) || SmallShell::getInstance().isAliasCommand(vars[1].str())) {
        std::cerr << "smash error: alias: " << vars[1].str() << "already exists or is reserved command" << std::endl ;
        return;
    }

    SmallShell::getInstance().addAliasCommand(vars[1].str(), vars[2].str());
}


void UnAliasCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int numArgs = _parseCommandLine(this->getCmdLine().c_str(), args);

    if (numArgs == 1) {
        std::cerr << "smash error: unalias: not enough arguments"<< std::endl;
        return;
    }

    for (int i = 1; i < numArgs; i++) {
        if (!SmallShell::getInstance().isAliasCommand(args[i])) {
            std::cerr << "smash error: unalias: " << args[i] << " alias does not exist"<< std::endl;
            return;
        }

        SmallShell::getInstance().removeAliasCommand(args[i]);
    }
}

void UnSetEnvCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int numArgs = _parseCommandLine(this->getCmdLine().c_str(), args);

    if (numArgs == 1) {
        std::cerr << "smash error: unsetenv: not enough arguments" << std::endl;
        return;
    }

    for (int i = 1; i < numArgs; i++) {
        if (!SmallShell::getInstance().isSetEnv(args[i])) {
            std::cerr << "smash error: unsetenv: " << args[i] << " does not exist" << std::endl;
            return;
        }
        SmallShell::getInstance().removeEnv(args[i]);
    }
}

void SysInfoCommand::execute() {
    SmallShell::getInstance().printSysInfo();
}


//////////////////////////////////////////////////////////////////////////////////////////
SmallShell::SmallShell() {
    this->currPrompt = "smash";
    this->pid = getpid();
    this->prevWorkDir = nullptr;

}

SmallShell::~SmallShell() {
    // TODO: add your implementation
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    firstWord = sliceInput(firstWord);

    if (firstWord.compare("chprompt") == 0) {
        return new ChpromptCommand(cmd_line);
    }

    if (firstWord.compare("showpid") == 0) {
        return new ShowPidCommand(cmd_line);
    }

    if (firstWord.compare("pwd") == 0) {
        return new GetCurrDirCommand(cmd_line);
    }

    if (firstWord.compare("cd") == 0) {
        return new ChangeDirCommand(cmd_line, &this->prevWorkDir);
    }

    if (firstWord.compare("alias") == 0) {
        return new AliasCommand(cmd_line);
    }

    if (firstWord.compare("unalias") == 0) {
        return new UnAliasCommand(cmd_line);
    }

    if (firstWord.compare("unsetenv") == 0) {
        return new UnSetEnvCommand(cmd_line);
    }

    if (firstWord.compare("sysinfo") == 0) {
        return new SysInfoCommand(cmd_line);
    }



    // For example:
    /*
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    if (firstWord.compare("pwd") == 0) {
      return new GetCurrDirCommand(cmd_line);
    }
    else if (firstWord.compare("showpid") == 0) {
      return new ShowPidCommand(cmd_line);
    }
    else if ...
    .....
    else {
      return new ExternalCommand(cmd_line);
    }
    */
    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    // for example:
    Command* cmd = CreateCommand(cmd_line);
    if (cmd != nullptr) {
        cmd->execute();
        delete cmd;
    }

    // Please note that you must fork a smash process for some commands (e.g., external commands....)
}



std::string SmallShell::getCurrPrompt() const {
    return this->currPrompt;
}

void SmallShell::setCurrPrompt(const std::string &prompt) {
    this->currPrompt = prompt;
}

pid_t SmallShell::getPid() const {
    return this->pid;
}

std::string SmallShell::sliceInput(const string& input) {
    std::string command = "";
    int i = 0;

    if (input.size() == 0) {
        return "";
    }

    while (i < input.size()) {
        if (input[i] == ' ' || input[i] == '&' || input[i] == '|' || input[i] == '<') {
            break;
        }
        command += input[i];
        i++;
    }
    return command;
}

void SmallShell::printCommandsByOrder() const {
    for (int i = 0; i < this->commandsByOrder.size(); i++) {
        std::cout << commandsByOrder[i] << std::endl;
    }
}

void SmallShell::addAliasCommand(const string aliasCommand, const string sCommand) {
    std::string command(sCommand);
    if (!this->aliasCommands.insert({aliasCommand, command}).second) {
        perror("smash error: alias failed");
    }else {
        std::string aliasToList = aliasCommand + "=" + "'" + sCommand + "'";
        this->commandsByOrder.push_back(aliasToList);
    }
}

bool SmallShell::isSavedCommands(const std::string command) {
    for(int i = 0; i < 7; i++){
        if(command == this->savedCommands[i]) { //the alias name conflicts with reserved keyword
            return true;
        }
    }
    return false;
}

bool SmallShell::isAliasCommand(const std::string command) {
    if (this->aliasCommands.find(command) != aliasCommands.end()) { //the alias name conflicts with existing alias
        return true;
    }
    return false;
}

void SmallShell::removeAliasCommand(const std::string aliasCommand) {
    this->aliasCommands.erase(aliasCommand);
    string toDelete = aliasCommand + "=";

    for (auto it = this->commandsByOrder.begin(); it != this->commandsByOrder.end(); it++) {
        if (it->find(toDelete) == 0) {
            this->commandsByOrder.erase(it);
            return;
        }
    }
}

bool SmallShell::isSetEnv(const string command) {
    string toFind = "/proc/" + to_string(this->pid) + "/environ";
    int fd = open(toFind.c_str(), O_RDONLY);
    if (fd == -1) {
        perror("smash error: open failed");
        return false;
    }

    char buffer[4096];
    ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
    if (bytesRead == -1) {
        perror("smash error: read failed");
        close(fd);
        return false;
    }

    string cmdToFind = command + "=";
    int i = 0;
    while (i < bytesRead) {
        string setEnv = "";

        while (buffer[i] != '\0') {
            setEnv += buffer[i];
            i++;
        }

        if (setEnv.find(cmdToFind) == 0) {
            close(fd);
            return true;
        }
        i++;
    }
    close(fd);
    return false;
}


void SmallShell::removeEnv(const string command) {
    string cmdToDelete = command + "=";
    int i = 0;
    while (environ[i] != nullptr) {
        if (strncmp(environ[i], cmdToDelete.c_str(), cmdToDelete.size()) == 0) {
            int j = i;
            while (environ[j] != nullptr) {
                environ[j] = environ[j + 1];
                j++;
            }
            return;
        }
        i++;
    }
}

void SmallShell::printSysInfo() const {
    utsname sys;
    if (uname(&sys) == -1) {
        perror("smash error: uname failed");
        return;
    }

    string OSName = sys.sysname;
    string hostname = sys.nodename;
    string kernelReleaseAndVersion = sys.release;
    string architecture = sys.machine;

    int fd = open("/proc/uptime", O_RDONLY);
    if (fd == -1) {
        perror("smash error: open failed");
        return;
    }

    char buffer[128];
    ssize_t bytesRead = read(fd, buffer, sizeof(buffer) - 1);
    if (bytesRead == -1) {
        perror("smash error: read failed");
        close(fd);
        return;
    }
    close(fd);
    buffer[bytesRead] = '\0';
    double uptime = atof(buffer);
    time_t current_time = time(NULL);
    time_t boot_time = current_time - (time_t)uptime;

    tm * timeinfo = localtime(&boot_time);
    char time_str[80];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);

    std::cout << "System: " << OSName << std::endl;
    std::cout << "Hostname: " << hostname << std::endl;
    std::cout << "Kernel: " << kernelReleaseAndVersion << std::endl;
    std::cout << "Architecture: " << architecture << std::endl;
    std::cout << "Boot Time: " << time_str << std::endl;
}
























