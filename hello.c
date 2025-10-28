#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>

#define MAX_PROCS 4096  // Assumption for max processes
#define TASK_COMM_LEN 16

typedef struct {
    pid_t pid;
    pid_t ppid;
    unsigned long starttime;  // From /proc/pid/stat (22nd field, jiffies since boot)
    long vmrss;               // From /proc/pid/status (VmRSS: in kB, multiply by 1024 for bytes)
    unsigned long cputime;    // utime + stime from /proc/pid/stat (14th + 15th fields)
    char state;               // From /proc/pid/stat (3rd field, e.g., 'T' for stopped)
    time_t creation_time;     // Converted human-readable time (use get_boot_time() + starttime / HZ)
} ProcInfo;

// Dynamic list (array) of ProcInfo
typedef struct {
    ProcInfo* items;
    int count;
    int capacity;
} ProcList;

ProcInfo procs[MAX_PROCS];
int num_procs = 0;

typedef struct ChildNode {
    int proc_index;
    struct ChildNode *next;
} ChildNode;

ChildNode *children[MAX_PROCS] = {0};

// Implement list helpers
void init_list(ProcList* list) {
    list->capacity = 16;
    list->items = (ProcInfo*)malloc(sizeof(ProcInfo) * list->capacity);
    list->count = 0;
}
void add_to_list(ProcList* list, ProcInfo info) {
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->items = (ProcInfo*)realloc(list->items, sizeof(ProcInfo) * list->capacity);
    }
    list->items[list->count++] = info;
}
void free_list(ProcList* list) {
    free(list->items);
    list->items = NULL;
    list->count = list->capacity = 0;
}

struct ChildNode *children[MAX_PROCS];  // Indexed by proc index

void handle_bcp() {
    printf("Handling bcp\n");
}
void handle_bop() {
    printf("Handling bop\n");
}

// Helpers to find index by pid
int find_proc_index(pid_t pid) {
    for (int i = 0; i < num_procs; i++) {
        if (procs[i].pid == pid) return i;
    }
    return -1;
}

// Helper to get ppid
pid_t get_ppid(pid_t pid) {
    int index = find_proc_index(pid);
    if (index != -1) return procs[index].ppid;
    return -1;
}

// Helper to check if process belongs to subtree
bool check_process_at_root(pid_t root, pid_t target) {
    pid_t current = target;
    while (current != 0 && current != 1) {
        if (current == root) return true;
        current = get_ppid(current);
    }
    return false;
}

// 1. depth of process_id
int handle_dpt(pid_t root, pid_t target) {
    int depth = 0;
    pid_t current = target;
    while (current != root && current != 0) {
        current = get_ppid(current);
        depth = depth + 1;
    }
    (current == root) ? return depth : return -1;
}
// 2. all processes at the same level as process_id
// Collect descendants recursively using child links (excludes self)
void collect_descendants(int parent_index, ProcList* descendants) {
    ChildNode *child = children[parent_index];
    while (child != NULL) {
        add_to_list(descendants, procs[child->proc_index]);
        collect_descendants(child->proc_index, descendants);
        child = child->next;
    }
}
void collect_all_in_tree(pid_t root, ProcList* tree) {
    ProcInfo root_info;
    add_to_list(tree, root_info); 
    collect_descendants(root, tree);
}
void handle_lvl(pid_t root_process, pid_t process_id) {
    int target_depth = handle_dpt(root_process, process_id);
    ProcList tree;
    init_list(&tree);
    collect_all_in_tree(root_process, &tree);

    int count = 0;
    for (int i = 0; i < tree.count; i++) {
        pid_t curr_pid = tree.items[i].pid;
        int curr_depth = handle_dpt(root_process, curr_pid);
        if (curr_depth == target_depth) {
            count = count + 1;
        }
    }

    printf("No. of processes at the same depth of %d in the process tree: %d\n", process_id, count);
    free_list(&tree);
}

// 3. count of all descendents of process_id
void handle_cnt(pid_t root_process, pid_t process_id) {
    ProcList descendants;
    init_list(&descendants);
    collect_descendants(target, &descendants);
    printf("%d\n", descendants.count);
    free_list(&descendants);
}

// 4. lists the process ID and the creation time of the oldest descendant of process_id
void handle_odt(pid_t root, pid_t target) {
    ProcList descendants;
    init_list(&descendants);
    collect_descendants(target, &descendants);
    if (descendants.count == 0) {
        printf("No descendants\n");
        free_list(&descendants);
        return;
    }
    // Find min starttime
    ProcInfo oldest = descendants.items[0];
    for (int i = 1; i < descendants.count; i++) {
        if (descendants.items[i].starttime < oldest.starttime) {
            oldest = descendants.items[i];
        }
    }
    // Format time
    char time_str[64];
    struct tm *tm = localtime(&oldest.creation_time);
    strftime(time_str, sizeof(time_str), "%a %d %b %Y %I:%M:%S %p %Z", tm);
    printf("Oldest descendant of %d is %d, whose creation time is %s\n", target, oldest.pid, time_str);
    free_list(&descendants);
}

