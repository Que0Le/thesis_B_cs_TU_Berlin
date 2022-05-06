/* https://cirosantilli.com/linux-kernel-module-cheat#mmap */

// #include <asm-generic/io.h> /* virt_to_phys */
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h> /* min */
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h> /* copy_from_user, copy_to_user */
#include <linux/slab.h>
/*  */
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/getcpu.h>
#include <linux/timekeeping.h>

#include <linux/string.h>

#include "../common.h"

static unsigned long *log_buffs[NUM_LOG_BUFF];

struct mmap_info {
	char *data;
};

static unsigned char *buff_from_here;
static unsigned char *buff_temp;
unsigned int current_index = 0; // index of (should be) next free cell
int status[MAX_PKT] = {0};

unsigned long count_pkt = 0;
unsigned long count_pkt_overflow = 0;

/* After unmap. */
static void vm_close(struct vm_area_struct *vma)
{
	pr_info("vm_close\n");
}

/* First page access. */
static vm_fault_t vm_fault(struct vm_fault *vmf)
{
	struct page *page;
	struct mmap_info *info;

	pr_info("vm_fault\n");
	info = (struct mmap_info *)vmf->vma->vm_private_data;
	if (info->data) {
		page = virt_to_page(info->data);
		get_page(page);
		vmf->page = page;
	}
	return 0;
}

/* After mmap. TODO vs mmap, when can this happen at a different time than mmap? */
static void vm_open(struct vm_area_struct *vma)
{
	pr_info("vm_open\n");
}

static struct vm_operations_struct vm_ops =
{
	.close = vm_close,
	.fault = vm_fault,
	.open = vm_open,
};

static int mmap(struct file *filp, struct vm_area_struct *vma)
{
	pr_info("mmap\n");
	vma->vm_ops = &vm_ops;
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_private_data = filp->private_data;
	vm_open(vma);
	return 0;
}

static int open(struct inode *inode, struct file *filp)
{
	struct mmap_info *info;

	pr_info("open\n");
	info = kmalloc(sizeof(struct mmap_info), GFP_KERNEL);
	pr_info("virt_to_phys = 0x%llx\n", (unsigned long long)virt_to_phys((void *)info));
	info->data = (char *)get_zeroed_page(GFP_KERNEL);
	// info->data = buff_from_here;
	memcpy(info->data, "asdf", 4);
	filp->private_data = info;
	return 0;
}

static ssize_t read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	struct mmap_info *info;
	ssize_t ret;
#ifdef DEBUG_READ_FROM_US
	printk(KERN_INFO "read: len[%zu] off[%zu]\n", len, (size_t)*off);
#endif
	if ((size_t)BUFFER_SIZE <= *off) {
		ret = 0;
	} else {
        // This 2 lines work (not neccessary) so let it be here.
		info = filp->private_data;
		ret = min(len, (size_t)BUFFER_SIZE - (size_t)*off);

        /* We return buff_temp (BUFFER_SIZE) data to userspace */
        unsigned int ci = current_index;        // remember last read index from last read() access
        memset(buff_temp, '\0', BUFFER_SIZE);
        int copied_pkts = 0;                    // how many packets did we grab to be copied to user space
        int copied_index[PKTS_PER_BUFFER];      // index of those pkt in KM buffer
        for (int i=0; i<PKTS_PER_BUFFER; i++)
            copied_index[i] = -1;
        int try = 0;                            // keep track on how many times we search for pkt in buffer
        /* We search for <PKTS_PER_BUFFER> available pkts in buffer, put it in buff_temp*/
        while (copied_pkts < PKTS_PER_BUFFER && try<MAX_PKT) {
            if (status[ci] != 0) {
                memcpy(buff_temp+copied_pkts*PKT_BUFFER_SIZE, 
                       buff_from_here + ci*PKT_BUFFER_SIZE, PKT_BUFFER_SIZE);
                copied_index[copied_pkts] = ci;
                copied_pkts += 1;
            }
            ci = (ci + 1) % MAX_PKT;
            try += 1;
        }
        /* If no pkts found in buffer, return */
        // if (copied_pkts==0) {
        //     return 0;
        // }
#ifdef DEBUG_READ_FROM_US
        printk(KERN_INFO "copied [%d] pkts\n", copied_pkts);
#endif
        /* Else, deliver to userspace and cleanup on success */
        if (copy_to_user(buf, buff_temp, BUFFER_SIZE)) {
            // unsigned long __copy_to_user (void __user * to,const void * from,unsigned long n);
            // Returns number of bytes that could not be copied. On success, this will be zero. 
            printk(KERN_INFO "copy_to_user failed!!!\n");
            ret = -EFAULT;
        } else {
            // Copy success. Clean up the KM bufffer slot(s)
            for (int i=0; i<PKTS_PER_BUFFER; i++) {
                if (copied_index[i] != -1) {
                    memset(buff_from_here + copied_index[i]*PKT_BUFFER_SIZE, '\0', PKT_BUFFER_SIZE);
                    status[copied_index[i]] = 0;
                }
            }
        }
    }
	return BUFFER_SIZE;
}

