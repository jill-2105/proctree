// to enable posix
#define _XOPEN_SOURCE 500
// to enable macro like FTW_ACTIONRETVAL
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ftw.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>

// Creating a global dirpath and defaulting it to .(pwd)
char *dir_path = ".";

// File counting (to list files and sorting them by creation time)
int file_count = 0;
typedef struct {
    char *file_path;
    time_t creation_time;
} File;
File files[25];

// 1. Listing files (no subdirectories)
int compare(const void *a, const void *b) {
    time_t t1 = ((File *)a)->creation_time;
    time_t t2 = ((File *)b)->creation_time;
    return t2 - t1;
}
void sort_files() {
    qsort(files, file_count, sizeof(File), compare);
}
void print_files() {
    printf("Files in reverse order\n");
    for (int i = 0; i < file_count; i++) {
        char *time_str = ctime(&files[i].creation_time);
        printf("%s: %s", files[i].file_path, time_str);
    }
}

// 2. Counting files (no subdirectories)
int fd_count = 0;
void print_file_count_in_dir() {
    printf("Total number of files in directory is: %d\n", fd_count);
}

// 3. Counting files of particular type (no subdirectories)
char *global_file_type;
int global_file_type_count = 0;
void print_file_type_count() {
    printf("Total number of file types is: %d\n", global_file_type_count);
}

// 4. Searching file of particular file type
char *global_file_name;
int file_name_count = 0;
char *filename[25];
void print_file_name() {
    if(file_name_count == 0) {
        printf("File not found\n");
    } else {
        for(int i = 0; i < file_name_count; i++) {
            printf("%s\n", filename[i]);
        }
    }
}

// 5. Listing sub-directories
char *global_sub_dir;
int sub_dir_count = 0;
char *subdir[25];
void print_sub_dir_name() {
    if(sub_dir_count == 0) {
        printf("Sub-directory not found\n");
    } else {
        for(int i = 0; i < sub_dir_count; i++) {
            printf("%s\n", subdir[i]);
        }
    }
}

// 6. Counting all files
int fdCount = 0;
void print_file_count() {
    printf("Total number of files is: %d\n", fdCount);
}

// 7. Count all directories
int count_all_dir = 0;
void print_dir_count() {
    printf("Total number of directories is: %d\n", count_all_dir);
}

// 8. Sum of all file sizes
int total_size = 0;
void print_total_size() {
    printf("Total size of files is: %d bytes\n", total_size);
}

// 9. Listing all files and directories
// 10. Listing files of a particular extension
char *listextn_type;
int listextn_files = 0;

// 11. Copying directory
char *src_path;
char *dest_path;
// Create a directory
int create_dir(const char *dest_path){
    int dir_res = mkdir(dest_path, 0777);
    if(dir_res == -1) {
        printf("Error creating directory\n");
        return -1;
    }
    return 0;
}
// Copy a file
int copy_file(const char *src_path, const char *dest_path){
    // Opening source file
    int src_fd = open(src_path, O_RDONLY);
    if (src_fd == -1) {
        printf("Error opening source file");
        return -1;
    }

    // Opening destination file
    int dest_fd = open(dest_path, O_CREAT | O_WRONLY | O_TRUNC, 0777);
    if (dest_fd == -1) {
        printf("Error opening destination file");
        return -1;
    }

    // Character buffer to store file content
    char buffer[2048];
    ssize_t bytes_at_src;
    ssize_t bytes_at_dest;
    while ((bytes_at_src = read(src_fd, buffer, sizeof(buffer))) > 0) {
        bytes_at_dest = write(dest_fd, buffer, bytes_at_src);
        if(bytes_at_dest != bytes_at_src) {
            printf("Error writing to destination file");
            return -1;
        }
    } if(bytes_at_src == -1) {
        printf("Error reading from source file");
        return -1;
    }

    close(src_fd);
    close(dest_fd);
    return 0;
}

// 12. Moving directory
char *source_dir_path;
char *destination_dir_path;

// 13. Deleting files of a particular extension
char *file_extension;