// 5. lists the process ID of the most recently created descendant of process_id
void handle_ndt(pid_t root, pid_t target) {
    int target_index = find_proc_index(target);
    
    ProcList descendants;
    init_list(&descendants);
    collect_descendants(target_index, &descendants);

    if (descendants.count == 0) {
        printf("No descendants\n");
        free_list(&descendants);
        return;
    }

    // Find max starttime
    ProcInfo newest = descendants.items[0];
    for (int i = 1; i < descendants.count; i++) {
        if (descendants.items[i].starttime > newest.starttime) {
            newest = descendants.items[i];
        }
    }
    printf("Most recently created descendant of %d is %d\n", target, newest.pid);
    free_list(&descendants);
} 

// 6. lists the count of all the non-direct descendants of process_id
void handle_dnd(pid_t root, pid_t target) {
    int target_index = find_proc_index(target);
    if (target_index == -1) {
        printf("Process %d not found\n", target);
        return;
    }
    ProcList descendants;
    init_list(&descendants);
    collect_descendants(target_index, &descendants);
    // Get direct children count
    int direct_count = 0;
    ChildNode *child = children[target_index];
    while (child != NULL) {
        direct_count++;
        child = child->next;
    }
    int non_direct = descendants.count - direct_count;
    printf("%d\n", non_direct >= 0 ? non_direct : 0);
    free_list(&descendants);
}

// 7. Kills the grandparent of process_id 
void handle_kgp(pid_t root, pid_t target) {
    
}

// 8. Kills the parent of process_id 
void handle_kpp(pid_t root, pid_t target) {

}

int main(int num_args, char *arguments[]) {

    if (num_args != 2 && num_args != 3 && num_args != 4) {
        printf("Invalid command\n");
        return 1;
    }

    if (num_args == 2) {
        // No process ID provided (Additional commands)
        // Array of valid commands
        const char *commands[] = { "-bcp", "-bop" };

        for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
            if (strcmp(arguments[1], commands[i]) == 0) {
                switch (i + 1) {
                case 1: 
                    handle_bcp();
                    break;
                case 2:
                    handle_bop();
                    break;
                default:
                    printf("Invalid command\n");
                    return 1;
                }
                return 0;
            }
        }
    }

    if (num_args == 3) {
        // no option received
        char *root_process = arguments[1];
        char *process_id = arguments[2];
        if(check_process_at_root()) {
            printf("Pid is: %d and PPID is: %d\n", getpid(), getppid());
        } else {
            printf("Process %s does not belong to the process subtree rooted at %s", process_id, root_process);
        }

    }

    if (num_args == 4){
        // option received
        // Array of valid commands
        const char *commands[] = {
            "-dpt", "-lvl", "-cnt", "-odt", "-ndt", "-dnd", "-kgp", "-kpp", "-ksp", "-kps", "-kgc", "-kcp", "-kst", "-dst", "-dct", "-krp", "-mmd", "-mpd", "-bcp", "-bop"
        };

        for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
            
            if (strcmp(arguments[1], commands[i]) == 0) {
                
                char *root_process = arguments[1];
                char *process_id = arguments[2];
                
                if (check_process_at_root()) {
                
                    switch (i + 1) {
                        
                        case 1: // 1. depth of process_id
                            int depth = handle_dpt(root_process, process_id);
                            printf("Depth of process %s is: %d\n", process_id, depth);
                            break;

                        case 2: // 2. all processes at the same level as process_id
                            handle_lvl(root_process, process_id);
                            break;
                        
                        case 3: // 3. count of all descendents of process_id
                            handle_cnt(root_process, process_id);
                            break;
                        
                        case 4: // 4. lists the process ID and the creation time of the oldest descendant of process_id
                            handle_odt(root_process, process_id);
                            break;
                        
                        case 5: // 5.  lists the process ID of the most recently created descendant of process_id
                            handle_ndt(root_process, process_id);
                            break;

                        case 6: // 6. lists the count of all the non-direct descendants of process_id 
                            handle_dnd(root_process, process_id);
                            break;

                        case 7: // 7. Kills the grandparent of process_id
                            handle_kgp(root_process, process_id);
                            break;
                        
                        case 8: // 8. Kills the parent of process_id
                            handle_kpp(root_process, process_id);
                            break;

                        default:
                            printf("Invalid command\n");
                            return 1;
                    }
                } else {
                    printf("Process %s does not belong to the process subtree rooted at %s", process_id, root_process);
                }
            }
        }
    }
}