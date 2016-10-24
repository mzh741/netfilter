#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/stat.h>        /* For mode constants */

#include "pptmonitor.h"
#include "../common/sysspec.h"

int main() {
	char *pp_name = "/port_to_pid";
	int pp_shm_fd;
	unsigned long *port_to_pid;
	int i, j;

/*	port_to_pid = malloc(sizeof(unsigned long *)*NIC_NUM);
	memset(shared_port_to_pid, 0, sizeof(unsigned long)*NIC_NUM*PORT_NUM);
	for (i = 0; i < NIC_NUM; i++) {
		shared_port_to_pid[i] = malloc(sizeof(unsigned long)*PORT_NUM);
	}
*/

	pp_shm_fd = shm_open(pp_name, O_RDONLY, 0666);
	if (pp_shm_fd == -1) {
		printf("shared memory failed\n");
		perror(NULL);
		exit(-1);
	}

	port_to_pid = (unsigned long *) mmap(NULL, sizeof(unsigned long)*NIC_NUM*PORT_NUM, 
			PROT_READ, MAP_SHARED, pp_shm_fd, 0);
	if (port_to_pid == MAP_FAILED) {
		printf("Map failed\n");
		exit(-1);
	}

	for (i = 0; i < NIC_NUM; i++) {
		for (j = 0; j < PORT_NUM; j++) {
			//printf("checking %d %d\n", i, j);
			if (*(port_to_pid+i*PORT_NUM+j) != 0) {
				printf("i=%d, j=%d, pid-%lu\n", i, j, *(port_to_pid+i*PORT_NUM+j));
			}
		/*	if (shared_port_to_pid[i][j] !=0) {
				printf("i=%d, j=%d, pid-%lu\n", i, j, shared_port_to_pid[i][j]);
			}
		*/
		}
	}

	if (shm_unlink(pp_name) == -1) {
		printf("Error removing %s\n", pp_name);
		exit(-1);
	}

	return 0;
}





