#ifndef __NICNUM__
#define __NICNUM__


#define NIC_NUM  2
#define PORT_NUM 65536
#define PID_NUM  32768 

#define MAX_TAG 255
#define NULL_TAG 0

#define WRITE_FULL_PORT_TO_PID PID_NUM+1
#define WRITE_FULL_PID_TO_TAG PID_NUM+2
char *nic_ip[NIC_NUM] = {
	"10.0.2.15",
	"127.0.0.1",
	};
#endif
