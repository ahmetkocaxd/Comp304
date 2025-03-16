#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h> // termios, TCSANOW, ECHO, ICANON
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <ctype.h>
#include <libgen.h>
#include <limits.h>

#define BUF_SIZE 4096

const char *sysname = "dash";

enum return_codes {
	SUCCESS = 0,
	EXIT = 1,
	UNKNOWN = 2,
};

struct command_t {
	char *name;
	bool background;
	bool auto_complete;
	int arg_count;
	char **args;
	char *redirects[3]; // in/out redirection
	struct command_t *next; // for piping
};


void kuhex_dump(const char *filename, int group_size) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file");
        return;
    }

    unsigned char buffer[16];
    size_t bytes_read;
    unsigned int offset = 0;

    while ((bytes_read = fread(buffer, 1, 16, file)) > 0) {
        // Print offset
        printf("%08x: ", offset);
        offset += bytes_read;

        // Print hex bytes grouped by group_size
        for (size_t i = 0; i < bytes_read; i++) {
            if (i > 0 && i % group_size == 0) {
                printf(" ");
            }
            printf("%02x", buffer[i]);
        }

        // Pad for shorter lines
        for (size_t i = bytes_read; i < 16; i++) {
            if (i % group_size == 0) {
                printf(" ");
            }
            printf("   ");
        }

        // Print ASCII representation
        printf("  ");
        for (size_t i = 0; i < bytes_read; i++) {
            char c = buffer[i];
            printf("%c", (c >= 32 && c <= 126) ? c : '.');
        }
        printf("\n");
    }

    fclose(file);
}


/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt() {
	char cwd[1024], hostname[1024];
	gethostname(hostname, sizeof(hostname));
	getcwd(cwd, sizeof(cwd));
	printf("%s@%s:%s %s> ", getenv("USER"), hostname, cwd, sysname);
	return 0;
}

/**
 * Helper function to list files in the current directory.
 */
void list_files(const char *prefix, char *buf, size_t *index) {
    FILE *fp = popen("ls", "r"); // Use "ls" to list files in the current directory
    if (fp == NULL) {
        perror("popen");
        return;
    }

    char line[1024];
    int matches = 0;
    char match[1024] = {0};

    while (fgets(line, sizeof(line), fp)) {
        // Remove newline character from the line
        line[strcspn(line, "\n")] = '\0';

        if (strncmp(line, prefix, strlen(prefix)) == 0) {
            matches++;
            if (matches == 1) {
                strcpy(match, line);
            } else {
                printf("\n%s", line);
            }
        }
    }

    pclose(fp);

	

    if (matches == 1) {
        size_t target_len = strlen(prefix);
        snprintf(buf + *index - target_len, sizeof(buf) - *index + target_len, "%s ", match);
        *index = strlen(buf);
        printf("%s", match + target_len); // Print only the completed part
    } else if (matches > 1) {
		printf("\n");
		show_prompt();
        printf("%s", buf); // Reprint the buffer for the user
		
    } else {
        printf("\nNo matches found\n%s", buf);
    }
}

/**
 * Helper function to list commands in PATH and built-in commands.
 */
