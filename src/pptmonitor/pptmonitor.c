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


//#define PPT_DEBUG
void get_nic_ip(char *nic_ip[], int nic_num); 
int get_pos(char *line, char *mark);
int get_colon_pos(char *line, int start_pos, int end_pos);
unsigned int get_port(char *line, int start_pos);
unsigned int get_ipv6_port(char *line, int start_pos, int end_pos);
unsigned long get_pid(char *line, int start_pos);
void get_local_port_to_pid(unsigned long port_to_pid[NIC_NUM][PORT_NUM], char *cmd, char *nic_ip[]);
void print_port_to_pid(unsigned long port_to_pid[NIC_NUM][PORT_NUM], char *nic_ip[]);

//TODO: should we rule out ports with LISTEN state? 
int main(int argc, char *argv[]) {
#ifdef PPT_DEBUG
	unsigned long *shared_port_to_pid;
	char *pp_name = "/port_to_pid";
	int pp_shm_fd;
#endif
	
	unsigned long local_port_to_pid[NIC_NUM][PORT_NUM];
	unsigned long pp_table_size = sizeof(unsigned long)*NIC_NUM*PORT_NUM;
	int fd, ret;
	unsigned long update_interval = DEFAULT_UPDATE_INTERVAL;
//	memset(local_port_to_pid, 0, pp_table_size);

#ifdef PPT_DEBUG
	pp_shm_fd = shm_open(pp_name, O_CREAT | O_RDWR, 0666);
	ftruncate(pp_shm_fd, pp_table_size);
	shared_port_to_pid = (unsigned long *) mmap(NULL, pp_table_size, 
			PROT_READ | PROT_WRITE, MAP_SHARED, pp_shm_fd, 0);
#endif

	if (argc < 2) {
		printf("Please specify the netstat command. Abort.\n");
	}

	fd = open("/dev/ExaOdevice", O_RDWR);             // Open the device with read/write access
   	if (fd < 0){
		perror(NULL);
		printf("Fail to open ExaOdevice. Abort.\n");
	}

	if (argc == 4) {
		if (!strcmp(argv[2], "-time")){
			update_interval = strtol(argv[3], NULL, 10);
		}
	}

	while(1) {
		//flush the table every time. 
		//TODO: is there a better way?
		memset(local_port_to_pid, 0, pp_table_size);
		get_local_port_to_pid(local_port_to_pid, argv[1], nic_ip);

#ifdef PPT_DEBUG
		memcpy(shared_port_to_pid, local_port_to_pid, pp_table_size);
		perror(NULL);
#endif
		print_port_to_pid(local_port_to_pid, nic_ip);


		ret = write(fd, local_port_to_pid, WRITE_FULL_PORT_TO_PID);
		if (ret < 0) {
			perror(NULL);
		}
#ifdef PPT_DEBUG
		break;
#endif
		sleep(update_interval);
	}
}

int get_colon_pos(char *line, int start_pos, int end_pos) {
	int i;

	for (i = start_pos; i < end_pos; i++) {
		if (line[i] == ':') { 
			break;
		}
	}

	if (i == start_pos) {//this is an IPv6 address
		return INVALID_POS;
	}
	else {//return the position of ':'
		return i;
	}
}


unsigned int get_port(char *line, int start_pos) {
	return strtol(&line[start_pos], NULL, 10);
}

unsigned int get_ipv6_port(char *line, int start_pos, int end_pos) {
	int i;

	for (i = start_pos-1; i >= end_pos; i--) {
		if (line[i] == ':' && i ) {
			return strtol(&line[i+1], NULL, 10);
		}
	}

	return INVALID_PORT;
}


unsigned long get_pid(char *line, int start_pos) {
	unsigned long pid = strtol(&line[start_pos], NULL, 10);
	//printf("pid is %lu\n", pid);
	return pid;
}

int get_pos(char *line, char *mark) {
	char *ptr;
	int pos = INVALID_POS;
	ptr = strstr(line, mark);

	if (ptr == NULL) {
		return pos;
	}
	else {
		pos = ptr-line;
		return pos;
	}
}

