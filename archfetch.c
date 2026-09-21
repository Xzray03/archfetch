/*
 * Program: Archfetch
 * Language ; C
 * Author: Xzrayツ
 *
 * Description:
 * A system information fetcher tool for Arch Linux with KDE Plasma.
 * This version is specifically designed for Arch Linux distributions
 * running the KDE Plasma desktop environment. It fetches and displays
 * system information including hardware specs, software versions, and
 * system configuration.
 *
 * The program can run in two modes:
 * 1. Normal mode: Displays system information with ASCII art
 * 2. Configuration mode (-fetch): Saves current system configuration to file
 *
 * Note: This version is currently only compatible with Arch Linux + KDE Plasma.
 */

/* ============================================================================== */


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>

/* Buffer size for string operations */
#define BUF 512

/* Configuration directory and file paths */
#define CONF_DIR ".config/archfetch"
#define CONF_FILE "fetch.conf"



/* ================= UTILITY FUNCTIONS ================= */

/**
 * trim - Removes leading and trailing whitespace from a string
 * @s: Input string to trim
 * Returns: Pointer to the trimmed string
 */
static char *trim(char *s) {
    /* Skip leading whitespace */
    while (isspace(*s)) s++;

    /* Remove trailing whitespace */
    char *e = s + strlen(s) - 1;
    while (e > s && isspace(*e)) *e-- = 0;

    return s;
}

/**
 * read_first_line - Reads the first line from a file
 * @path: Path to the file
 * @out: Buffer to store the read line
 * Returns: 1 on success, 0 on failure
 */
static int read_first_line(const char *path, char *out) {
    FILE *f = fopen(path, "r");
    if (!f) {
        out[0] = 0;
        return 0;
    }

    fgets(out, BUF, f);
    fclose(f);

    /* Remove newline character if present */
    out[strcspn(out, "\n")] = 0;
    return 1;
}

/**
 * run_cmd - Executes a shell command and captures its output
 * @cmd: Command to execute
 * @out: Buffer to store command output
 * Returns: 1 on success, 0 on failure
 */
static int run_cmd(const char *cmd, char *out) {
    FILE *p = popen(cmd, "r");
    if (!p) {
        out[0] = 0;
        return 0;
    }

    fgets(out, BUF, p);
    pclose(p);

    /* Remove newline character if present */
    out[strcspn(out, "\n")] = 0;
    return 1;
}



/* ================= SYSTEM INFORMATION DETECTION ================= */

/**
 * get_user_host - Gets username and hostname in format "user@host"
 * @out: Buffer to store the result
 */
static void get_user_host(char *out) {
    char *p = out;
    size_t left = BUF;

    /* Get username */
    if (getlogin_r(p, left) != 0 || !*p) {
        *p = '?';
        p[1] = '\0';
    }

    size_t len = strlen(p);
    p += len;
    left -= len;

    /* Add '@' separator */
    if (left > 1) {
        *p++ = '@';
        left--;
    }

    /* Get hostname */
    if (left && gethostname(p, left) != 0)
        *p = '\0';
}

/**
 * get_os - Gets operating system information
 * @out: Buffer to store OS information
 *
 * Reads from /etc/os-release and uname to get detailed OS info
 * including architecture and CPU type
 */
static void get_os(char *out) {
    char name[128] = "";

    /* Read OS name from /etc/os-release */
    FILE *f = fopen("/etc/os-release", "r");
    if (f) {
        char l[256];
        while (fgets(l, sizeof l, f)) {
            if (!strncmp(l, "NAME=", 5)) {
                strcpy(name, l + 6); /* Skip "NAME=" and quote */
                name[strlen(name) - 2] = 0; /* Remove trailing quote */
            }
        }
        fclose(f);
    }

    /* Get system architecture */
    struct utsname u;
    uname(&u);

    /* Map architecture to more readable CPU names */
    const char *cpu = !strcmp(u.machine, "x86_64") ? "AMD64" :
    !strcmp(u.machine, "aarch64") ? "ARM64" : u.machine;

    /* Format: OS Name Architecture (CPU Type) */
    snprintf(out, BUF, "%s %s (%s)", name, u.machine, cpu);
}

/**
 * get_host - Gets system product name and version from DMI
 * @out: Buffer to store host information
 *
 * Reads product information from /sys/devices/virtual/dmi/id/
 */