void list_commands(const char *prefix, char *buf, size_t *index) {
    const char *path = getenv("PATH");
    char *path_copy = strdup(path);
    char *dir = strtok(path_copy, ":");

    const char *builtins[] = {"cd", "exit", NULL};
    int matches = 0;
    char match[1024] = {0};

    // Check built-in commands
    for (int i = 0; builtins[i] != NULL; ++i) {
        if (strncmp(builtins[i], prefix, strlen(prefix)) == 0) {
            matches++;
            if (matches == 1) {
                strcpy(match, builtins[i]);
            } else {
                printf("\n%s", builtins[i]);
            }
        }
    }

    // Check PATH directories for executable files
    while (dir != NULL) {
        FILE *fp;
        char command[1024];
        snprintf(command, sizeof(command), "ls %s", dir);
        fp = popen(command, "r");
        if (fp == NULL) {
            dir = strtok(NULL, ":");
            continue;
        }

        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            // Remove newline character
            line[strcspn(line, "\n")] = '\0';

            if (strncmp(line, prefix, strlen(prefix)) == 0) {
                matches++;
                if (matches == 1) {
                    strcpy(match, line);
                } else {
                    printf("\n%s", line);
                }
            }
        }

        pclose(fp);
        dir = strtok(NULL, ":");
    }

    free(path_copy);

    if (matches <= 2) {
		size_t target_len = strlen(prefix);
		size_t match_len = strlen(match);

		// Append the remaining part of the match to the buffer
		strncpy(buf + *index - target_len, match, match_len);

		// Add a space to signify completion and null-terminate the string
		buf[*index - target_len + match_len] = ' ';
		buf[*index - target_len + match_len + 1] = '\0';

		// Update the index to reflect the new buffer length
		*index = strlen(buf);

		// Print the updated buffer
		printf("\r%s", buf); // Reprint the prompt and the completed command



    } else if (matches > 2) {
		printf("\n");
		show_prompt();
        printf("%s", buf); // Reprint the buffer for the user
    } 
}

/**
 * Handles auto-completion for commands and files.
 */
void auto_complete(char *buf, size_t *index) {
    char *last_token = strrchr(buf, ' '); // Find the last token
    const char *target = last_token ? last_token + 1 : buf;

    // Check if the input matches any known commands or is fully typed
    if (strchr(buf, ' ') == NULL) {
        // If there's no space, assume it's a command and check for matches
        list_commands(target, buf, index);
    } else {
        // If there's a space, assume the command is fully typed and list files
        list_files(target, buf, index);
    }
}

/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command) {
	int i = 0;
	printf("Command: <%s>\n", command->name);
	printf("\tIs Background: %s\n", command->background ? "yes" : "no");
	printf("\tNeeds Auto-complete: %s\n",
		   command->auto_complete ? "yes" : "no");
	printf("\tRedirects:\n");

	for (i = 0; i < 3; i++) {
		printf("\t\t%d: %s\n", i,
			   command->redirects[i] ? command->redirects[i] : "N/A");
	}

	printf("\tArguments (%d):\n", command->arg_count);

	for (i = 0; i < command->arg_count; ++i) {
		printf("\t\tArg %d: %s\n", i, command->args[i]);
	}

	if (command->next) {
		printf("\tPiped to:\n");
		print_command(command->next);
	}
}

/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command) {
	if (command->arg_count) {
		for (int i = 0; i < command->arg_count; ++i)
			free(command->args[i]);
		free(command->args);
	}

	for (int i = 0; i < 3; ++i) {
		if (command->redirects[i])
			free(command->redirects[i]);
	}

	if (command->next) {
		free_command(command->next);
		command->next = NULL;
	}

	free(command->name);
	free(command);
	return 0;
}