// Deleting file
int del_file(const char *file_path){
    int del_file_result = unlink(file_path);
        if (del_file_result == -1) {
            printf("Error deleting file");
            return -1;
        } else {
            printf("File deleted successfully");
            return 0;
        }
}
// Deleting functionality (copy move and delete directory)
static int delete(const char *file_path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F) {
        del_file(file_path);
        return 0;
    } else if (typeflag == FTW_DP) {
        int del_dir_result = rmdir(file_path);
        if (del_dir_result == -1) {
            printf("Error deleting directory\n");
            return -1;
        }
        return 0;
    }
    return 0;
}
// Deleting directory
int del_dir(const char *file_path){
    // calling another nftw call here to make sure all files and subdirectories are deleted first
    int result = nftw(file_path, delete, 20, FTW_DEPTH | FTW_PHYS);
    if (result == -1) {
        printf("Error deleting directory");
        return -1;
    }
    return 0;
}

/*
======Functions======
*/

// 1. Listing files (no subdirectories)
int dirlist(const char *file_path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_D) {
        if (ftwbuf->level >= 1) {
            return FTW_SKIP_SUBTREE;
        }
    }
    if (typeflag == FTW_F) {
        // Check array bounds to prevent overflow
        if (file_count >= 25) {
            printf("Warning: Maximum file limit (25) reached, skipping additional files\n");
            return 0;
        }
        files[file_count].file_path = strdup(file_path);
        files[file_count].creation_time = sb->st_ctime;
        file_count++;
    }
    return 0;
}

// 2. Counting files (no subdirectories)
int countfd(const char *file_path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_D) {
        if (ftwbuf->level >= 1) {
            return FTW_SKIP_SUBTREE;
        }
    }
    if (typeflag == FTW_F) {
        fd_count++; 
    }
    return 0;
} 

// 3. Counting files of particular type (no subdirectories)
int countype(const char *file_path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_D) {
        if (ftwbuf->level >= 1) {
            return FTW_SKIP_SUBTREE;
        }
    }
    if (typeflag == FTW_F) {
        char *ext = strrchr(file_path, '.');
        if (strcmp(global_file_type, ext) == 0) {
            global_file_type_count++;
        }
    }
    return 0;
}

// 4. Searching file of particular file type
int filesrch(const char *file_path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F) {
        char *file = basename((char *)file_path);
        if (strcmp(file, global_file_name) == 0) {
            filename[file_name_count++] = strdup(file_path);
            if (file_name_count>=25) {
                printf("Max limit reached");
                return -1;
            }
        }
    }
    return 0;
}

// 5. Listing sub-directories
int dirlst(const char *file_path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_D) {
        char *sub_dir = basename((char *)file_path);
        if (strcmp(sub_dir, global_sub_dir) == 0) {
            subdir[sub_dir_count++] = strdup(file_path);
            if (sub_dir_count>=25) {
                printf("Max limit reached");
                return -1;
            }
        }
    }
    return 0;
}

// 6. Counting all files 
int fcountrt(const char *file_path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F) {
        fdCount++; 
   }
    return 0;
}

// 7. Count all directories
int dircnt(const char *file_path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_D) {
        count_all_dir++;
    }
    return 0;
}

// 8. Sum of all file sizes
int sumfilesize(const char *file_path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if(typeflag==FTW_F){
        total_size = total_size + sb->st_size;
    }
    return 0;
}

// 9. Listing all files and directories
int rootlist(const char *file_path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_D || typeflag == FTW_F) {
        char *allfiles = basename((char *)file_path);
        printf("File: %s Path: %s\n", allfiles, file_path);
    }
    return 0;
}

// 10. Listing files of a particular extension
int listextn(const char *file_path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F) {
        char *ext = strrchr(file_path, '.');
        if (ext != NULL) {
            if(strcmp(listextn_type, ext) == 0) {
                listextn_files++;
                char *allfiles = basename((char *)file_path);
                printf("File: %s Path: %s\n", allfiles, file_path);
            } 
        }
    }
    return 0;
}

