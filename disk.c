#include "disk.h"
#include <sys/statvfs.h>
#include <stdlib.h>
#include <stdio.h>
#include "defs.h"

#define DISK_LOG "DISK:"

double disk_getCapacity(char *dev_path) {
        unsigned long long result = 0;
        double f_cap = 0.0;
        struct statvfs sfs;

        if (statvfs(dev_path, &sfs) != -1) {
                result = (unsigned long long)sfs.f_bsize * sfs.f_blocks;
        }
        if (result > 0) {
                f_cap = (double)result/(1024*1024);
        }
        return f_cap;
}

double disk_getFreeSpace(char *dev_path) {
        unsigned long long result = 0;
        double f_cap = 0.0;
        struct statvfs sfs;

        if (statvfs (dev_path, &sfs) != -1) {
                result = (unsigned long long)sfs.f_bsize * sfs.f_bfree;
        }
        if (result > 0)
        {
                f_cap = (double)result/(1024*1024);
        }
        return f_cap;
}

int disk_usedSpace(char *dev_path) {
	struct statvfs sfs;
	float result = 0.0;

	if (statvfs (dev_path, &sfs) != -1) {
		result = (sfs.f_blocks - sfs.f_bfree) / (double)(sfs.f_blocks - sfs.f_bfree + sfs.f_bavail) * 100.0;
		LOG(DISK_LOG "disk usage = %f\n",result);
        }

	return (int)result;
}

int disk_removeLogs(void) {
	char command[60] = { 0 };

	/* Por hora, o caminho dos arquivos de log estao hardcoded
	 * TODO: mudar essa funcao para que os caminhos venham por
	 * linha de comando.
	 */
	sprintf(command,"rm -f /var/www/serial_service/debug.txt");
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-result"
  		system(command);
  #pragma GCC diagnostic pop

	sprintf(command,"rm -f /var/www/equalizer-api/equalizer-api/debug_web.txt");
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-result"
  		system(command);
  #pragma GCC diagnostic pop

	return 0;
}
