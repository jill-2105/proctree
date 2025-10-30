# Process Tree Utility - `proctree_Jill_Patel_110176154.c`

## Overview

This C program provides a set of utilities to explore and manipulate the Linux process tree using `/proc`. It can report information about process relationships, kill specific process groups, and perform operations based on process ancestry, memory, and CPU usage.

> **Note:** This utility must be run on a Linux system with a procfs (`/proc`) filesystem. Many functions require root privileges to signal/kill processes not owned by the user.

---

## Compilation

Use `gcc` to compile:

```bash
gcc -o proctree proctree_Jill_Patel_110176154.c
```

---

## Usage

```
./proctree <rootpid> <pid> <option>
./proctree <rootpid> <pid>
./proctree <option>
```

- `<rootpid>`: Root of the process tree (usually 1 for `init`)
- `<pid>`: Target process ID
- `<option>`: Operation/command to be executed

---

## Supported Options (Commands)

All commands below expect `<rootpid>` and `<pid>`, unless noted otherwise.

| Option  | Description                                              |
|---------|----------------------------------------------------------|
| -dpt    | Print the depth (distance from root) of `<pid>`          |
| -lvl    | Number of processes at same depth as `<pid>`             |
| -cnt    | Count descendants of `<pid>`                             |
| -odt    | Oldest descendant (by start time)                        |
| -ndt    | Newest descendant                                        |
| -dnd    | Count non-direct descendants                             |
| -kgp    | Kill grandparent process                                 |
| -kpp    | Kill parent process                                      |
| -ksp    | Kill siblings                                            |
| -kps    | Kill parent's siblings (uncles)                          |
| -kgc    | Kill grandchildren (children's children)                 |
| -kcp    | Kill child processes                                     |
| -kst    | Kill all descendants in subtree (in creation order)      |
| -dst    | Send `SIGSTOP` to all descendants                        |
| -dct    | Send `SIGCONT` to stopped descendants                    |
| -krp    | Kill root process                                        |
| -mmd    | Print descendant(s) using most memory                    |
| -mpd    | Print descendant(s) with maximum CPU ticks               |

**Special commands (require only one flag):**

| Option  | Description                                        |
|---------|-----------------------------------------------------|
| -bcp    | Print number of processes under any `bash` subtree  |
| -bop    | Print number of processes NOT under any `bash`      |

---

## Examples

- **Print depth of PID 1234 in process tree rooted at 1:**
  ```bash
  ./proctree 1 1234 -dpt
  ```
- **Count all descendants of 2345:**
  ```bash
  ./proctree 1 2345 -cnt
  ```
- **Kill siblings of process 5678:**
  ```bash
  ./proctree 1 5678 -ksp
  ```
- **Number of processes under `bash`:**
  ```bash
  ./proctree -bcp
  ```

---

## Notes
- The program performs safety checks (will not kill `init`/`bash`).
- Use caution with commands that _kill_ or _signal_ processes!
- Some commands (like kill) require appropriate permissions.
- If a process does not exist or is not part of the subtree rooted at `<rootpid>`, an error message is shown.

---

## Structure & Functions
- **ProcList, ProcInfo**: Dynamic structures for storing and managing process data.
- **scanprocfs**: Parses `/proc` for the current snapshot of processes.
- **Various handle_* functions**: Implement the functionality for each command/option.
- **Safety helpers**: Prevent termination of essential system processes

---

## License
This project is for educational purposes. Modify and use at your own risk.