void print_port_to_pid(unsigned long port_to_pid[NIC_NUM][PORT_NUM], char *nic_ip[]) {
	int i, j;
	printf("PRINT UPDATED P2P:\n");
	for (i = 0; i < NIC_NUM; i++) {
		for (j = 0; j < PORT_NUM; j++) {
			if (port_to_pid[i][j] != 0) {	
				printf("nic: %s, port: %d, pid: %lu\n", nic_ip[i], j, port_to_pid[i][j]);
			}
		}
	}
}

/* deprecated function */
void get_nic_ip(char *nic_ip[], int nic_num) {
	char *line = NULL;
	int i;
	size_t len;
	FILE *fp_nic = fopen("../common/niciplist", "r");
	struct in_addr addr;

	for (i = 0; i < nic_num; i++) {
		getline(&line, &len, fp_nic);
		char *tmp;

		/*use inet_aton and inet_ntoa to remove the extra new line character*/
		inet_aton(line, &addr);
		tmp = inet_ntoa(addr);
		nic_ip[i] = (char *) malloc(strlen(tmp)+1);
		strcpy(nic_ip[i], tmp);	
	}
}

void get_local_port_to_pid(unsigned long port_to_pid[NIC_NUM][PORT_NUM], char *cmd, char *nic_ip[]) {
	FILE *fp;
	int line_idx = 0;
	int mark_set = 0;
	char line[MAX_LINE_LENGTH];
	char *mark_local = "Local Address";
	char *mark_foreign = "Foreign Address";
	char *mark_pid = "PID"; 
	int pos_local,  pos_foreign, pos_pid;

	fp = popen(cmd, "r");
	if (fp == NULL) {
		printf("Failed to run netstat.\n");
	}

	while (fgets(line, sizeof(line), fp) != NULL) {
		line_idx++;
		if (!mark_set) {
			/*find the positions of local address, foreign address and PID*/
			pos_foreign = get_pos(line, mark_foreign);
			pos_pid = get_pos(line, mark_pid);
			pos_local = get_pos(line, mark_local);
			if ((pos_local != INVALID_POS) 
				&& (pos_foreign != INVALID_POS)
				&& (pos_pid != INVALID_POS)) {
				mark_set = 1;
#ifdef PPT_DEBUG
				printf("marks are found at line %d\n", line_idx);
				printf("pos_local: %d, pos_foreign: %d, pos_pid: %d\n", pos_local, pos_foreign, pos_pid);
#endif
			}
			else {
#ifdef PPT_DEBUG
				printf("marks are not found yet at line %d\n", line_idx);
#endif
			}
		}
		else {
			int pos_colon; 
			char *ipv4_addr;
			unsigned int port;
			unsigned long pid;
			int i;

			pos_colon = get_colon_pos(line, pos_local, pos_foreign);
			if ( pos_colon == INVALID_POS) {//this is an IPv6 address, ignore the whole line
				port = get_ipv6_port(line, pos_foreign, pos_local);
				if (port != INVALID_PORT) {
					pid = get_pid(line, pos_pid);
					for (i = 0; i < NIC_NUM; i++) {
						if (port_to_pid[i][port] == 0) {
							//only insert when this port->pid mapping does not exist in previous IPv4 entries
							port_to_pid[i][port] = pid;
						}
					}
				}
			}
			else {
				ipv4_addr = (char *) malloc(pos_colon-pos_local);
				strncpy(ipv4_addr, &line[pos_local], pos_colon-pos_local);
				port = get_port(line, pos_colon+1);
				pid = get_pid(line, pos_pid);

				if (!strcmp(ipv4_addr, "0.0.0.0")) {
					for (i = 0; i < NIC_NUM; i++) {
						port_to_pid[i][port] = pid;
					}
				}
				else {
					for (i = 0; i < NIC_NUM; i++) {
						if (!strcmp(ipv4_addr, nic_ip[i])) {
							port_to_pid[i][port] = pid;
							break;
						}
					}
				}
				free(ipv4_addr);
			}
		}
	}
}



