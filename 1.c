#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <ctype.h>
#include <sys/sysinfo.h>

#define TASKCOMMLEN 16
#define PATHMAX 256
#define HZ 100
#define INITIAL_CAPACITY 1024

typedef struct {
    pid_t pid;
    pid_t ppid;
    unsigned long starttime;
    long vmrss;
    unsigned long cputime;
    char state;
    time_t creationtime;
    char comm[TASKCOMMLEN];
} ProcInfo;

typedef struct {
    ProcInfo *items;
    int count;
    int capacity;
} ProcList;

ProcList *create_proclist() {
    ProcList *list = malloc(sizeof(ProcList));
    if (!list) return NULL;
    list->count = 0;
    list->capacity = INITIAL_CAPACITY;
    list->items = malloc(sizeof(ProcInfo) * list->capacity);
    if (!list->items) {
        free(list);
        return NULL;
    }
    return list;
}

int expand_proclist(ProcList *list) {
    if (list->count < list->capacity) return 1;
    int new_capacity = list->capacity * 2;
    ProcInfo *new_items = realloc(list->items, sizeof(ProcInfo) * new_capacity);
    if (!new_items) return 0;
    list->items = new_items;
    list->capacity = new_capacity;
    return 1;
}

void free_proclist(ProcList *list) {
    if (list) {
        if (list->items) free(list->items);
        free(list);
    }
}

void scanprocfs(ProcList *proclist) {
    DIR *procdir = opendir("/proc");
    if (!procdir) return;
    struct dirent *entry;
    proclist->count = 0;
    while ((entry = readdir(procdir)) && expand_proclist(proclist)) {
        char *endptr;
        pid_t pid = strtol(entry->d_name, &endptr, 10);
        if (*endptr != '\0' || pid <= 0) continue;
        char statpath[PATHMAX];
        snprintf(statpath, sizeof(statpath), "/proc/%d/stat", pid);
        FILE *statf = fopen(statpath, "r");
        if (!statf) continue;
        pid_t ppid;
        unsigned long utime, stime, starttime;
        char state;
        if (fscanf(statf, "%d %s %c %d %lu %lu %lu", &pid, proclist->items[proclist->count].comm, &state, &ppid, &utime, &stime, &starttime) != 7) {
            fclose(statf);
            continue;
        }
        fclose(statf);
        proclist->items[proclist->count].pid = pid;
        proclist->items[proclist->count].ppid = ppid;
        proclist->items[proclist->count].state = state;
        proclist->items[proclist->count].starttime = starttime;
        proclist->items[proclist->count].cputime = utime + stime;
        // Parse VmRSS from status
        char statuspath[PATHMAX];
        snprintf(statuspath, sizeof(statuspath), "/proc/%d/status", pid);
        FILE *statusf = fopen(statuspath, "r");
        proclist->items[proclist->count].vmrss = 0;
        if (statusf) {
            char line[256];
            while (fgets(line, sizeof(line), statusf)) {
                if (strncmp(line, "VmRSS:", 6) == 0) {
                    sscanf(line + 6, "%ld kB", &proclist->items[proclist->count].vmrss);
                    proclist->items[proclist->count].vmrss *= 1024;
                    break;
                }
            }
            fclose(statusf);
        }
        // Approximate creation time (simplified, without precise uptime)
        proclist->items[proclist->count].creationtime = time(NULL) - (starttime / HZ);
        proclist->count++;
    }
    closedir(procdir);
}

// Find proc index by pid
int find_proc_index(const ProcList *list, pid_t pid) {
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].pid == pid) return i;
    }
    return -1;
}

// Get ppid
pid_t get_ppid(const ProcList *list, pid_t pid) {
    int idx = find_proc_index(list, pid);
    return (idx != -1) ? list->items[idx].ppid : -1;
}

