# Mini Shell in C - Operating Systems Project for Cybersecurity Engineering

This project is a custom command-line mini shell developed in C, created as part of the Operating Systems course within the Cybersecurity Engineering program. It provides a practical, hands-on approach to understanding core shell functionalities and system-level programming in a Unix-like environment.

## Key Features

- **Command Execution**: Supports execution of standard shell commands, handling both built-in and external commands.
- **Process Control**: Enables background and foreground process handling, showcasing Unix process management.
- **Input/Output Redirection**: Includes support for redirection of standard input/output to enable flexible data handling and piping.
- **Environment Variable Management**: Allows users to set, unset, and view environment variables within the shell session.
- **Error Handling and Debugging**: Provides detailed error messages and debugging information for command execution and I/O operations.
- **Customizable Prompt**: Displays a customizable prompt, similar to standard shells, for an enhanced user experience.

## Installation
1. Clone the repository:
   ```bash
   git clone https://github.com/your-username/mini-shell.git
   ```
2. Navigate to the project directory and compile the code:
   ```bash
   cd mini-shell
   make
   ```
3. Run the mini shell:
   ```bash
   ./mini-shell
   ```
## Usage
Start the shell and use it just like a traditional Unix shell. It supports common commands, process management, and I/O redirection. Here are a few examples:
- **Run a command:**
  ```bash
  figueron@msh:/home/figueron/Desktop$> ls -l
  ```
- **Redirect output:**
  ```bash
  figueron@msh:/home/figueron/Desktop$ echo "Hello, world!" > output.txt
  ```
- **Background process:**
  ```bash
  figueron@msh:/home/figueron/Desktop$> find / name "file.txt" &
  ```
## Learning Objectives
This project aims to:

- Deepen understanding of system calls and process management in Unix.
- Reinforce key concepts of command parsing, error handling, and memory management.
- Illustrate the structure and design of shell applications for cybersecurity contexts.
## Contributing
If you'd like to contribute, please fork the repository and use a feature branch. Pull requests are warmly welcomed.

## License
This project is licensed under the MIT License - see the LICENSE file for details. 

