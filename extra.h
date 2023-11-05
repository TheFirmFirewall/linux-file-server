#include <string.h>
#include <sys/stat.h>

void get_filename_ext(const char *filename, char *ext) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) *ext = '\0';
    else sprintf(ext, dot+1);
}

double isFILE(const char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode) ? path_stat.st_size : 0;
}

int isDIR(const char *path) {
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}

int min(int a, int b) {
    return a < b ? a : b;
}