static ssize_t write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
	struct mmap_info *info;

	pr_info("write\n");
	info = filp->private_data;
	if (copy_from_user(info->data, buf, min(len, (size_t)BUFFER_SIZE))) {
		return -EFAULT;
	} else {
		return len;
	}
}

static int release(struct inode *inode, struct file *filp)
{
	struct mmap_info *info;

	pr_info("release\n");
	info = filp->private_data;
	free_page((unsigned long)info->data);
	kfree(info);
	filp->private_data = NULL;
	return 0;
}

static const struct proc_ops pops = {
	.proc_mmap = mmap,
	.proc_open = open,
	.proc_release = release,
	.proc_read = read,
	.proc_write = write,
};


/* begin net helpers */
/* rxhPacketIn function is called when a packet is received on a registered netdevice */
rx_handler_result_t rxhPacketIn(struct sk_buff **ppkt) {
    
    struct sk_buff* pkt;
    struct iphdr *ip_header;
    // int cpuid;
    // struct tcphdr *tcp_header;
    struct udphdr *udp_udphdr;
    // uint8_t *tcp_headerflags;

    pkt = *ppkt;
    ip_header = (struct iphdr *)skb_network_header(pkt);

    /* Check if IPv4 packet */
    if(ip_header->version != 4){
        // printk(KERN_INFO "[RXH] Got packet that is not IPv4\n");
        return RX_HANDLER_PASS;
    }
    
    /* If not UDP, pass the packet */
    if(ip_header->protocol != 17) {
        return RX_HANDLER_PASS;     // /usr/include/linux/in.h
    }
    // printk(KERN_INFO "pkt size=%d data_len=%d \n", pkt->len, pkt->data_len);

    /* Report CPU id, source IP, destination IP, and Protocol version */
    // cpuid = raw_smp_processor_id();
    // printk(KERN_INFO "[RXH] CPU id [%i], Source IP [%x], Destination IP [%x], Protocol [%i]\n",
    //     cpuid, ip_header->saddr, ip_header->daddr, ip_header->protocol);

    /* Parse UDP header */
    udp_udphdr = (struct udphdr *)skb_transport_header(pkt);
    if ((unsigned int)ntohs(udp_udphdr->dest) != 8080) {
        return RX_HANDLER_PASS;
    }
    
    u64 now = ktime_get_ns();
    unsigned long uid = 0;
    count_pkt += 1;

    /* Write timestamp to log */
    memcpy(&uid, pkt->data+28+8, sizeof(unsigned long));
    if (uid<MAX_LOG_ENTRY && uid>=0) {
#ifdef DEBUG_KM_INCOMING_PACKETS
        printk(KERN_INFO "uid[%lu] now[%llu]\n", uid, now);
#endif
        unsigned int buff_index = uid / MAX_ENTRIES_PER_LOG_BUFF;
        log_buffs[buff_index][uid % MAX_ENTRIES_PER_LOG_BUFF] = now;
    }

    /* Write data into the module's cache */
    unsigned int try = 0;
    unsigned int write_index = current_index;
    void *pos = 0;
    while (try<KM_FIND_BUFF_SLOT_MAX_TRY) {
        if (status[write_index] == 0) {
            pos = buff_from_here + write_index*PKT_BUFFER_SIZE;
            // Copy packet (14 ethernet already tripped off till this stage) 20 ip, 8 udp
            memcpy(pos, pkt->data+28, (unsigned int)ntohs(udp_udphdr->len) - 8);
            // Set ks_time_arrival_2 to "now"
            // memcpy(pos+40, &now, sizeof(u64));
            status[write_index] = 1;
            current_index = (write_index + 1) % MAX_PKT;
            break;
        }
        write_index = (write_index + 1) % MAX_PKT;
        ++try;
        /* No free slot found. We will just discard the packet.
         * TODO: Should we replace old packet with new incomming? 
         */
        if (try == KM_FIND_BUFF_SLOT_MAX_TRY) {
            count_pkt_overflow += 1;
// #ifdef DEBUG_KM_INCOMING_PACKETS
            // memcpy(&uid, pkt->data+28+8, sizeof(unsigned long));
            printk(KERN_INFO "Packet uid[%lu] cannot be written in cache: No free slot!", uid);
// #endif
        }
    }

#ifdef DEBUG_KM_INCOMING_PACKETS
    printk(KERN_INFO "UDP srcPort [%u], destPort[%u], len[%u], check_sum[%u], payload_byte[%d] - pos[%d] now[%llu]\n",
        (unsigned int)ntohs(udp_udphdr->source) ,(unsigned int)ntohs(udp_udphdr->dest), 
        (unsigned int)ntohs(udp_udphdr->len), (unsigned int)ntohs(udp_udphdr->check), 
        (unsigned int)ntohs(udp_udphdr->len) - 8, write_index, now);
#endif  

    return RX_HANDLER_PASS;
}