static void get_host(char *out) {
    char product_name[128], product_version[128];

    /* Read product information from DMI */
    read_first_line("/sys/devices/virtual/dmi/id/product_name", product_name);
    read_first_line("/sys/devices/virtual/dmi/id/product_version", product_version);

    /* Format based on which string is longer */
    if (strlen(product_name) < strlen(product_version))
        snprintf(out, BUF, "%s (%s)", product_version, product_name);
    else
        snprintf(out, BUF, "%s (%s)", product_name, product_version);
}

/**
 * get_kernel - Gets kernel release version
 * @out: Buffer to store kernel version
 */
static void get_kernel(char *out) {
    struct utsname u;
    uname(&u);
    strcpy(out, u.release);
}

/**
 * get_uptime - Calculates and formats system uptime
 * @out: Buffer to store formatted uptime
 *
 * Reads from /proc/uptime and converts seconds to human-readable format
 * showing years, months, weeks, days, hours, minutes
 */
static void get_uptime(char *out) {
    double uptime;
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) {
        strcpy(out, "unknown");
        return;
    }

    fscanf(f, "%lf", &uptime);
    fclose(f);

    /* Pre-calculate */
    long secs = (long)uptime;
    const long data[] = {
        secs / 31536000,              /* years */
        (secs % 31536000) / 2592000,  /* months */
        (secs % 2592000) / 604800,    /* weeks */
        (secs % 604800) / 86400,      /* days */
        (secs % 86400) / 3600,        /* hours */
        (secs % 3600) / 60,           /* minutes */
        secs % 60                     /* seconds */
    };

    const char *labels[] = {"years", "months", "weeks", "days",
        "hours", "mins", "seconds"};

        /* Build formatted string */
        char *p = out;
        for (int i = 0; i < 7; i++) {
            if (data[i] > 0) {
                p += snprintf(p, BUF - (p - out), "%ld %s, ", data[i], labels[i]);
            }
        }

        /* Remove trailing */
        if (p > out) p[-2] = '\0';
}

/**
 * pkg_filter - Filter function for scandir to select package directories
 * @e: Directory entry
 * Returns: 1 if entry is a directory and not hidden, 0 otherwise
 */
static int pkg_filter(const struct dirent *e) {
    return e->d_type == DT_DIR && e->d_name[0] != '.';
}

/**
 * get_package_count - Counts installed pacman packages
 * @count: Pointer to store package count
 *
 * Counts directories in /var/lib/pacman/local (each represents a package)
 */
static void get_package_count(int *count) {
    struct dirent **namelist;
    int n = scandir("/var/lib/pacman/local", &namelist, pkg_filter, NULL);

    if (n < 0) {
        *count = 0;
        return;
    }

    *count = n;

    /* Free allocated memory */
    while (n--)
        free(namelist[n]);
    free(namelist);
}

/**
 * get_pacman_version - Gets pacman package manager version
 * @out: Buffer to store pacman version
 */
static void get_pacman_version(char *out) {
    run_cmd("pacman --version | grep -o 'Pacman v[^ ]*'", out);
}

/**
 * get_total_memory - Gets total system memory in MB
 * @total_mb: Pointer to store total memory in MB
 *
 * Reads from /proc/meminfo to get MemTotal
 */
static void get_total_memory(long *total_mb) {
    FILE *f = fopen("/proc/meminfo", "r");
    long total_kb = 0;
    char line[256];

    if (f) {
        while (fgets(line, sizeof line, f)) {
            if (strstr(line, "MemTotal:")) {
                sscanf(line, "MemTotal: %ld kB", &total_kb);
                break;
            }
        }
        fclose(f);
    }

    *total_mb = total_kb / 1024; /* Convert KB to MB */
}

/**
 * get_memory_usage - Calculates memory usage statistics
 * @out: Buffer to store formatted memory usage
 * @total_mb: Total system memory in MB
 *
 * Calculates used memory based on MemAvailable from /proc/meminfo
 */
static void get_memory_usage(char *out, long total_mb) {
    FILE *f = fopen("/proc/meminfo", "r");
    long avail_kb = 0;

    if (f) {
        char key[32];
        long value;
        char unit[8];

        /* Parse meminfo file for available memory */
        while (fscanf(f, "%31s %ld %7s", key, &value, unit) == 3) {
            if (strcmp(key, "MemAvailable:") == 0) {
                avail_kb = value;
                break;
            }
        }
        fclose(f);
    }

    /* Calculate used and available memory */
    long avail_mb = avail_kb >> 10; /* Divide by 1024 using bit shift */
    long used_mb  = total_mb - avail_mb;

    snprintf(out, BUF, "%ldMiB / %ldMiB", used_mb, total_mb);
}

