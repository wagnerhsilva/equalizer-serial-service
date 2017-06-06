#include "disk.h"
#include <sys/statvfs.h>
#include <stdio.h>

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
	double capacity = 0.0;
	double free_space = 0.0;
	double used_space = 0.0;

	/* Obtem a capacidade total do disco, em megabytes */	
	capacity = disk_getCapacity(dev_path);
	/* Obtem o espaco disponivel, em megabytes */
	free_space = disk_getFreeSpace(dev_path);
	/* Calcula o espaco ocupado, a partir da porcentagem
	 * do espaco livre */
	used_space = (1 - (free_space/capacity));
	/* Retorna a informacao em inteiro */
	return (int)(used_space * 100);
}

int disk_removeLogs(void) {
	char command[60] = { 0 };

	/* Por hora, o caminho dos arquivos de log estao hardcoded
	 * TODO: mudar essa funcao para que os caminhos venham por
	 * linha de comando.
	 */
	sprintf(command,"rm -f /var/www/serial-service/debug.txt");
	system(command);

	sprintf(command,"rm -f /var/www/equalizer-api/equalizer-api/debug_web.txt");
	system(command);

	return 0;
}

