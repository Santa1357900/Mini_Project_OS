#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static const char *pattern;
static bool ignore_case;
static bool files_only;

static double elapsed_seconds(const struct timespec *start, const struct timespec *end) {
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000000.0;
}

static bool matches(const char *name) {
    if (!ignore_case) return strstr(name, pattern) != NULL;
    for (const unsigned char *s = (const unsigned char *)name; *s; ++s) {
        const unsigned char *a = s, *b = (const unsigned char *)pattern;
        while (*a && *b) {
            unsigned char x = *a, y = *b;
            if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
            if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
            if (x != y) break;
            ++a; ++b;
        }
        if (!*b) return true;
    }
    return false;
}

static char *join(const char *dir, const char *name) {
    size_t a = strlen(dir), b = strlen(name);
    bool slash = a && dir[a - 1] != '/';
    if (a > SIZE_MAX - b - (size_t)slash - 1) return NULL;
    char *out = malloc(a + b + (size_t)slash + 1);
    if (!out) return NULL;
    memcpy(out, dir, a);
    if (slash) out[a++] = '/';
    memcpy(out + a, name, b + 1);
    return out;
}

static int search_path(const char *path, const char *name, FILE *out) {
    struct stat st;
    if (lstat(path, &st) < 0) return 0; /* Skip inaccessible or removed entries. */
    if (matches(name) && (!files_only || S_ISREG(st.st_mode)) &&
        fprintf(out, "%s\n", path) < 0) return 1;
    if (!S_ISDIR(st.st_mode)) return 0; /* Do not follow symbolic links. */
    DIR *dir = opendir(path);
    if (!dir) return 0; /* A protected directory must not stop the search. */
    int failed = 0;
    struct dirent *ent;
    errno = 0;
    while ((ent = readdir(dir)) != NULL) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        char *child = join(path, ent->d_name);
        if (!child) { fprintf(stderr, "out of memory\n"); failed = 1; break; }
        failed |= search_path(child, ent->d_name, out);
        free(child);
        errno = 0;
    }
    /* Directory read errors are skipped; stdout remains paths only. */
    closedir(dir);
    return failed;
}

static int worker(const char *root, unsigned index, unsigned workers, FILE *out) {
    DIR *dir = opendir(root);
    if (!dir) { perror(root); return 1; }
    unsigned long entry = 0;
    int failed = 0;
    struct dirent *ent;
    errno = 0;
    while ((ent = readdir(dir)) != NULL) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        if (entry++ % workers != index) { errno = 0; continue; }
        char *path = join(root, ent->d_name);
        if (!path) { fprintf(stderr, "out of memory\n"); failed = 1; break; }
        failed |= search_path(path, ent->d_name, out);
        free(path);
        errno = 0;
    }
    if (errno) failed = 1;
    if (closedir(dir) < 0) failed = 1;
    if (fflush(out) == EOF) { perror("temporary output"); failed = 1; }
    return failed;
}

static void usage(const char *program) {
    fprintf(stderr, "Usage: %s [-i] [-f] [-j workers] ROOT PATTERN\n", program);
}

int main(int argc, char **argv) {
    unsigned workers = 4;
    int opt;
    while ((opt = getopt(argc, argv, "ifj:")) != -1) {
        if (opt == 'i') ignore_case = true;
        else if (opt == 'f') files_only = true;
        else if (opt == 'j') {
            char *end;
            errno = 0;
            long n = strtol(optarg, &end, 10);
            if (errno || *end || n < 1 || n > 64) { usage(argv[0]); return 2; }
            workers = (unsigned)n;
        } else { usage(argv[0]); return 2; }
    }
    if (argc - optind != 2 || !argv[optind + 1][0]) { usage(argv[0]); return 2; }
    const char *root = argv[optind];
    pattern = argv[optind + 1];
    struct stat st;
    if (stat(root, &st) < 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "ROOT must be an accessible directory: %s\n", root);
        return 2;
    }
    struct timespec started_at, finished_at;
    if (clock_gettime(CLOCK_MONOTONIC, &started_at) < 0) {
        perror("timer");
        return 1;
    }
    /* Separate output files avoid pipe deadlocks and interleaved lines. */
    FILE **results = calloc(workers, sizeof(*results));
    pid_t *pids = calloc(workers, sizeof(*pids));
    if (!results || !pids) { perror("calloc"); free(results); free(pids); return 1; }
    unsigned started = 0;
    int failed = 0;
    for (unsigned i = 0; i < workers; ++i) {
        results[i] = tmpfile();
        if (!results[i]) { perror("tmpfile"); failed = 1; break; }
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); failed = 1; break; }
        if (pid == 0) {
            for (unsigned k = 0; k < i; ++k) fclose(results[k]);
            int status = worker(root, i, workers, results[i]);
            fclose(results[i]);
            _exit(status ? 1 : 0);
        }
        pids[i] = pid;
        ++started;
    }
    for (unsigned i = 0; i < started; ++i) {
        int status;
        if (waitpid(pids[i], &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status)) failed = 1;
        rewind(results[i]);
        char buffer[8192];
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), results[i])) > 0)
            if (fwrite(buffer, 1, n, stdout) != n) { failed = 1; break; }
        if (ferror(results[i])) failed = 1;
        fclose(results[i]);
    }
    if (started < workers && results[started]) fclose(results[started]);
    free(results);
    free(pids);
    if (fflush(stdout) == EOF) failed = 1;
    if (clock_gettime(CLOCK_MONOTONIC, &finished_at) < 0) {
        perror("timer");
        return 1;
    }
    fprintf(stderr, "Time: %.3f s\n", elapsed_seconds(&started_at, &finished_at));
    return failed ? 1 : 0;
}