/**
 * get_shell - Gets current shell and its version
 * @out: Buffer to store shell information
 */
static void get_shell(char *out) {
    const char *s = getenv("SHELL");
    const char *shell_name;
    char cmd[256];
    char version[256] = "";
    FILE *fp;

    /* Extract shell name from path */
    shell_name = strrchr(s, '/');
    shell_name = shell_name ? shell_name + 1 : s;

    /* Get shell version */
    snprintf(cmd, sizeof(cmd),
             "%s --version 2>/dev/null | head -n1", s);
    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(version, sizeof(version), fp)) {
            version[strcspn(version, "\n")] = '\0';
        }
        pclose(fp);
    }

    snprintf(out, BUF, "%s - %s",
             shell_name,
             version[0] ? version : "version unknown");
}

/**
 * get_resolution - Gets primary display resolution
 * @out: Buffer to store resolution
 *
 * Reads from /sys/class/drm/cardX/modes for display modes
 */
static void get_resolution(char *out) {
    run_cmd("cat /sys/class/drm/card*-*/modes 2>/dev/null | head -1", out);
}

/**
 * get_de - Gets desktop environment information (KDE Plasma specific)
 * @out: Buffer to store DE information
 *
 * Specifically detects KDE Plasma and checks if running on X11 or Wayland
 */
static void get_de(char *out) {
    char *session_type = getenv("XDG_SESSION_TYPE");
    char version[64] = "";

    /* Get KDE Plasma version if available */
    run_cmd("plasmashell --version 2>/dev/null | awk '{print $2}'", version);

    if (strlen(version))
        snprintf(out, BUF, "KDE Plasma %s (%s)", version,
                 !strcmp(session_type, "wayland") ? "Wayland" : "X11");
        else
            snprintf(out, BUF, "KDE Plasma (%s)",
                     !strcmp(session_type, "wayland") ? "Wayland" : "X11");
}

/**
 * get_compositor - Gets compositor information (KDE specific)
 * @out: Buffer to store compositor name
 *
 * Detects kwin compositor and whether it's running on X11 or Wayland
 */
static void get_compositor(char *out) {
    char *desktop = getenv("XDG_CURRENT_DESKTOP");
    char *session_type = getenv("XDG_SESSION_TYPE");

    if (desktop && !strcmp(desktop, "KDE"))
        snprintf(out, BUF, "kwin_%s",
                 !strcmp(session_type, "wayland") ? "wayland" : "x11");
        else
            out[0] = 0;
}

/**
 * get_theme - Gets current GTK theme (GTK4 specific)
 * @out: Buffer to store theme information
 *
 * Reads from ~/.config/gtk-4.0/settings.ini for GTK4 theme
 */
static void get_theme(char *out) {
    char path[BUF], line[BUF];

    /* Path to GTK4 settings */
    snprintf(path, BUF, "%s/.config/gtk-4.0/settings.ini", getenv("HOME"));
    FILE *f = fopen(path, "r");
    if (!f) {
        out[0] = 0;
        return;
    }

    /* Search for icon theme setting */
    while (fgets(line, BUF, f)) {
        if (strstr(line, "gtk-icon-theme-name")) {
            char *value = trim(strchr(line, '=') + 1);
            value[0] = toupper(value[0]); /* Capitalize first letter */
            snprintf(out, BUF, "%s (GTK 4)", value);
            break;
        }
    }
    fclose(f);
}

/**
 * get_terminal - Detects current terminal emulator
 * @out: Buffer to store terminal name
 *
 * Traverses process tree to find terminal emulator process
 */
static void get_terminal(char *out) {
    pid_t pid = getpid(), ppid;
    char path[64], buf[256];

    /* Traverse process tree upwards */
    while (pid > 1) {
        snprintf(path, sizeof(path), "/proc/%d/stat", pid);
        FILE *f = fopen(path, "r");
        if (!f) break;

        fscanf(f, "%*d %*[^)]%*c %*c %d", &ppid);
        fclose(f);

        pid = ppid;
        snprintf(path, sizeof(path), "/proc/%d/comm", pid);
        f = fopen(path, "r");
        if (!f) break;

        if (!fgets(out, BUF, f)) {
            fclose(f);
            break;
        }
        fclose(f);

        out[strcspn(out, "\n")] = 0;

        /* Skip shell processes, look for terminal emulator */
        if (strcmp(out, "bash") &&
            strcmp(out, "zsh")  &&
            strcmp(out, "fish") &&
            strcmp(out, "sh")   &&
            strcmp(out, "dash"))
            break;
    }

    /* Capitalize first letter if lowercase */
    if (out[0] >= 'a' && out[0] <= 'z')
        out[0] = out[0] - 'a' + 'A';
}

