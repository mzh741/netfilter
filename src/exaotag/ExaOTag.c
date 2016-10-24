#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/string.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <asm/uaccess.h>

#include "../common/sysspec.h"

//#define DEBUG
#define DEVICE_NAME "ExaOdevice"
#define  CLASS_NAME  "ExaO"

MODULE_LICENSE("GPL");            ///< The license type -- this affects available functionality
MODULE_AUTHOR("Qiao Xiang");
MODULE_DESCRIPTION("ExaO tag device");  ///< The description -- see modinfo
MODULE_VERSION("0.1");            ///< A version number to inform users

static struct nf_hook_ops nfho;
struct sk_buff *sock_buff;
struct iphdr *ip_header;
struct tcphdr *tcp_header;
char src_ip[16];

static int    major_number;                  ///< Stores the device number -- determined automatically
static int    number_opens = 0;              ///< Counts the number of times the device is opened
static struct class*  exao_class  = NULL; ///< The device-driver class struct pointer
static struct device* exao_device = NULL; ///< The device-driver device struct pointer

unsigned long *port_to_pid;
uint8_t *pid_to_tag;
unsigned long pp_table_size = sizeof(unsigned long)*NIC_NUM*PORT_NUM;
unsigned long pt_table_size = sizeof(uint8_t)*PID_NUM;

// The prototype functions for the character driver -- must come before the struct definition
static int     exao_dev_open(struct inode *, struct file *);
static int     exao_dev_release(struct inode *, struct file *);
static ssize_t exao_dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t exao_dev_write(struct file *, const char *, size_t, loff_t *);

/* helper functions*/
static void print_port_to_pid(unsigned long *port_to_pid);
static void print_pid_to_tag(uint8_t *port_to_pid);
static int get_nic_idx(unsigned char *src_ip);

/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops =
{
	.open = exao_dev_open,
	.read = exao_dev_read,
	.write = exao_dev_write,
	.release = exao_dev_release,
};

unsigned int hook_func(unsigned int hooknum, 
		//struct sk_buff **skb,
		struct sk_buff *skb, const struct net_device *in, 
		const struct net_device *out, int (*okfn)(struct sk_buff *)) {

	uint16_t nic_idx, port;
	uint8_t tag;
	unsigned long pid;

	//sock_buff = *skb;
	sock_buff = skb; 
	ip_header = (struct iphdr *)skb_network_header(sock_buff);
	tcp_header = (struct tcphdr *)skb_transport_header(sock_buff);
	
	snprintf(src_ip, 16, "%pI4", &ip_header->saddr);
	nic_idx = get_nic_idx(src_ip);

	if (ip_header->protocol == 6 && 
		//	ip_header->saddr == 0x0100007f &&
		//	ip_header->daddr == 0x0100007f) { //TEST: an UDP packet
		nic_idx != NIC_NUM) {//TODO: post_routing, don't need to check src_ip for validness?
#ifdef DEBUG
		snprintf(src_ip, 16, "%pI4", &ip_header->saddr);
		nic_idx = get_nic_idx(src_ip);
#endif
		
		port = ntohs(tcp_header->source);
#ifdef DEBUG
		printk(KERN_INFO "port: %d\n", port);
		printk(KERN_INFO "CHECKSUM before: %x3", ip_header->check);
		printk(KERN_INFO "pid for this pkt is %lu\n", *(port_to_pid+nic_idx*PORT_NUM+port));
#endif
		
		pid = *(port_to_pid+nic_idx*PORT_NUM+port);
	
		if (pid > 0 ) {//pid 0 is reserved by system
			tag = *(pid_to_tag+pid);
		}
		else {
			tag = 0;
		}

#ifdef DEBUG
		printk(KERN_INFO "new tag is: %04x", tag);
#endif
		ip_header->tos = tag;
		ip_send_check(ip_header);

#ifdef DEBUG

		//ip_header->tos = 0x0b;
		printk(KERN_INFO "CHECKSUM after: %x", ip_header->check);
		printk(KERN_INFO "src port is %x", tcp_header->source);

		printk(KERN_INFO "ExaO filter got a self-outgoing udp packet with SADDR: %pI4 and ADDR: %pI4 \n ", 
				&ip_header->saddr, &ip_header->daddr);     //log we’ve got udp packet to /var/log/messages
#endif
	}

	return NF_ACCEPT;
}

//int init_module() {
static int  exao_init(void) {
	printk(KERN_INFO "EBBChar: Initializing the EBBChar LKM\n");

	/* Try to dynamically allocate a major number for the device -- more difficult but worth it */
	major_number = register_chrdev(0, DEVICE_NAME, &fops);
      	if (major_number<0){
		printk(KERN_ALERT "ExaO failed to register a major number\n");
		return major_number;
	}
        printk(KERN_INFO "ExaO: registered correctly with major number %d\n", major_number);

	/* Register the device class */
	exao_class = class_create(THIS_MODULE, CLASS_NAME);
   	if (IS_ERR(exao_class)){                // Check for error and clean up if there is
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "Failed to register device class\n");
		return PTR_ERR(exao_class);          // Correct way to return an error on a pointer
	}
      	printk(KERN_INFO "ExaO: device class registered correctly\n");

   	/* Register the device driver */
   	exao_device = device_create(exao_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
      	if (IS_ERR(exao_device)){               // Clean up if there is an error
		class_destroy(exao_class);           // Repeated code but the alternative is goto statements
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "Failed to create the device\n");
		return PTR_ERR(exao_device);
	}
        printk(KERN_INFO "ExaO: device class created correctly\n"); // Made it! device was initialized

	/* allocate memory for port->pid and pid->tag mapping */
	port_to_pid = (unsigned long *)kmalloc(pp_table_size, GFP_KERNEL);
	pid_to_tag = (uint8_t *)kmalloc(pt_table_size, GFP_KERNEL);
	memset(port_to_pid, 0, pp_table_size);