// Check if target in root subtree
int check_process_at_root(const ProcList *list, pid_t root, pid_t target) {
    if (root == target) return 1;
    pid_t current = target;
    while (current > 0 && current != root) {
        current = get_ppid(list, current);
    }
    return (current == root);
}

// Depth calc
int handle_dpt(const ProcList *list, pid_t root, pid_t target) {
    int depth = 0;
    pid_t current = target;
    while (current != root && current > 0) {
        current = get_ppid(list, current);
        depth++;
    }
    return (current == root) ? depth : -1;
}

// Collect descendants indices dynamically
int *descendants = NULL;
int desc_capacity = 0;
int desc_count = 0;
void collect_descendants(const ProcList *proclist, int parent_idx, int exclude_self) {
    desc_count = 0;
    pid_t parent_pid = proclist->items[parent_idx].pid;
    for (int i = 0; i < proclist->count; i++) {
        if (proclist->items[i].ppid == parent_pid && (exclude_self || proclist->items[i].pid != parent_pid)) {
            if (desc_count >= desc_capacity) {
                desc_capacity = desc_capacity ? desc_capacity * 2 : 256;
                int *new_desc = realloc(descendants, sizeof(int) * desc_capacity);
                if (!new_desc) return;
                descendants = new_desc;
            }
            descendants[desc_count++] = i;
            // Collect deeper levels iteratively
            collect_descendants(proclist, i, 0);
        }
    }
}

// 1. depth of processid
void print_dpt(const ProcList *list, pid_t root, pid_t target) {
    int depth = handle_dpt(list, root, target);
    printf("Depth of %d is %d\n", target, depth);
}

// 2. all processes at the same level as processid
void handle_lvl(const ProcList *list, pid_t root, pid_t target) {  
    int target_depth = handle_dpt(list, root, target);
    int count = 0;
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].pid == root) continue;
        int curr_depth = handle_dpt(list, root, list->items[i].pid);
        if (curr_depth == target_depth) count++;
    }
    printf("No. of processes at the same depth of %d in the process tree %d\n", target, count);
}

// 3. count of all descendants of processid
void handle_cnt(const ProcList *list, pid_t root, pid_t target) {
    int target_idx = find_proc_index(list, target);
    collect_descendants(list, target_idx, 1);
    printf("%d\n", desc_count);
}

// Helper for oldest/newest
void find_oldest_newest(const ProcList *list) {
    if (desc_count == 0) return;
    int first_idx = descendants[0];
    list->items[first_idx].pid;  // Trigger copy
    ProcInfo oldest = list->items[first_idx];
    ProcInfo newest = oldest;
    for (int i = 1; i < desc_count; i++) {
        int idx = descendants[i];
        if (list->items[idx].starttime < oldest.starttime) {
            oldest = list->items[idx];
        }
        if (list->items[idx].starttime > newest.starttime) {
            newest = list->items[idx];
        }
    }
    // Store in globals for print (simple)
    oldest_desc = oldest;
    newest_desc = newest;
}

ProcInfo oldest_desc, newest_desc;

// 4. oldest descendant
void handle_odt(const ProcList *list, pid_t root, pid_t target) {
    int target_idx = find_proc_index(list, target);
    collect_descendants(list, target_idx, 1);
    if (desc_count == 0) {
        printf("No descendants\n");
        return;
    }
    find_oldest_newest(list);
    char timestr[64];
    struct tm *tm = localtime(&oldest_desc.creationtime);
    strftime(timestr, sizeof(timestr), "%a %d %b %Y %I:%M:%S %p %Z", tm);
    printf("Oldest descendant of %d is %d, whose creation time is %s\n", target, oldest_desc.pid, timestr);
}

// 5. most recently created descendant
void handle_ndt(const ProcList *list, pid_t root, pid_t target) { 
    int target_idx = find_proc_index(list, target);
    collect_descendants(list, target_idx, 1);
    if (desc_count == 0) {
        printf("No descendants\n");
        return;
    }
    find_oldest_newest(list);
    printf("Most recently created descendant of %d is %d\n", target, newest_desc.pid);
}