/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command) {
	const char *splitters = " \t"; // split at whitespace
	int index, len;
	len = strlen(buf);

	// trim left whitespace
	while (len > 0 && strchr(splitters, buf[0]) != NULL) {
		buf++;
		len--;
	}

	while (len > 0 && strchr(splitters, buf[len - 1]) != NULL) {
		// trim right whitespace
		buf[--len] = 0;
	}

	// auto-complete
	if (len > 0 && buf[len - 1] == '?') {
		command->auto_complete = true;
	}

	// background
	if (len > 0 && buf[len - 1] == '&') {
		command->background = true;
	}

	char *pch = strtok(buf, splitters);
	if (pch == NULL) {
		command->name = (char *)malloc(1);
		command->name[0] = 0;
	} else {
		command->name = (char *)malloc(strlen(pch) + 1);
		strcpy(command->name, pch);
	}

	command->args = (char **)malloc(sizeof(char *));

	int redirect_index;
	int arg_index = 0;
	char temp_buf[1024], *arg;

	while (1) {
		// tokenize input on splitters
		pch = strtok(NULL, splitters);
		if (!pch)
			break;
		arg = temp_buf;
		strcpy(arg, pch);
		len = strlen(arg);

		// empty arg, go for next
		if (len == 0) {
			continue;
		}

		// trim left whitespace
		while (len > 0 && strchr(splitters, arg[0]) != NULL) {
			arg++;
			len--;
		}

		// trim right whitespace
		while (len > 0 && strchr(splitters, arg[len - 1]) != NULL) {
			arg[--len] = 0;
		}

		// empty arg, go for next
		if (len == 0) {
			continue;
		}

		// piping to another command
		if (strcmp(arg, "|") == 0) {
			struct command_t *c = malloc(sizeof(struct command_t));
			int l = strlen(pch);
			pch[l] = splitters[0]; // restore strtok termination
			index = 1;
			while (pch[index] == ' ' || pch[index] == '\t')
				index++; // skip whitespaces

			parse_command(pch + index, c);
			pch[l] = 0; // put back strtok termination
			command->next = c;
			continue;
		}

		// background process
		if (strcmp(arg, "&") == 0) {
			// handled before
			continue;
		}

		// handle input redirection
		redirect_index = -1;
		if (arg[0] == '<') {
			redirect_index = 0;
		}

		if (arg[0] == '>') {
			if (len > 1 && arg[1] == '>') {
				redirect_index = 2;
				arg++;
				len--;
			} else {
				redirect_index = 1;
			}
		}

		if (redirect_index != -1) {
			command->redirects[redirect_index] = malloc(len);
			strcpy(command->redirects[redirect_index], arg + 1);
			continue;
		}

		// normal arguments
		if (len > 2 &&
			((arg[0] == '"' && arg[len - 1] == '"') ||
			 (arg[0] == '\'' && arg[len - 1] == '\''))) // quote wrapped arg
		{
			arg[--len] = 0;
			arg++;
		}

		command->args =
			(char **)realloc(command->args, sizeof(char *) * (arg_index + 1));

		command->args[arg_index] = (char *)malloc(len + 1);
		strcpy(command->args[arg_index++], arg);
	}
	command->arg_count = arg_index;

	// increase args size by 2
	command->args = (char **)realloc(
		command->args, sizeof(char *) * (command->arg_count += 2));

	// shift everything forward by 1
	for (int i = command->arg_count - 2; i > 0; --i) {
		command->args[i] = command->args[i - 1];
	}

	// set args[0] as a copy of name
	command->args[0] = strdup(command->name);

	// set args[arg_count-1] (last) to NULL
	command->args[command->arg_count - 1] = NULL;

	return 0;
}

void prompt_backspace() {
	putchar(8); // go back 1
	putchar(' '); // write empty over
	putchar(8); // go back 1 again
}