// 11. Copying directory
int copyd(const char *file_path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    // Skip the source directory
    if (ftwbuf->level == 0) {
        return 0;
    }
    
    // Construct relative path
    const char *relative_path = file_path + strlen(src_path);
    // Skipping the final slash
    if (*relative_path == '/') relative_path++; 
    
    // Construct full destination path
    char full_dest[PATH_MAX];
    int result = snprintf(full_dest, sizeof(full_dest), "%s/%s", dest_path, relative_path);
    
    // Check if the path was truncated (buffer overflow protection)
    if (result >= PATH_MAX) {
        printf("Error: Destination path too long\n");
        return -1;
    }
    
    if(typeflag == FTW_D) {
        int dir_result = create_dir(full_dest);
        if (dir_result != 0) {
            printf("Error creating directory\n");
        } 
        return 0;
    } else if(typeflag == FTW_F) {
        int file_result = copy_file(file_path, full_dest);
        if (file_result != 0) {
            printf("Error copying file\n");
        }
        return 0;
    }
    return 0;
}

// 12. Moving directory
int dmove(const char *file_path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    // Skip the source directory itself (level 0)
    if (ftwbuf->level == 0) {
        return 0;
    }
    
    const char *relative_path = file_path + strlen(source_dir_path);
    // Skipping the final slash
    if (*relative_path == '/') relative_path++; 
    
    // Construct full destination path
    char full_dest[PATH_MAX];
    int result = snprintf(full_dest, sizeof(full_dest), "%s/%s", destination_dir_path, relative_path);
    
    // Check if the path was truncated (buffer overflow protection)
    if (result >= PATH_MAX) {
        printf("Error: Destination path too long\n");
        return -1;
    }
    
    // Logic of move
    if(typeflag == FTW_F) {
        if (copy_file(file_path, full_dest) == 0) {
            del_file(file_path);
            return 0;
        }
        return -1;
    } else if(typeflag == FTW_D) {
        create_dir(full_dest);
        return 0;
    }
    return 0;
}

// 13. Deleting files of a particular extension
int remd(const char *file_path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if(typeflag == FTW_F) {
        char *extension = strrchr(file_path, '.');
        if (strcmp(file_extension, extension) == 0) {
            del_file(file_path);
        }
    }
    return 0;
}