// 6. count of all non-direct descendants
void handle_dnd(const ProcList *list, pid_t root, pid_t target) {   
    int target_idx = find_proc_index(list, target);
    collect_descendants(list, target_idx, 1);
    int direct_count = 0;
    pid_t target_pid = list->items[target_idx].pid;
    for (int i = 0; i < desc_count; i++) {
        int d_idx = descendants[i];
        if (list->items[d_idx].ppid == target_pid) direct_count++;
    }
    int nondirect = desc_count - direct_count;
    printf("%d\n", nondirect < 0 ? 0 : nondirect);
}

// Can kill check
int can_kill_process(pid_t pid, const char *role) {
    if (pid == 1) {
        printf("%d is a INIT process and will not be terminated\n", pid);
        return 0;
    }
    char path[PATHMAX];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char comm[32];
    fgets(comm, sizeof(comm), f);
    comm[strcspn(comm, "\n")] = 0;  // Trim newline
    fclose(f);
    if (strstr(comm, "bash") != NULL) {
        printf("%d is BASH process and will not be terminated\n", pid);
        return 0;
    }
    return 1;
}

// 7. Kills grandparent
void handle_kgp(const ProcList *list, pid_t root, pid_t target) {
    pid_t parent = get_ppid(list, target);
    if (parent == -1) {
        printf("No parent for process %d\n", target);
        return;
    }
    pid_t grand = get_ppid(list, parent);
    if (grand == -1) {
        printf("No grandparent for process %d\n", target);
        return;
    }
    if (can_kill_process(grand, "Grandparent")) {
        printf("Killing grandparent %d\n", grand);
        if (kill(grand, SIGKILL) == 0) {
            printf("%d is terminated\n", grand);
        } else {
            printf("Failed to kill grandparent\n");
        }
    }
}

// 8. Kills parent
void handle_kpp(const ProcList *list, pid_t root, pid_t target) {
    pid_t parent = get_ppid(list, target);
    if (parent == -1) {
        printf("No parent for process %d\n", target);
        return;
    }
    if (can_kill_process(parent, "Parent")) {
        printf("Killing parent %d\n", parent);
        if (kill(parent, SIGKILL) == 0) {
            printf("%d is terminated\n", parent);
        } else {
            printf("Failed to kill parent\n");
        }
    }
}

// 9. Kills siblings
void handle_ksp(const ProcList *list, pid_t root, pid_t target) {
    pid_t parent = get_ppid(list, target);
    if (parent == -1) {
        printf("No parent for process %d\n", target);
        return;
    }
    int killed_count = 0;
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].ppid == parent && list->items[i].pid != target) {
            pid_t sib = list->items[i].pid;
            if (can_kill_process(sib, "Sibling")) {
                printf("Killing sibling %d\n", sib);
                if (kill(sib, SIGKILL) == 0) {
                    printf("SIGKILL was sent to %d\n", sib);
                    killed_count++;
                } else {
                    printf("Failed to kill sibling %d\n", sib);
                }
            }
        }
    }
}

// 10. Kills siblings of parent (uncles/aunts)
void handle_kps(const ProcList *list, pid_t root, pid_t target) {   
    pid_t parent = get_ppid(list, target);
    if (parent == -1) return;
    pid_t grand = get_ppid(list, parent);
    if (grand == -1) {
        printf("No grandparent for process %d\n", target);
        return;
    }
    int killed_count = 0;
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].ppid == grand && list->items[i].pid != parent) {
            pid_t ua = list->items[i].pid;
            if (can_kill_process(ua, "UncleAunt")) {
                printf("Killing uncleaunt %d\n", ua);
                if (kill(ua, SIGKILL) == 0) {
                    printf("%d is terminated\n", ua);
                    killed_count++;
                } else {
                    printf("Failed to kill uncleaunt %d\n", ua);
                }
            }
        }
    }
}