#ifdef DEBUG
	memset(pid_to_tag, 0x1a, pt_table_size);	
#else
	memset(pid_to_tag, 0, pt_table_size);
#endif

	/* register hook callback function */
	nfho.hook = hook_func;
//	nfho.hooknum = NF_INET_LOCAL_OUT;
	nfho.hooknum = NF_INET_POST_ROUTING;
	nfho.pf = PF_INET;
	nfho.priority = NF_IP_PRI_FIRST;
					 
	nf_register_hook(&nfho);
						       
	return 0;
}


/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int exao_dev_open(struct inode *inodep, struct file *filep){
	number_opens++;
	printk(KERN_INFO "ExaO: Device has been opened %d time(s)\n", number_opens);
	return 0;
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
//TODO: currently we don't use read function at all!
static ssize_t exao_dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
	unsigned long error_count = 0;
	
	// copy_to_user has the format ( * to, *from, size) and returns 0 on success
	error_count = copy_to_user(buffer, port_to_pid, pp_table_size);
	if (error_count==0){
		printk(KERN_INFO "ExaO: Sent %lu characters to the user\n", pp_table_size);
	        return 0;
	}
	else {
		printk(KERN_INFO "ExaO: Failed to send %lu characters to the user\n", error_count);
	        return -EFAULT;
	}
}
	     
/** @brief This function is called whenever the device is being written to from user space i.e
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the sprintf() function along with the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t exao_dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
	//sprintf(message, "%s(%zu letters)", buffer, len);   // appending received string with its length
	//size_of_message = strlen(message);                 // store the length of the stored message
	unsigned long error_count = 0;

	if (len == WRITE_FULL_PORT_TO_PID) {
		error_count = copy_from_user(port_to_pid, buffer, pp_table_size);
		if (!error_count) {
			printk(KERN_INFO "ExaO: got the latest port_to_pid with size %lu\n", pp_table_size);
			print_port_to_pid(port_to_pid);
		        return 0; 
		}
		else {
			printk(KERN_INFO "ExaO: failed to get the latest port_to_pid with error_count %lu\n", error_count);
			return error_count;
		}
	}
	else if (len == WRITE_FULL_PID_TO_TAG) {
		error_count = copy_from_user(pid_to_tag, buffer, pt_table_size);
		if (!error_count) {
			printk(KERN_INFO "ExaO: got the latest pid_to_tag with size %lu\n", pt_table_size);
			print_pid_to_tag(pid_to_tag);	
		        return 0;
		}
		else {
			printk(KERN_INFO "ExaO: fail to get the latest pid_to_tag with error_count %lu\n", error_count);
			return error_count;
		}
	}
	else if ((len < PID_NUM) && (len > 0)) {
		error_count = copy_from_user((pid_to_tag+len), buffer, sizeof(uint8_t));
		if (!error_count) {
			printk(KERN_INFO "ExaO: got the new tag of pid %lu, the tag is %d\n", len, *(pid_to_tag+len));
		        print_pid_to_tag(pid_to_tag);
			return 0;
		}
		else {
			printk(KERN_INFO "ExaO: fail to get the new tag of pid %lu\n", len);
			return error_count;
		}
	}

	printk(KERN_INFO "ExaO: invalid length of write: %lu.\n", len);
	return len;
}
	      
static int exao_dev_release(struct inode *inodep, struct file *filep){
	printk(KERN_INFO "ExaO: Device successfully closed\n");
	return 0;
}

//void cleanup_module() {
static void exao_exit(void){
	device_destroy(exao_class, MKDEV(major_number, 0));     // remove the device
   	class_unregister(exao_class);                          // unregister the device class
      	class_destroy(exao_class);                             // remove the device class
        unregister_chrdev(major_number, DEVICE_NAME);             // unregister the major number

	nf_unregister_hook(&nfho);     

	printk(KERN_INFO "ExaO: Goodbye from the LKM!\n");
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  *  identify the initialization function at insertion time and the cleanup function (as
 *   *  listed above)
 *    */
module_init(exao_init);
module_exit(exao_exit);

/*Helper functions*/

static void print_port_to_pid(unsigned long *port_to_pid) {
#ifdef DEBUG
	int i, j;
	for (i = 0; i < NIC_NUM; i++) {
		for (j = 0; j < PORT_NUM; j++) {
			if (*(port_to_pid+i*PORT_NUM+j) != 0) {
				printk(KERN_INFO "ExaO: nic: %d, port %d, pid: %lu\n", i, j, *(port_to_pid+i*PORT_NUM+j));
			}
		}
	}
#endif
}

static void print_pid_to_tag(uint8_t *pid_to_tag) {
#ifdef DEBUG
	int i;
	for (i = 0; i < PID_NUM; i++) {
		if (*(pid_to_tag+i) != 0) {
			printk(KERN_INFO "ExaO: pid: %d, tag%d\n", i, *(pid_to_tag+i));
		}
	}
#endif
}

static int get_nic_idx(unsigned char *src_ip) {
	int i;
	for (i = 0; i < NIC_NUM; i++) {
		if (!strcmp(src_ip, nic_ip[i])) {
#ifdef DEBUG
			printk(KERN_INFO "ExaO: ip: %s, nic idx: %d\n", src_ip, i);
#endif
			return i;
		}
	}
	return NIC_NUM;
}