/* This was derived from linux source code net/core/net.c .
Valid return values are RX_HANDLER_CONSUMED, RX_HANDLER_ANOTHER, RX_HANDER_EXACT, RX_HANDLER_PASS.
If your intention is to handle the packet here in your module code then you should 
return RX_HANDLER_CONSUMED, in which case you are responsible for release of skbuff
and should be done via call to kfree_skb(pkt).
*/
int registerRxHandlers(void) {
    struct net_device *device;
    int regerr;
    read_lock(&dev_base_lock);
    device = first_net_device(&init_net);
    while (device) {
        printk(KERN_INFO "[RXH] Found [%s] netdevice\n", device->name);
        /* Register only net device with name lo (loopback) */
        if(!strcmp(device->name,"ens33")) {
            rtnl_lock();
            regerr = netdev_rx_handler_register(device,rxhPacketIn,NULL);
            rtnl_unlock();
            if(regerr) {
                printk(KERN_INFO "[RXH] Could not register handler with device [%s], error %i\n", device->name, regerr);
            } else {
                printk(KERN_INFO "[RXH] Handler registered with device [%s]\n", device->name);
            }
        	  device = NULL;
        } else {
            device = next_net_device(device);
	    }
    }
    read_unlock(&dev_base_lock);

    return 0;
}

void unregisterRxHandlers(void) {
    struct net_device *device;
    read_lock(&dev_base_lock);
    device = first_net_device(&init_net);
    while (device) {
        /* Unregister only lo (loopback) */
        if(!strcmp(device->name,"ens33")) {
            rtnl_lock();
            netdev_rx_handler_unregister(device);
            rtnl_unlock();
            printk(KERN_INFO "[RXH] Handler un-registered with device [%s]\n", device->name);
      	    device = NULL;
        } else {
            device = next_net_device(device);
      	}
    }
    read_unlock(&dev_base_lock);
}
/* End net helpers */