// 11. Kills grandchildren (-kgc)
void handle_kgc(const ProcList *list, pid_t root, pid_t target) {  
    int target_idx = find_proc_index(list, target);
    // Collect children indices
    int *children = NULL;
    int child_count = 0;
    int child_cap = 0;
    pid_t target_pid = list->items[target_idx].pid;
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].ppid == target_pid) {
            if (child_count >= child_cap) {
                child_cap = child_cap ? child_cap * 2 : 64;
                int *new_children = realloc(children, sizeof(int) * child_cap);
                if (!new_children) {
                    free(children);
                    return;
                }
                children = new_children;
            }
            children[child_count++] = i;
        }
    }
    // For each child, kill their children (grandchildren)
    for (int c = 0; c < child_count; c++) {
        int child_idx = children[c];
        pid_t child_pid = list->items[child_idx].pid;
        for (int i = 0; i < list->count; i++) {
            if (list->items[i].ppid == child_pid) {
                pid_t grandc = list->items[i].pid;
                if (can_kill_process(grandc, "Grandchild")) {
                    printf("Killing grandchild %d\n", grandc);
                    if (kill(grandc, SIGKILL) == 0) {
                        printf("%d is terminated\n", grandc);
                    } else {
                        printf("Failed to kill grandchild\n");
                    }
                }
            }
        }
    }
    free(children);
}

// 12. Kills children (-kcp)
void handle_kcp(const ProcList *list, pid_t root, pid_t target) {  
    int killed_count = 0;
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].ppid == target) {
            pid_t child = list->items[i].pid;
            if (can_kill_process(child, "Child")) {
                printf("Killing child %d\n", child);
                if (kill(child, SIGKILL) == 0) {
                    printf("SIGKILL was sent to %d\n", child);
                    killed_count++;
                } else {
                    printf("Failed to kill child\n");
                }
            }
        }
    }
}

// 13. Kills subtree in creation order (-kst)
void handle_kst(const ProcList *list, pid_t root, pid_t target) {   
    int target_idx = find_proc_index(list, target);
    collect_descendants(list, target_idx, 1);
    if (desc_count == 0) return;
    // Sort descendants ascending starttime (oldest first)
    for (int i = 0; i < desc_count - 1; i++) {
        for (int j = i + 1; j < desc_count; j++) {
            if (list->items[descendants[i]].starttime > list->items[descendants[j]].starttime) {
                int temp = descendants[i];
                descendants[i] = descendants[j];
                descendants[j] = temp;
            }
        }
    }
    // Kill in order, print PID and time
    for (int i = 0; i < desc_count; i++) {
        int idx = descendants[i];
        pid_t pid = list->items[idx].pid;
        if (can_kill_process(pid, "Subtree")) {
            printf("Killing %d (created %ld)\n", pid, list->items[idx].starttime);
            if (kill(pid, SIGKILL) == 0) {
                char timestr[64];
                struct tm *tm = localtime(&list->items[idx].creationtime);
                strftime(timestr, sizeof(timestr), "%a %d %b %Y %I:%M:%S %p %Z", tm);
                printf("Terminated %d at %s\n", pid, timestr);
            } else {
                printf("Failed to kill %d\n", pid);
            }
        }
    }
}

// 14. SIGSTOP descendants (-dst)
void handle_dst(const ProcList *list, pid_t root, pid_t target) { 
    int target_idx = find_proc_index(list, target);
    collect_descendants(list, target_idx, 1);
    for (int i = 0; i < desc_count; i++) {
        pid_t pid = list->items[descendants[i]].pid;
        if (can_kill_process(pid, "Descendant")) {
            kill(pid, SIGSTOP);
            printf("SIGSTOP sent to %d\n", pid);
        }
    }
}