/**
 * get_cpu - Gets CPU model information
 * @out: Buffer to store CPU information
 *
 * Reads from /proc/cpuinfo to get model name
 */
static void get_cpu(char *out) {
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        out[0] = 0;
        return;
    }

    char line[BUF];
    while (fgets(line, BUF, f))
        if (strstr(line, "model name")) {
            strcpy(out, strchr(line, ':') + 2); /* Skip ": " */
            out[strcspn(out, "\n")] = 0; /* Remove newline */
            break;
        }
        fclose(f);
}

/**
 * get_gpu - Gets GPU information using glxinfo
 * @out: Buffer to store GPU information
 *
 * Uses glxinfo command to get GPU device name
 */
static void get_gpu(char *out) {
    run_cmd("glxinfo | grep 'Device:' | head -1 | sed 's/.*: //' | sed 's/ (.*//'", out);
}



/* ================= CONFIGURATION MANAGEMENT ================= */

/**
 * conf_path - Constructs full path to configuration file
 * @out: Buffer to store configuration file path
 */
static void conf_path(char *out) {
    snprintf(out, BUF, "%s/%s/%s", getenv("HOME"), CONF_DIR, CONF_FILE);
}

/**
 * conf_exists - Checks if configuration file exists
 * Returns: 1 if file exists, 0 otherwise
 */
static int conf_exists(void) {
    char path[BUF];
    conf_path(path);
    return access(path, F_OK) == 0;
}

/**
 * conf_write - Writes system configuration to file
 *
 * Saves current system information to configuration file for faster
 * display in subsequent runs
 */
static void conf_write(
    const char *os, const char *host, const char *kernel,
    const char *de, const char *comp, const char *theme,
    const char *cpu, const char *gpu, const char *pacman_version,
    long total_memory, const char *resolution
) {
    char dir[BUF], path[BUF];

    /* Create configuration directory if it doesn't exist */
    snprintf(dir, BUF, "%s/%s", getenv("HOME"), CONF_DIR);
    mkdir(dir, 0755);

    /* Write configuration to file */
    conf_path(path);
    FILE *f = fopen(path, "w");
    fprintf(f,
            "OS=%s\nHOST=%s\nKERNEL=%s\nDE=%s\nCOMPOSITOR=%s\n"
            "THEME=%s\nCPU=%s\nGPU=%s\nPACMAN_VERSION=%s\n"
            "TOTAL_MEMORY=%ld\nRESOLUTION=%s\n",
            os, host, kernel, de, comp, theme, cpu, gpu,
            pacman_version, total_memory, resolution
    );
    fclose(f);
}

/**
 * conf_read - Reads system configuration from file
 *
 * Loads previously saved system information from configuration file
 */
static void conf_read(
    char *os, char *host, char *kernel,
    char *de, char *comp, char *theme,
    char *cpu, char *gpu, char *pacman_version,
    long *total_memory, char *resolution
) {
    char path[BUF], line[BUF];
    conf_path(path);

    FILE *f = fopen(path, "r");
    if (!f) return;

    /* Parse configuration file line by line */
    while (fgets(line, sizeof line, f)) {
        char *value = strchr(line, '=');
        if (!value) continue;

        *value++ = '\0';
        value[strcspn(value, "\n")] = '\0';

        /* Match key and store value in appropriate variable */
        switch (line[0]) {
            case 'O':
                if (!strcmp(line, "OS")) strcpy(os, value);
                break;
            case 'H':
                if (!strcmp(line, "HOST")) strcpy(host, value);
                break;
            case 'K':
                if (!strcmp(line, "KERNEL")) strcpy(kernel, value);
                break;
            case 'D':
                if (!strcmp(line, "DE")) strcpy(de, value);
                break;
            case 'C':
                if (!strcmp(line, "CPU")) strcpy(cpu, value);
                else if (!strcmp(line, "COMPOSITOR")) strcpy(comp, value);
                break;
            case 'T':
                if (!strcmp(line, "THEME")) strcpy(theme, value);
                else if (!strcmp(line, "TOTAL_MEMORY")) *total_memory = atol(value);
                break;
            case 'G':
                if (!strcmp(line, "GPU")) strcpy(gpu, value);
                break;
            case 'P':
                if (!strcmp(line, "PACMAN_VERSION")) strcpy(pacman_version, value);
                break;
            case 'R':
                if (!strcmp(line, "RESOLUTION")) strcpy(resolution, value);
                break;
        }
    }
    fclose(f);
}



