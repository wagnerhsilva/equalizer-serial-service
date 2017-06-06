#ifndef DISK_H_
#define DISK_H_

double disk_getCapacity(char *dev_path);
double disk_getFreeSpace(char *dev_path);
int disk_usedSpace(char *dev_path);
int disk_removeLogs(void);

#endif