// 15. SIGCONT stopped descendants (-dct)
void handle_dct(const ProcList *list, pid_t root, pid_t target) { 
    int target_idx = find_proc_index(list, target);
    collect_descendants(list, target_idx, 1);
    for (int i = 0; i < desc_count; i++) {
        int d_idx = descendants[i];
        pid_t pid = list->items[d_idx].pid;
        if (list->items[d_idx].state == 'T' && can_kill_process(pid, "Descendant")) {
            kill(pid, SIGCONT);
            printf("SIGCONT sent to %d\n", pid);
        }
    }
}

// 16. Kill root (-krp)
void handle_krp(const ProcList *list, pid_t root, pid_t target) { 
    if (can_kill_process(root, "Root")) {
        printf("Killing root %d\n", root);
        if (kill(root, SIGKILL) == 0) {
            printf("%d is terminated\n", root);
        } else {
            printf("Failed to kill root\n");
        }
    }
}

// Helpers for max mmd/mpd
long find_max_vmrss(const ProcList *list, int target_idx) {
    long max_vmrss = 0;
    collect_descendants(list, target_idx, 1);
    for (int i = 0; i < desc_count; i++) {
        int idx = descendants[i];
        if (list->items[idx].vmrss > max_vmrss) {
            max_vmrss = list->items[idx].vmrss;
        }
    }
    return max_vmrss;
}

unsigned long find_max_cpu(const ProcList *list, int target_idx) {
    unsigned long max_cpu = 0;
    collect_descendants(list, target_idx, 1);
    for (int i = 0; i < desc_count; i++) {
        int idx = descendants[i];
        if (list->items[idx].cputime > max_cpu) {
            max_cpu = list->items[idx].cputime;
        }
    }
    return max_cpu;
}

// 17. Most memory descendant (-mmd)
void handle_mmd(const ProcList *list, pid_t root, pid_t target) {
    int target_idx = find_proc_index(list, target);
    long max_vmrss = find_max_vmrss(list, target_idx);
    int listed = 0;
    printf("Descendant(s) of %d consuming most memory. VmRSS %ld bytes:\n", target, max_vmrss);
    for (int i = 0; i < list->count; i++) {
        if (check_process_at_root(list, target, list->items[i].pid) && list->items[i].vmrss == max_vmrss) {
            printf("%d ", list->items[i].pid);
            listed++;
        }
    }
    if (listed == 0) printf("No descendants\n");
    printf("\n");
}

// 18. Most CPU descendant (-mpd)
void handle_mpd(const ProcList *list, pid_t root, pid_t target) {
    int target_idx = find_proc_index(list, target);
    unsigned long max_cpu = find_max_cpu(list, target_idx);
    int listed = 0;
    printf("Descendant(s) of %d with most CPU time. Total %lu clock ticks:\n", target, max_cpu);
    for (int i = 0; i < list->count; i++) {
        if (check_process_at_root(list, target, list->items[i].pid) && list->items[i].cputime == max_cpu) {
            printf("%d ", list->items[i].pid);
            listed++;
        }
    }
    if (listed == 0) printf("No descendants\n");
    printf("\n");
}

// Bash count helpers
int is_bash_subtree(const ProcList *list, pid_t pid) {
    pid_t current = pid;
    while (current > 0) {
        int idx = find_proc_index(list, current);
        if (idx != -1 && strstr(list->items[idx].comm, "bash") != NULL) return 1;
        current = get_ppid(list, current);
    }
    return 0;
}

// Additional command -bcp
int count_bcp(const ProcList *list) {
    int count = 0;
    pid_t self_pid = getpid();
    for (int i = 0; i < list->count; i++) {
        if (is_bash_subtree(list, list->items[i].pid) && list->items[i].pid != self_pid) count++;
    }
    return count;
}
void handle_bcp(ProcList *list) {
    scanprocfs(list);
    printf("%d\n", count_bcp(list));
}

// Additional command -bop
int count_bop(const ProcList *list) {
    int count = 0;
    for (int i = 0; i < list->count; i++) {
        if (!is_bash_subtree(list, list->items[i].pid) && list->items[i].pid > 1) count++;
    }
    return count;
}
void handle_bop(ProcList *list) {
    scanprocfs(list);
    printf("%d\n", count_bop(list));
}

