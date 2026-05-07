#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>

#include <unistd.h> // for fork(), exec(), and dup2()
#include <sys/wait.h>   // for waitpid()
#include <fcntl.h>  // for open()

namespace fs = std::filesystem;

bool executeProgram(const std::string& command) {

    // If the input is not one of the shell's built in commands,
    // assume the user is trying to execute a program.
    //
    // fork() creates a second process.
    //
    // pid == 0:
    // We are inside the child process.
    // The child process will be replaced by the requested program using execl().
    //
    // pid > 0:
    // We are inside the parent process.
    // The value of pid is the process ID of the child.
    // The parent keeps the shell running and waits for the child to finish.
    //
    // pid < 0:
    // fork() failed and no child process was created.
    //
    // Without fork(), execl() would permanently replace the shell itself.

    pid_t pid = fork();

    if (pid == 0) {

        // execl replaces the current child process with the new program.
        // If execl succeeds, the remaining code in this block never runs.
        // execl only returns if an error occurs.

        if (execl(command.c_str(),
                  command.c_str(),
                  static_cast<char*>(nullptr)) == -1) {

            std::cout << "There was an error when trying to exec"
                      << std::endl;

            std::cout << "Program could not be found."
                      << std::endl;

            exit(1);
            // exit(1) immediately terminates the child process
            // and returns a nonzero status code to the parent process.
        }
    }
    else if (pid > 0) {

        int status;

        // waitpid pauses the parent process until the child process finishes.
        waitpid(pid, &status, 0);

        // WIFEXITED checks whether the child process ended normally.
        // If true, WEXITSTATUS extracts the program's exit code.
        if (WIFEXITED(status)) {

            int code = WEXITSTATUS(status);

            if (code != 0) {

                std::cout << "Program exited with status code "
                          << code << std::endl;

                return false;
            }

            return true;
        }
    }
    else {

        std::cout << "Fork failed." << std::endl;
        return false;
    }

    return false;
}

int main() {
    fs::path workingDir{"/"};
    // chdir changes the actual operating system working directory
    // of the shell process so child programs inherit the same directory.
    chdir(workingDir.c_str());
    std::string input;
    std::vector<std::string> history;

    std::cout << "Welcome to ElbeeShell\n\n";

    while (true) {
        std::cout << workingDir.string() << " $ ";
        std::getline(std::cin, input);

        // find space
        size_t spacePos = input.find(' ');
        std::string command;
        std::string argument;

        if (spacePos == std::string::npos) {
            command = input;
        } else {
            command = input.substr(0, spacePos);
            argument = input.substr(spacePos + 1);
        }

        // commands
        if (command == "exit") {
            break;
        }

        else if (command == "workdir") {
            std::cout << "Working directory: " << workingDir.string() << std::endl;
            history.push_back(std::move(input));
        }
        else if (command == "cd") {
            if (argument.empty()) {
                std::cout << "No path provided." << std::endl;
            } else {
                try {
                    fs::path newPath;

                    if (argument[0] == '/') {
                        // absolute path
                        newPath = fs::canonical(argument);
                    } else {
                        // relative path
                        newPath = fs::canonical(workingDir / argument);
                    }

                    workingDir = newPath;
                    chdir(workingDir.c_str());
                    history.push_back(std::move(input));
                }
                catch (fs::filesystem_error &) {
                    std::cout << "Path does not exist." << std::endl;
                }
            }
        }
        else if (command == "list") {
            std::vector<fs::path> directories;
            std::vector<fs::path> files;

            try {
                for (const auto &entry : fs::directory_iterator(workingDir)) {
                    if (entry.is_directory()) {
                        directories.push_back(entry.path().filename());
                    }
                    else if (entry.is_regular_file()) {
                        files.push_back(entry.path().filename());
                    }
                }

                // std::sort uses iterators to define a range
                // begin() returns an iterator to the first element
                // end() returns an iterator one past the last element
                // the range [begin, end) includes all elements in the vector
                // iterators behave like pointers, so you can dereference them
                // *directories.begin() gives the first element
                std::sort(directories.begin(), directories.end());
                std::sort(files.begin(), files.end());

                for (const auto &d : directories) {
                    std::cout << d.string() << " (Directory)" << std::endl;
                }

                for (const auto &f : files) {
                    std::cout << f.string() << std::endl;
                }
                history.push_back(std::move(input));
            }
            catch (fs::filesystem_error &) {
                std::cout << "Error accessing directory." << std::endl;
            }
        }

        // using reverse iterators!!
        else if (command == "history") {
            for (auto it = history.rbegin(); it != history.rend(); ++it) {
                std::cout << *it << std::endl;
            }
        }

        else {
            if (executeProgram(command)) {
                history.push_back(std::move(input));
            }
        }

        std::cout << std::endl;
    }

    return 0;
}