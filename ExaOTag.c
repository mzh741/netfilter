#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <net/ip.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>

static struct nf_hook_ops nfho;
struct sk_buff *sock_buff;
struct iphdr *ip_header;

unsigned int hook_func(unsigned int hooknum, 
		//struct sk_buff **skb,
		struct sk_buff *skb,  
		const struct net_device *in, 
		const struct net_device *out, 
		int (*okfn)(struct sk_buff *)) {
	//sock_buff = *skb;
	sock_buff = skb; 
	ip_header = (struct iphdr *)skb_network_header(sock_buff);

	if (ip_header->protocol == 6 && 
			ip_header->saddr == 0x0100007f &&
			ip_header->daddr == 0x0100007f) { //TEST: an UDP packet
		printk(KERN_INFO "CHECKSUM before: %x", ip_header->check);
		ip_header->tos = 0x0b;
		ip_send_check(ip_header);
		printk(KERN_INFO "CHECKSUM after: %x", ip_header->check);

		printk(KERN_INFO "ExaO filter got a self-outgoing udp packet with SADDR: %pI4 and ADDR: %pI4 \n ", 
				&ip_header->saddr, &ip_header->daddr);     //log we’ve got udp packet to /var/log/messages
//		printk(KERN_INFO "ExaO filter got a self-outgoing udp packet with SADDR: %x and ADDR: %x \n ", 
//				ip_header->saddr, ip_header->daddr);     //log we’ve got udp packet to /var/log/messages
	}

	return NF_ACCEPT;
}

int init_module() {
	nfho.hook = hook_func;
//	nfho.hooknum = NF_INET_LOCAL_OUT;
	nfho.hooknum = NF_INET_POST_ROUTING;
	nfho.pf = PF_INET;
	nfho.priority = NF_IP_PRI_FIRST;
					 
	nf_register_hook(&nfho);
						       
	return 0;
}

void cleanup_module() {
	nf_unregister_hook(&nfho);     
}






















