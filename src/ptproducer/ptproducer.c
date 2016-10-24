#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
//#include <sys/ipc.h> 
//#include <sys/mman.h>
//#include <sys/shm.h>
//#include <sys/socket.h>
//#include <sys/stat.h>        /* For mode constants */

#include "../common/sysspec.h"




int main(int argc, char *argv[]) {
	int fd, ret;
	unsigned long pid_idx, pid_num, pid;
	uint8_t tag = 0;
	int tmp_tag, suc_cnt = 0;	

/*	if (argc < 4) {
		printf("Missing argument.\n");
		return 1;
	}	
 
	if (argc % 2 != 0) {
		printf("pid and tag should be passed to me in pair.\n");
		return 2;
	}
*/

	if (!strcmp(argv[1], "add")) {//add
		if ((argc % 2 != 0) || (argc == 2)) {
			printf("pid and tag should be passed to me in pair.\n");
			return 1;
		}

		fd = open("/dev/ExaOdevice", O_RDWR);             // Open the device with read/write access
   		if (fd < 0){
			perror(NULL);
			printf("Fail to open ExaOdevice. Abort.\n");
		}

		pid_num = (argc - 2) / 2;

		for (pid_idx = 0; pid_idx <pid_num; pid_idx++) {
			pid = strtol(argv[pid_idx*2+2], NULL, 10);
			
			if (pid == 0) {
				printf("pid cannot be 0. Move to next.\n");
				continue;
			}

			tmp_tag = atoi(argv[pid_idx*2+3]);
			if ((tmp_tag > MAX_TAG) || (tmp_tag == NULL_TAG)) {
				printf("%d is not a valid tag. Move to next.\n", tmp_tag);
				continue;
			}
			else {
				tag = tmp_tag;
				ret = write(fd, &tag, pid);
				if (ret < 0) {
					perror(NULL);
					continue;
				}
				suc_cnt++;
			}
		}

		printf("Got %lu pid-tag pairs, and successfully add %d of them.\n", pid_num, suc_cnt);
		return 0;
	}

	else if (!strcmp(argv[1], "delete")) {
		fd = open("/dev/ExaOdevice", O_RDWR);             // Open the device with read/write access
   		if (fd < 0){
			perror(NULL);
			printf("Fail to open ExaOdevice. Abort.\n");
		}

		pid_num = argc - 2;
		tag = NULL_TAG;
		for (pid_idx = 0; pid_idx <pid_num; pid_idx++) {
			pid = strtol(argv[pid_idx+2], NULL, 10);
			
			if (pid == 0) {
				printf("pid cannot be 0. Move to next.\n");
				continue;
			}

			ret = write(fd, &tag, pid);
			if (ret < 0) {
				perror(NULL);
				continue;
			}
			suc_cnt++;
		}

		printf("Got %lu pids, and successfully delete %d of them.\n", pid_num, suc_cnt);
		return 0;
	}

	else {
		printf("Please specify add or delete in argv[1].\n");
		return 2;
	}
}
































