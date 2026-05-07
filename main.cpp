#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <sstream>

#include <unistd.h> // for fork(), exec(), and dup2()
#include <sys/wait.h>   // for waitpid()
#include <fcntl.h>  // for open()

namespace fs = std::filesystem;

void redirectStandardOutput(const std::string& outputFile) {

    // Open the output file for writing.
    //
    // O_WRONLY:
    // open for writing only
    //
    // O_CREAT:
    // create the file if it does not exist
    //
    // O_TRUNC:
    // clear the file contents if it already exists
    int fd = open(outputFile.c_str(),
                  O_WRONLY | O_CREAT | O_TRUNC,
                  0644);

    // open returns a negative value if the file could not be opened.
    if (fd < 0) {

        std::cout << "Could not open output file."
                  << std::endl;

        exit(1);
    }

    // dup2 redirects standard output and standard error
    // so anything printed by the child program
    // goes into the output file instead of the terminal.
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);

    // The file descriptor is no longer needed after dup2.
    close(fd);
}

bool executeProgram(const std::string& input) {

    bool redirectOutput{false};
    std::string outputFile;

    // Use a string stream to separate the user input
    // into individual space separated words.
    //
    // Example:
    // "/bin/ls -a -l"
    //
    // becomes:
    // "/bin/ls"
    // "-a"
    // "-l"
    std::istringstream stream(input);

    std::string programPath;

    // Stores command line arguments like:
    // "-a", "-l", etc.
    std::vector<std::string> programArguments;

    std::string argument;

    // The first word in the stream is always the program path.
    stream >> programPath;

    // Continue reading arguments until the stream is empty.
    while (!stream.eof()) {

        stream >> argument;

        // Skip empty reads.
        if (argument.empty()) {
            continue;
        }

        // If we encounter ">", the next value is the output file.
        if (argument == ">") {

            redirectOutput = true;

            stream >> outputFile;

            break;
        }

        // Otherwise this is a normal command line argument.
        programArguments.push_back(argument);

        argument.clear();
    }

    // execv is a C function.
    // It expects arguments as a dynamic array of char pointers.
    //
    // +2 because:
    // cargs[0] = program path
    // last index = nullptr
    char** cargs {new char*[programArguments.size() + 2]};

    // The first argument must always be the program itself.
    //
    // Example:
    // "/bin/ls"
    cargs[0] = programPath.data();

    // Fill the remaining array with pointers
    // to each argument string.
    //
    // Example:
    // "-a"
    // "-l"
    for (size_t i = 0; i < programArguments.size(); ++i) {

        cargs[i + 1] = programArguments[i].data();
    }

    // execv argument arrays must always end with nullptr.
    cargs[programArguments.size() + 1] = nullptr;

    // fork() creates a second process.
    pid_t pid = fork();

    // pid == 0 means we are inside the child process.
    if (pid == 0) {

        // If the user requested output redirection,
        // redirect stdout and stderr to the output file.
        if (redirectOutput) {
            redirectStandardOutput(outputFile);
        }

        // execv replaces the child process with the requested program.
        //
        // Parameter 1:
        // path to the program
        //
        // Parameter 2:
        // array of argument character pointers
        if (execv(programPath.c_str(), cargs) == -1) {

            std::cout << "There was an error when trying to exec"
                      << std::endl;

            std::cout << "Program could not be found."
                      << std::endl;

            // exit(1) immediately terminates the child process
            // and returns a nonzero status code to the parent.
            exit(1);
        }
    }

    // pid > 0 means we are inside the parent process.
    else if (pid > 0) {

        int status;

        // waitpid pauses the parent process
        // until the child process finishes.
        waitpid(pid, &status, 0);

        // The parent no longer needs the dynamic array.
        delete[] cargs;

        // WIFEXITED checks whether the child ended normally.
        // WEXITSTATUS extracts the child's exit code.
        if (WIFEXITED(status)) {

            int code = WEXITSTATUS(status);

            // Nonzero status codes usually indicate failure.
            if (code != 0) {

                std::cout << "Program exited with status code "
                          << code << std::endl;

                return false;
            }

            return true;
        }
    }

    // pid < 0 means fork() failed.
    else {

        std::cout << "Fork failed." << std::endl;

        delete[] cargs;

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
            if (executeProgram(input)) {
                history.push_back(std::move(input));
            }
        }

        std::cout << std::endl;
    }

    return 0;
}