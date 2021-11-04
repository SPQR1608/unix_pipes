#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

void split(const string& str, vector<string> &tokens,
           const string &delimiters = " ")
{
    // Skip delimiters at beginning.
    string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    string::size_type pos     = str.find_first_of(delimiters, lastPos);

    while (string::npos != pos || string::npos != lastPos)
    {
        // Found a token, add it to the vector.
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }
}

inline string trim(string &str)
{
    const string whitespaces(" \t\f\v\n\r");

    string::size_type pos = str.find_first_not_of(whitespaces);
    if(pos != string::npos)
        str.erase(0, pos);   // prefixing spaces

    pos = str.find_last_not_of(whitespaces);
    if(pos != string::npos)
        str.erase(pos + 1);    // surfixing spaces

    return str;
}

void parse_command(string &command, string &name, string &argc)
{
    command = trim(command);

    string::size_type pos = command.find_first_of(' ');
    if(pos != string::npos) {
        name = command.substr(0, pos);
        argc = command.substr(pos + 1, command.length() - pos - 1);
    } else {
        name = command;
        argc = "";
    }
}

void exec_command(uint n, vector<string> &commands)
{
    string name, args;
    parse_command(commands[n], name, args);
    if(args.length() > 0)
        execlp(name.c_str(), name.c_str(), args.c_str(), NULL);
    else
        execlp(name.c_str(), name.c_str(), NULL);
}

// who ----(stdout)---> pfd[1] --- pfd[0] ----(stdin)---> wc -l
void execute_line(vector<string> &commands, size_t i, int *parent_pfd = 0)
{
    int pfd[2];
    pipe(pfd);
    if(i > 0 && !fork()) {
        // Child
        if(i >= 1) {
            execute_line(commands, i-1, pfd);
            close(pfd[1]);
            close(pfd[0]);
        } else {
            printf("Deeper child %d: %s, parent_pfd[0]=%d, parent_pfd[1]=%d, "
                   "pfd[0]=%d, pfd[1]=%d\n",
                   getpid(), trim(commands[i-1]).c_str(),
                   parent_pfd[0], parent_pfd[1], pfd[0], pfd[1]);

            close(STDOUT_FILENO);
            dup2(pfd[1], STDOUT_FILENO);        // Copy STDOUT to pipe out

            close(pfd[1]);
            close(pfd[0]);

            exec_command(i - 1, commands);
        }
    } else {
        if(parent_pfd) {
            printf("Middle child %d: %s, parent_pfd[0]=%d, parent_pfd[1]=%d, "
                   "pfd[0]=%d, pfd[1]=%d\n",
                   getpid(), trim(commands[i]).c_str(), parent_pfd[0], parent_pfd[1],
                   pfd[0], pfd[1]);

            close(STDIN_FILENO);
            dup2(pfd[0], STDIN_FILENO);         // Copy STDIN to pipe in

            close(STDOUT_FILENO);
            dup2(parent_pfd[1], STDOUT_FILENO); // Copy STDOUT to parent pipe out

            close(pfd[1]);
            close(pfd[0]);

            exec_command(i, commands);
        } else {
            printf("Final %d: %s, pfd=%p, parent_pfd=%p, pfd[0]=%d, pfd[1]=file\n",
                   getpid(), trim(commands[i]).c_str(), pfd, parent_pfd, pfd[0]);
            int fd = open("result.out", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            dup2(fd, STDOUT_FILENO);            // Copy stdout to file
            dup2(pfd[0], STDIN_FILENO);         // Copy STDIN to pipe in
            close(pfd[0]);  // Close as was redirected
            close(pfd[1]);  // Close WRITE as not necessary here
            close(fd);

            exec_command(i, commands);
        }
    }
}

int main()
{
    char buffer[1024];
    ssize_t size = read(STDIN_FILENO, buffer, 1024);

    if(size > 0) {
        buffer[size] = '\0';
        string command = buffer;
        vector<string> commands;
        split(command, commands, "|");
        execute_line(commands, commands.size() - 1);
    }
    return 0;
}