static int myinit(void)
{
    buff_from_here = (unsigned char *) kmalloc(PKT_BUFFER_SIZE*MAX_PKT, GFP_KERNEL);
    memset(buff_from_here, '\0', PKT_BUFFER_SIZE*MAX_PKT);
    buff_temp = (unsigned char *) kmalloc(BUFFER_SIZE, GFP_KERNEL);
    memset(buff_temp, '\0', BUFFER_SIZE);

    /* Alloc memory for log buffers */
    unsigned long *r;
    for (int i=0; i<NUM_LOG_BUFF; i++) {
        r =  (unsigned long *) __get_free_pages(GFP_KERNEL, PAGES_ORDER);
        if (!r) {
            // error
            printk(KERN_INFO "[RXH] ERROR: __get_free_pages i[%d] of %d max_entries[%d]!\n", i, NUM_LOG_BUFF, MAX_ENTRIES_PER_LOG_BUFF);
            for (int j=0; j<i; j++) {
                free_pages((unsigned long) log_buffs[j], PAGES_ORDER);
            }
            return -1;
        }
        log_buffs[i] = r;
    }
    printk(KERN_INFO "[RXH] Allocated NUM_LOG_BUFF[%d] for MAX_LOG_ENTRY[%d] packets!\n", 
                NUM_LOG_BUFF, MAX_LOG_ENTRY);
    /* Create proc file */
	proc_create(proc_filename, 0, NULL, &pops);
    /*  */
    int i = 0;
    printk(KERN_INFO "[RXH] Kernel module loaded!\n");
    i=registerRxHandlers();
    /*  */

	return 0;
}

static void myexit(void)
{
    unregisterRxHandlers();
    printk(KERN_INFO "[RXH] Kernel module unloaded.\n");

    /* write log buffers in file */
    unsigned long zeroed = 0;
    printk(KERN_INFO "[RXH] Kernel module exporting log");
    struct file *fp; 
    mm_segment_t fs; 
    loff_t pos_file; 
    fp = filp_open(path_km, O_RDWR | O_CREAT, 0777); 
    if (IS_ERR(fp)) { 
        printk("create file error\n"); 
        return; 
    } 
    fs = get_fs(); 
    set_fs(KERNEL_DS); 
    pos_file = 0;
    char temp[128];
    for (unsigned long i=0; i<MAX_LOG_ENTRY; i++) {
        memset(temp, '\0', 128);
        unsigned int buff_index = i / MAX_ENTRIES_PER_LOG_BUFF;
        snprintf(temp, 100, "%lu\n", log_buffs[buff_index][i % MAX_ENTRIES_PER_LOG_BUFF]);
        vfs_write(fp, temp, strlen(temp), &pos_file);
        if (log_buffs[buff_index][i % MAX_ENTRIES_PER_LOG_BUFF] == 0)
            zeroed += 1;
    }
    filp_close(fp, NULL); 
    set_fs(fs);
    printk(KERN_INFO "---------------------------------------\n");
    printk(KERN_INFO "Total count_pkts[%lu]\n", count_pkt);
    printk(KERN_INFO "Count_pkt_overflow: %lu\n", count_pkt_overflow);
    printk(KERN_INFO "Zeroed: %lu\n", zeroed);
    printk(KERN_INFO "---------------------------------------\n");

    /* Free memory */
    remove_proc_entry(proc_filename, NULL);
    kfree(buff_from_here);
    kfree(buff_temp);
    
    for (int i=0; i<NUM_LOG_BUFF; i++) {
        free_pages((unsigned long) log_buffs[i], PAGES_ORDER);
    }
}

module_init(myinit)
module_exit(myexit)
MODULE_LICENSE("GPL");
