#include <iostream>
#include <filesystem>
#include <vector>

#include <unistd.h> // for fork(), exec(), and dup2()
#include <wait.h>   // for waitpid()
#include <fcntl.h>  // for open()

namespace fs = std::filesystem;

int main() {
    fs::path workingDir{"/"};
    std::string input;

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
            }
            catch (fs::filesystem_error &) {
                std::cout << "Error accessing directory." << std::endl;
            }
        }
        else {
            std::cout << "Unknown command." << std::endl;
        }

        std::cout << std::endl;
    }

    return 0;
}