/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command) {
	size_t index = 0;
	char c;
	char buf[4096];
	static char oldbuf[4096];

	// tcgetattr gets the parameters of the current terminal
	// STDIN_FILENO will tell tcgetattr that it should write the settings
	// of stdin to oldt
	static struct termios backup_termios, new_termios;
	tcgetattr(STDIN_FILENO, &backup_termios);
	new_termios = backup_termios;
	// ICANON normally takes care that one line at a time will be processed
	// that means it will return if it sees a "\n" or an EOF or an EOL
	new_termios.c_lflag &=
		~(ICANON |
		  ECHO); // Also disable automatic echo. We manually echo each char.
	// Those new settings will be set to STDIN
	// TCSANOW tells tcsetattr to change attributes immediately.
	tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

	show_prompt();
	buf[0] = 0;

	while (1) {
		c = getchar();
		// printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

		// handle tab
		if (c == 9) {
			//buf[index++] = '?'; // autocomplete
			//break;
			
            auto_complete(buf, &index);
            continue;
		}

		// handle backspace
		if (c == 127) {
			if (index > 0) {
				prompt_backspace();
				index--;
			}
			continue;
		}

		if (c == 27 || c == 91) {
			continue;
		}

		// up arrow
		if (c == 65) {
			while (index > 0) {
				prompt_backspace();
				index--;
			}

			char tmpbuf[4096];
			printf("%s", oldbuf);
			strcpy(tmpbuf, buf);
			strcpy(buf, oldbuf);
			strcpy(oldbuf, tmpbuf);
			index += strlen(buf);
			continue;
		}

		putchar(c); // echo the character
		buf[index++] = c;
		if (index >= sizeof(buf) - 1)
			break;
		if (c == '\n') // enter key
			break;
		if (c == 4) // Ctrl+D
			return EXIT;
	}

	// trim newline from the end
	if (index > 0 && buf[index - 1] == '\n') {
		index--;
	}

	// null terminate string
	buf[index++] = '\0';

	strcpy(oldbuf, buf);

	parse_command(buf, command);

	// print_command(command); // DEBUG: uncomment for debugging

	// restore the old settings
	tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
	return SUCCESS;
}

int process_command(struct command_t *command);

int main() {
	while (1) {
		struct command_t *command = malloc(sizeof(struct command_t));

		// set all bytes to 0
		memset(command, 0, sizeof(struct command_t));

		int code;
		code = prompt(command);
		if (code == EXIT) {
			break;
		}

		code = process_command(command);
		if (code == EXIT) {
			break;
		}

		free_command(command);
	}

	printf("\n");
	return 0;
}

