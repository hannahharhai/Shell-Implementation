#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>


typedef struct {
    char *plugin_path;
    char **argv;
    void *handle;
}Plugins;


void printCommandPrompt() {
    printf("> ");
    fflush(stdout);
}

char* getUserInput() {
    char *input = calloc(201, sizeof(char));
    fgets(input, 201, stdin);

    // remove newline character from string
    int length = strlen(input);
    int end = length-1;
    input[end] = '\0';

    return input;
}

char** parseCommand(char *input) {
    char **cmd = calloc(201, sizeof(char));
    // retrieve first substring from user input
    char* substr = strtok(input, " ");

    /* assign 1st substring to the 0th position in cmd array, then
    loop over the rest of user input string splitting them into seperate substrings 
    with strtok and assigning them their relative positions in cmd array */
    int idx = 1;
    cmd[0] = substr;
    while((substr = strtok(NULL, " "))) {
        cmd[idx++] = substr;
    }
    return cmd;
}

int isBuiltIn(char **cmd) {
    // check if command is a built in
    if ((strcmp(cmd[0], "exit") == 0) || (strcmp(cmd[0], "load") == 0)) {
        return 1; 
    } else {
        return 0;
    }
}

char* trimString(char* plugin_path) {
    // trim string to contain only the plugin name, not extensions (./ or .so)
    char *temp = calloc(20, sizeof(char));
    int length = strlen(plugin_path);
    strncpy(temp, (plugin_path+2), length-5);

    return temp;
}

int cmdLength(char **cmd) {
    // find length of cmd
    int length = 0;
    for(int i = 0; cmd[i] != NULL; i++) {
        length++;
    }
    return length;
}

int ifPluginIsLoaded(Plugins* plugin, char **cmd) {
    // check if plugin has been loaded
    for (int i = 0; plugin[i].plugin_path != NULL; i++) {
        char *temp = trimString(plugin[i].plugin_path);
        if (strcmp(temp, cmd[1]) == 0) {
            free(temp);
            return 1;
        }
    }
    // if not loaded 
    return 0;
}

// function ptrs to .so file functions
static int (*initialize)();
static int (*run)(char **argv);

void runBuiltIn(char *input, char **cmd, Plugins* plugin) {
    // if cmd is exit then exit the program
    if(strcmp(cmd[0], "exit") == 0) {     
        freeMem(input, cmd, plugin);
        exit(0);
    } else {
        // find length of cmd
        int length = cmdLength(cmd);

        // if command only = load, print an error
        if (length == 1) {
            fprintf(stdout, "Error: Plugin some_string initialization failed!\n");
        } else {
            // check if plugin has already been loaded
            if (ifPluginIsLoaded(plugin, cmd)) {
                fprintf(stdout, "Error: Plugin %s initialization failed!\n", cmd[1]);
            } else {
                // a new plugin has been provided, initialize plugin
                initPlugin(plugin, cmd);
            }
        }
    }
}


void initPlugin(Plugins* plugin, char **cmd) { 
    int count = 0;
    // allocate space for plugin data
    plugin[count].plugin_path = calloc(20, sizeof(char));
    plugin[count].handle = calloc(1, sizeof(void*));

    // loading a plugin, append characters to create correct file path for .so file
    strcat(plugin[count].plugin_path, "./");
    strcat(plugin[count].plugin_path, cmd[1]);
    strcat(plugin[count].plugin_path, ".so");

    // open .so file, if it fails print an error message
    plugin[count].handle = dlopen(plugin[count].plugin_path, RTLD_LAZY);
    if (!plugin[count].handle) {
        fprintf(stdout, "Error: Plugin %s initialization failed!\n", cmd[1]);
    } else {
        // if .so file opens successfully, load initialize function and run it
        initialize = dlsym(plugin[count].handle, "initialize");
        int init = initialize();
        // if init fails print error message and close .so file
        if (init) {
            fprintf(stdout, "Error: Plugin %s initialization failed!\n", cmd[1]);
            dlclose(plugin[count].handle);
        }
    }
    count++;
}


int isPluginInvoked(Plugins* plugin, char **cmd) {
    // check is plugin is being called/invoked
    for (int i = 0; plugin[i].plugin_path != NULL; i++) {
        char *temp = trimString(plugin[i].plugin_path);
        if (strcmp(temp, cmd[0]) == 0) {
            free(temp);
            return 1;
        }
    }
    return 0;
}

Plugins searchPlugin(Plugins* plugin, char **cmd) {
    // find desired plugin to run
    for (int i = 0; plugin[i].plugin_path != NULL; i++) {
        char *temp = trimString(plugin[i].plugin_path);
        if (strcmp(temp, cmd[0]) == 0) {
            free(temp);
            return plugin[i];
        }
    }
}

void runPlugin(Plugins* plugin, char **cmd) {

    // find plugin to be run
    Plugins foundPlugin = searchPlugin(plugin, cmd);

        // allocate space for plugin args
        foundPlugin.argv = calloc(20, sizeof(char*));

        int argLength = cmdLength(cmd);
             
        run = dlsym(foundPlugin.handle, "run");
        
        // loop over rest of user cmd and append args to argument string
        for(int i = 0; cmd[i] != NULL; i++) {
            foundPlugin.argv[i] = cmd[i];
        }
        // append NULL to end of argument string then run 
        foundPlugin.argv[argLength] = NULL;
        run(foundPlugin.argv);
    }

void freeMem(char *input, char **cmd, Plugins* plugin) {
    free(input);
    free(cmd);
    
    for (int i = 0; plugin[i].plugin_path != NULL; i++) {
        free(plugin[i].plugin_path);
        free(plugin[i].argv);
        // free(plugin[i].handle);
        dlclose(plugin[i].handle);
    }
    free(plugin);
}

int main() {

    Plugins* plugin = calloc(10, sizeof(plugin));

    while(1) {

        printCommandPrompt();
        char *input = getUserInput();
        char **cmd = parseCommand(input);

        if (isBuiltIn(cmd)) {
            runBuiltIn(input, cmd, plugin);
        } else { 
            if (isPluginInvoked(plugin, cmd)) {
                runPlugin(plugin, cmd);
            } else {
                int length = cmdLength(cmd);
                // append null to args for exec
                cmd[length] = NULL;
                
                pid_t pid = fork();
                if (pid != 0) {
                    wait(NULL);
                } else {
                    execvp(cmd[0], cmd);
                }
            }
        }

        free(input);
        free(cmd);
    }
}

// gcc -Wall -g -std=c99 -o msh msh.c