// Main Function
int main(int num_args, char *arguments[]) {

    if (num_args != 2 && num_args != 3 && num_args != 4) {
        printf("Invalid command\n");
        return 1;
    }

    ProcList *proclist = create_proclist();
    if (!proclist) {
        printf("Memory allocation failed\n");
        return 1;
    }
    scanprocfs(proclist);

    if (num_args == 2) {
        // No process ID provided (Additional commands)
        // Array of valid commands
        const char *commands[] = { "-bcp", "-bop" };

        for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
            if (strcmp(arguments[1], commands[i]) == 0) {
                switch (i + 1) {
                    case 1: 
                        handle_bcp(proclist); break;

                    case 2:
                        handle_bop(proclist); break;

                    default:
                        printf("Invalid command\n"); return 1;
                }
                free_proclist(proclist);
                free(descendants);
                return 0;
            }
        }
    }


    if (num_args == 3) {
        // no option received
        pid_t root_process = atoi(arguments[1]);
        pid_t process_id = atoi(arguments[2]);
        if(check_process_at_root(proclist, root_process, process_id)) {
            pid_t ppid = get_ppid(proclist, process_id);
            printf("Pid is: %d and PPID is: %d\n", process_id, ppid);
        } else {
            printf("Process %d does not belong to the process subtree rooted at %d\n", process_id, root_process);
        }
        free_proclist(proclist);
        free(descendants);
        return 0;
    }


    if (num_args == 4) {
        // option received
        // Array of valid commands
        const char *commands[] = {
            "-dpt", "-lvl", "-cnt", "-odt", "-ndt", "-dnd", "-kgp", "-kpp", "-ksp", "-kps", "-kgc", "-kcp", "-kst", "-dst", "-dct", "-krp", "-mmd", "-mpd", "-bcp", "-bop"
        };

        for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
            
            if (strcmp(arguments[1], commands[i]) == 0) {

                pid_t root_process = atoi(arguments[1]);
                pid_t process_id = atoi(arguments[2]);

                if (check_process_at_root(proclist, root_process, process_id)) {
                    
                    switch (i + 1) {
                        case 1:
                            print_dpt(proclist, root_process, process_id); break;
                        
                        case 2:
                            handle_lvl(proclist, root_process, process_id); break;

                        case 3:
                            handle_cnt(proclist, root_process, process_id); break;

                        case 4:
                            handle_odt(proclist, root_process, process_id); break;

                        case 5:
                            handle_ndt(proclist, root_process, process_id); break;

                        case 6:
                            handle_dnd(proclist, root_process, process_id); break;

                        case 7:
                            handle_kgp(proclist, root_process, process_id); break;

                        case 8:
                            handle_kpp(proclist, root_process, process_id); break;

                        case 9:
                            handle_ksp(proclist, root_process, process_id); break;

                        case 10:
                            handle_kps(proclist, root_process, process_id); break;

                        case 11:
                            handle_kgc(proclist, root_process, process_id); break;

                        case 12:
                            handle_kcp(proclist, root_process, process_id); break;

                        case 13:
                            handle_kst(proclist, root_process, process_id); break;

                        case 14:
                            handle_dst(proclist, root_process, process_id); break;

                        case 15:
                            handle_dct(proclist, root_process, process_id); break;

                        case 16:
                            handle_krp(proclist, root_process, process_id); break;

                        case 17:
                            handle_mmd(proclist, root_process, process_id); break;

                        case 18:
                            handle_mpd(proclist, root_process, process_id); break;

                        default:
                            printf("Invalid command\n"); return 1;
                    }

                } else {
                    printf("Process %d does not belong to the process subtree rooted at %d\n", process_id, root_process);
                    free_proclist(proclist);
                    free(descendants);
                    return 1;
                }
            }
        }
    }    
}