int process_command(struct command_t *command) {
	int r;

	if (strcmp(command->name, "") == 0) {
		return SUCCESS;
	}

	if (strcmp(command->name, "exit") == 0) {


		// Check if the kernel module is loaded
		FILE *proc_modules = fopen("/proc/modules", "r");
		if (!proc_modules) {
			perror("Failed to open /proc/modules");
			return EXIT;
		}

		bool module_loaded = false;
		char line[256];
		while (fgets(line, sizeof(line), proc_modules)) {
			if (strstr(line, "mymodule") != NULL) {
				module_loaded = true;
				printf("Removing Kernel Module.\n");
				break;
			}
		}
		fclose(proc_modules);

		// Remove the kernel module if it is loaded and exiting
		if (module_loaded) {
			if (system("sudo rmmod mymodule") != 0) {
				perror("Failed to remove psvis kernel module");
				return EXIT;
			}
		}

		return EXIT;
	}

	if (strcmp(command->name, "cd") == 0) {
		

		if (command->arg_count > 1) {
			r = chdir(command->args[1]);
			if (r == -1) {
				printf("-%s: cd: %s\n", sysname, strerror(errno));
			}
			return SUCCESS;
		} else {
			printf("-%s: cd: missing argument\n", sysname);
			return SUCCESS;
		}
	}

	




	// Handle piping
	if (command->next != NULL) { 
		struct command_t *current_command = command;
		int pipefd[2];
		int prev_pipe_read_end = -1; // To store the read end of the previous pipe
		pid_t pid;

		while (current_command != NULL) {
			if (current_command->next != NULL) {
				// create a pipe for the current command
				if (pipe(pipefd) == -1) {
					perror("Pipe failed");
					return UNKNOWN;
				}
			}

			pid = fork();
			if (pid == 0) { // child process
				
				// if there's a previous pipe, connect its read end to stdin
				if (prev_pipe_read_end != -1) {
					dup2(prev_pipe_read_end, STDIN_FILENO);
					close(prev_pipe_read_end);
				}

				// if there's a next command, connect stdout to the write end of the current pipe
				if (current_command->next != NULL) {
					dup2(pipefd[1], STDOUT_FILENO);
					close(pipefd[1]);
					close(pipefd[0]);
				}

				// execute the current command
				char *path = getenv("PATH");
				char *dir = strtok(path, ":");
				char full_path[1024];
				while (dir != NULL) {
					snprintf(full_path, sizeof(full_path), "%s/%s", dir, current_command->name);
					if (access(full_path, X_OK) == 0) {
						execv(full_path, current_command->args);
						break;
					}
					dir = strtok(NULL, ":");
				}
				perror("Command execution failed");
				exit(EXIT_FAILURE);

			} else if (pid > 0) { // parent process
				
				if (current_command->next != NULL) {
					close(pipefd[1]);
				}

				if (prev_pipe_read_end != -1) {
					close(prev_pipe_read_end);
				}

				// move to the next command and update prev_pipe_read_end
				prev_pipe_read_end = pipefd[0];
				current_command = current_command->next;
			} else {
				perror("Fork failed");
				return UNKNOWN;
			}
		}

		// wait for all child processes to finish
		while (wait(NULL) > 0);
		return SUCCESS;
	}
	
	

	

	pid_t pid = fork();
	// child
	if (pid == 0) {

		

		int fd_read;
		int fd_write;
		int fd_append;

		// open read redirect file if exists
		if (command->redirects[0] != NULL) {
			fd_read = open(command->redirects[0], O_RDONLY);
			if (fd_read == EXIT) {
				perror("failed to open input redirect file");
				exit(EXIT);
			}
		}

		// open write redirect file if exists
		if (command->redirects[1] != NULL) {
			fd_write = open(command->redirects[1], O_WRONLY | O_TRUNC | O_CREAT, 0666);
			if (fd_write == EXIT) {
				perror("failed to open write redirect file");
				exit(EXIT);
			}
		}

		// open append redirect file if exists
		if (command->redirects[2] != NULL) {
			fd_append = open(command->redirects[2], O_WRONLY | O_APPEND | O_CREAT, 0666);
			if (fd_append == EXIT) {
				perror("failed to open append redirect file");
				exit(EXIT);
			}
		}

		// execute command
		pid_t pid2;
		if ((pid2 = fork()) == 0) {

			// Redirections 

			// Reading redirection
			if (command->redirects[0] != NULL) {
				dup2(fd_read, STDIN_FILENO);
				close(fd_read);
			}

			// Overwrite redirection
			if (command->redirects[1] != NULL) {
				dup2(fd_write, STDOUT_FILENO);
				close(fd_write);
			}

			// Append redirection
			if (command->redirects[2] != NULL) {
				dup2(fd_append, STDOUT_FILENO);
				close(fd_append);
			}


			if (strcmp(command->name, "kuhex") == 0) {
				if (command->arg_count < 3) {
					printf("Usage: kuhex <filename> [-g <group_size>]\n");
					return UNKNOWN;
				}

				char *filename = command->args[1];
				int group_size = 1; // Default group size

				if (command->arg_count == 5 && strcmp(command->args[1], "-g") == 0) {
					group_size = atoi(command->args[2]);
					if (group_size != 1 && group_size != 2 && group_size != 4 &&
						group_size != 8 && group_size != 16) {
						printf("Invalid group size. Supported values: 1, 2, 4, 8, 16.\n");
						return UNKNOWN;
					}
					filename = command->args[3];
				}

				kuhex_dump(filename, group_size);
				exit(SUCCESS);
			} 



			else if (strcmp(command->name, "psvis") == 0) {
				if (command->arg_count < 3) {
					printf("Usage: psvis <PID> <output_file>\n");
					return UNKNOWN;
				}

				const char *pid = command->args[1];
				const char *output_file = command->args[2];

				// check if the kernel module is loaded
				FILE *proc_modules = fopen("/proc/modules", "r");
				if (!proc_modules) {
					perror("Failed to open /proc/modules");
					return UNKNOWN;
				}

				bool module_loaded = false;
				char line[256];
				while (fgets(line, sizeof(line), proc_modules)) {
					if (strstr(line, "mymodule") != NULL) {
						module_loaded = true;
						printf("Kernel Module is already loaded.\n");
						break;
					}
				}
				fclose(proc_modules);


				
				char path_to_module[PATH_MAX];
				char src_dir[PATH_MAX];
				ssize_t len = readlink("/proc/self/exe", path_to_module, sizeof(path_to_module) - 1);
				if (len != -1) {
					path_to_module[len] = '\0'; // null terminate

					
					strncpy(src_dir, path_to_module, sizeof(src_dir) - 1);
					src_dir[sizeof(src_dir) - 1] = '\0'; // null terminate

					char *directory = dirname(src_dir); // get the directory of the current executable
					
					// construct the full path of mymodule.ko 
					snprintf(path_to_module, sizeof(path_to_module), "%s/module/mymodule.ko", directory);

					// if kernel module is not loaded, load it.
					if (!module_loaded) {
						char insmod_command[PATH_MAX + 20];
						snprintf(insmod_command, sizeof(insmod_command), "sudo insmod %s", path_to_module);
						if (system(insmod_command) != 0) {
							perror("Failed to load psvis kernel module");
							return UNKNOWN;
						}
					}
				} else {
					perror("Failed to determine the path to the executable");
					return UNKNOWN;
				}


				// write the PID to /proc/psvis as parameter
				FILE *proc_psvis = fopen("/proc/psvis", "w");
				if (!proc_psvis) {
					perror("Failed to open /proc/psvis for writing");
					return UNKNOWN;
				}

				fprintf(proc_psvis, "%s", pid); 
				fclose(proc_psvis);

				
				proc_psvis = fopen("/proc/psvis", "r");
				if (!proc_psvis) {
					perror("Failed to open /proc/psvis for reading");
					return UNKNOWN;
				}

				// create a DOT file
				FILE *dot_file = fopen("process_tree.dot", "w");
				if (!dot_file) {
					perror("Failed to create DOT file");
					fclose(proc_psvis);
					return UNKNOWN;
				}

				
				fprintf(dot_file, "digraph ProcessTree {\n");
				fprintf(dot_file, "node [shape=ellipse];\n");

				// read the kernel module output and write it to the DOT file
				char linee[256];
				while (fgets(linee, sizeof(linee), proc_psvis)) {
					fprintf(dot_file, "%s", linee);
				}

				fprintf(dot_file, "}\n");
				fclose(proc_psvis);
				fclose(dot_file);

				// Graphviz operations to generate the PNG image
				char graphviz_command[256];
				snprintf(graphviz_command, sizeof(graphviz_command), "dot -Tpng process_tree.dot -o %s", output_file);
				if (system(graphviz_command) != 0) {
					perror("Failed to generate graph image using Graphviz");
					return UNKNOWN;
				}

				printf("Process tree visualization saved to %s\n", output_file);
				exit(SUCCESS);
			}
			
			
			
			else {

				char *path = getenv("PATH");
				char *dir = strtok(path, ":");
				char full_path[1024];
				while (dir != NULL) {
					snprintf(full_path, sizeof(full_path), "%s/%s", dir, command->name);
					if (access(full_path, X_OK) == 0) {
						execv(full_path, command->args); 
						break;
					}
					dir = strtok(NULL, ":");
				}
				printf("-%s: %s: command not found\n", sysname, command->name);
				exit(EXIT_FAILURE);
			}
		}

		// Parent waits for pid2 to finish
		waitpid(pid2, NULL, 0);

		// Close file descriptors
		close(fd_read);
		close(fd_write);
		close(fd_append);

		exit(SUCCESS);

	} else {
		// TODO: implement background processes here
		if (command->background) {
			// don't wait for the child process
			printf("[%d] Running in background\n", pid);
			return SUCCESS;

		}
		else if (!command->background) {
			wait(0); // wait for child process to finish
			return SUCCESS;
		}


		
	}

	// TODO: your implementation here

	printf("-%s: %s: command not found\n", sysname, command->name);
	return UNKNOWN;
}