/*
======Main Function======
*/
int main(int num_args, char *arguments[]) {

    if (num_args < 2) {
        printf("Invalid command\n");
        return 1;
    }

    // Array of valid commands
    const char *commands[] = {
        "-dirlist", "-countfd", "-countype", "-filesrch", "-dirlst", "-fcountrt", "-dircnt", "-sumfilesize", "-rootlist", "-Listextn", "-copyd", "-dmove", "-remd"
    };

    for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {

        if (strcmp(arguments[1], commands[i]) == 0) {

            switch (i + 1) {
            case 1: // 1. Listing files (no subdirectories)
                if (num_args != 3) {
                    printf("Few/many arguments received\n");
                    return 1;
                }
                dir_path = arguments[2];
                int nftw_result =nftw(dir_path, dirlist, 20, FTW_PHYS | FTW_ACTIONRETVAL);
                if(nftw_result == -1) {
                    printf("Error in nftw function\n");
                    return 1;
                }
                sort_files(); 
                print_files();
                break;

            case 2: // 2. Counting files (no subdirectories)
                if (num_args != 3) {
                    printf("Few/many arguments received\n");
                    return 1;
                }
                dir_path = arguments[2];
                nftw_result = nftw(dir_path, countfd, 20, FTW_PHYS | FTW_ACTIONRETVAL);
                if(nftw_result == -1) {
                    printf("Error in nftw function\n");
                    return 1;
                }
                print_file_count_in_dir();
                break;

            case 3: // 3. Counting files of particular type (no subdirectories)
                if (num_args != 4) {
                    printf("Few/many arguments received\n");
                    return 1;
                }
                global_file_type = arguments[2];
                dir_path = arguments[3];
                nftw_result = nftw(dir_path, countype, 20, FTW_PHYS | FTW_ACTIONRETVAL);
                if(nftw_result == -1) {
                    printf("Error in nftw function\n");
                    return 1;
                }
                print_file_type_count();
                break;

            case 4: // 4. Searching file of particular file type
                if (num_args != 4) {
                    printf("Few/many arguments received\n");
                    return 1;
                }
                global_file_name = arguments[2];
                dir_path = arguments[3];
                nftw_result = nftw(dir_path, filesrch, 20, FTW_PHYS);
                if(nftw_result == -1) {
                    printf("Error in nftw function\n");
                    return 1;
                }
                print_file_name();
                break;

            case 5: // 5. Listing sub-directories
                if (num_args != 4) {
                    printf("Few/many arguments received\n");
                    return 1;
                }
                global_sub_dir = arguments[2];
                dir_path = arguments[3];
                nftw_result = nftw(dir_path, dirlst, 20, FTW_PHYS);
                if(nftw_result == -1) {
                    printf("Error in nftw function\n");
                    return 1;
                }
                print_sub_dir_name();
                break;

            case 6: // 6. Counting all files
                if (num_args != 3) {
                    printf("Few/many arguments received\n");
                    return 1;
                }
                dir_path = arguments[2];
                nftw_result = nftw(dir_path, fcountrt, 20, FTW_PHYS);
                if(nftw_result == -1) {
                    printf("Error in nftw function\n");
                    return 1;
                }
                print_file_count();
                break;

            case 7: // 7. Count all directories
                if (num_args != 3) {
                    printf("Few/many arguments received\n");
                    return 1;
                }
                dir_path = arguments[2];
                nftw_result = nftw(dir_path, dircnt, 20, FTW_PHYS);
                if(nftw_result == -1) {
                    printf("Error in nftw function\n");
                    return 1;
                }
                print_dir_count();
                break;  

            case 8: // 8. Sum of all file sizes
                if (num_args != 3) {
                    printf("Few/many arguments received\n");
                    return 1;
                }
                dir_path = arguments[2];
                nftw_result = nftw(dir_path, sumfilesize, 20, FTW_PHYS);
                if(nftw_result == -1) {
                    printf("Error in nftw function\n");
                    return 1;
                }
                print_total_size();
                break;

            case 9: // 9. Listing all files and directories
                if (num_args != 3) {
                    printf("Few/many arguments received\n");
                    return 1;
                }
                dir_path = arguments[2];
                nftw_result = nftw(dir_path, rootlist, 20, FTW_PHYS);
                if(nftw_result == -1) {
                    printf("Error in nftw function\n");
                    return 1;
                }
                break;

            case 10: // 10. Listing files of a particular extension
                if (num_args != 4) {
                    printf("Few/many arguments received\n");
                    return 1;
                }
                dir_path = arguments[2];
                listextn_type = arguments[3];
                listextn_files = 0;
                nftw_result = nftw(dir_path, listextn, 20, FTW_PHYS);
                if (nftw_result == -1) {
                    printf("Error in nftw function\n");
                    return 1;
                }
                if (listextn_files == 0) {
                    printf("No file found\n");
                }
                break;

            case 11: // 11. Copying directory
                if (num_args != 4) {
                    printf("Few/many arguments received\n");
                    return 1;
                }
                src_path = arguments[2];
                dest_path = arguments[3];
                nftw_result = nftw(src_path, copyd, 20, FTW_PHYS);
                if (nftw_result == -1) {
                    printf("Error in nftw function\n");
                    return 1;
                }
                printf("Copy completed\n");
                break;

            case 12: // 12. Moving directory
                if (num_args != 4) {
                    printf("Few/many arguments received\n");
                    return 1;
                }
                source_dir_path = arguments[2];
                destination_dir_path = arguments[3];
                nftw_result = nftw (source_dir_path, dmove, 20, FTW_PHYS);
                if (nftw_result == -1) {
                    printf("Error in nftw function\n");
                    return 1;
                }
                del_dir(source_dir_path); // Deleting the source directory after moving
                printf("Move completed\n");
                break;

            case 13: // 13. Deleting files of a particular extension
                if (num_args != 4) {
                    printf("Few/many arguments received\n");
                    return 1;
                }
                source_dir_path = arguments[2];
                file_extension = arguments[3];
                nftw_result = nftw(source_dir_path, remd, 20, FTW_PHYS);
                if (nftw_result == -1) {
                    printf("Error in nftw function\n");
                    return 1;
                }
                printf("Delete completed\n");
                break;

            default:
                printf("Invalid command\n");
            }
            return 0;
        }
    }
    return 1;
}