/* ================= MAIN FUNCTION ================= */

/**
 * main - Program entry point
 *
 * Two modes of operation:
 * 1. Normal mode: Display system information with ASCII art
 * 2. Configuration mode (-fetch): Save current system config to file
 */
int main(int argc, char **argv) {
    /* Buffers for system information */
    char user_host[BUF], os[BUF], host[BUF], kernel[BUF], uptime[BUF];
    char shell[BUF], resolution[BUF], de[BUF], compositor[BUF], theme[BUF], terminal[BUF];
    char cpu[BUF], gpu[BUF], memory[BUF], pacman_version[BUF];
    int package_count;
    long total_memory;

    /* Configuration mode: save current system information */
    if (argc == 2 && !strcmp(argv[1], "-fetch")) {
        long mem_total;
        char res[BUF];

        /* Gather all system information */
        get_os(os);
        get_host(host);
        get_kernel(kernel);
        get_de(de);
        get_compositor(compositor);
        get_theme(theme);
        get_cpu(cpu);
        get_gpu(gpu);
        get_pacman_version(pacman_version);
        get_total_memory(&mem_total);
        get_resolution(res);

        /* Save to configuration file */
        conf_write(os, host, kernel, de, compositor, theme, cpu, gpu,
                   pacman_version, mem_total, res);
        return 0;
    }

    /* Normal mode: display system information */

    /* Check if configuration exists */
    if (!conf_exists()) {
        puts("fetch.conf not found");
        puts("Run: archfetch -fetch");
        return 1;
    }

    /* Load saved configuration */
    conf_read(os, host, kernel, de, compositor, theme, cpu, gpu,
              pacman_version, &total_memory, resolution);

    /* Gather dynamic information that changes frequently */
    get_user_host(user_host);
    get_uptime(uptime);
    get_package_count(&package_count);
    get_shell(shell);
    get_terminal(terminal);
    get_memory_usage(memory, total_memory);

    /* Display system information with ASCII art and color*/
    printf(
        "\033[36m                   -`                    \033[31m%s\n"
        "\033[36m                  .o+`\n"
        "\033[36m                 `ooo/                   --------------------\n"
        "\033[36m                `+oooo:                  OS: \033[0m%s\n"
        "\033[36m               `+oooooo:                 Host: \033[0m%s\n"
        "\033[36m               -+oooooo+:                Kernel: \033[0m%s\n"
        "\033[36m             `/:-:++oooo+:               Uptime: \033[0m%s\n"
        "\033[36m            `/++++/+++++++:              Packages: \033[0m%d (%s)\n"
        "\033[36m           `/++++++++++++++:             Shell: \033[0m%s\n"
        "\033[36m          `/+++ooooooooooooo/`           Resolution: \033[0m%s\n"
        "\033[36m         ./ooosssso++osssssso+`          DE: \033[0m%s\n"
        "\033[36m        .oossssso-````/ossssss+`         Compositor: \033[0m%s\n"
        "\033[36m       -osssssso.      :ssssssso.        Theme: \033[0m%s\n"
        "\033[36m      :osssssss/        osssso+++.       Terminal: \033[0m%s\n"
        "\033[36m     /ossssssss/        +ssssooo/-       CPU: \033[0m%s\n"
        "\033[36m   `/ossssso+/:-        -:/+osssso+-     GPU: \033[0m%s\n"
        "\033[36m  `+sso+:-`                 `.-/+oso:    Memory: \033[0m%s\n\n",
        user_host, os, host, kernel, uptime, package_count, pacman_version,
        shell, resolution, de, compositor, theme, terminal, cpu, gpu, memory
    );

    /* Display color blocks for visual appeal */
    puts("                                        \033[40m   \033[41m   \033[43m   \033[42m   \033[44m   \033[45m   \033[0m");
    puts("                                        \033[100m   \033[101m   \033[103m   \033[102m   \033[104m   \033[105m   \033[0m");

    return 0;
}
