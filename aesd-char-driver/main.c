/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include "aesdchar.h"
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/string.h>
#include <linux/uaccess.h>
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Parth Kharade"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

/** Reference Linux Device Drivers Chapter 3 - Scull Open*/
int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *aesd_device;


    PDEBUG("open");
    /**
     * TODO: handle open
     */
    aesd_device = container_of(inode->i_cdev,struct aesd_dev,cdev);
    filp->private_data = aesd_device;
    return 0;
}
/** Reference Linux Device Drivers Chapter 3 - Scull release*/
int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    filp->private_data = NULL;
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    struct aesd_buffer_entry *ret_buf = NULL;
    struct aesd_dev *dev = filp->private_data;
    size_t byte_offset = 0;
    size_t bytes_avail = 0;
    size_t read_bytes = 0;
    int bytes_rem = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */


    retval = mutex_lock_interruptible(&dev->lock);
    if(retval!=0){
        PDEBUG("Failed to acquire mutex");
        retval = -ERESTART;
        goto exit;
    }
    
    ret_buf = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->dev_buff,*f_pos,&byte_offset);
    if(ret_buf == NULL){
        retval = 0;
        PDEBUG("Not enough bytes to read.");
        goto free_mutex;
    }

    bytes_avail = ret_buf->size - byte_offset;
    read_bytes = (bytes_avail>count)?count:bytes_avail;

    bytes_rem = copy_to_user(buf,(ret_buf->buffptr + byte_offset),read_bytes);
    if(bytes_rem!=0){
        retval = -EFAULT;
        PDEBUG("copy to user failed. Bytes remaining %d",bytes_rem);
        goto free_mutex;
    }
    retval = read_bytes;
    *f_pos+=read_bytes;
    free_mutex:
    mutex_unlock(&dev->lock);
    exit:
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    PDEBUG("Write");
    ssize_t retval = 0;
    struct aesd_dev *dev = filp->private_data;
    void *temp_realloc_ptr;
    char *offset_ptr; 
    char *temp_buff = (char *)kmalloc(count,GFP_KERNEL);
    if(count == 0){
        retval = count;
        goto exit; // If we try to realloc with 0 the pointer will be freed?
    }

    if(dev == NULL){
        PDEBUG("Device not initialised");
        retval = -ENODEV;
        goto exit;
    }

    retval = copy_from_user(temp_buff,buf,count);
    if(retval != 0){
        PDEBUG("Failed to copy to kernel buffer");
        goto exit;
    } 
    PDEBUG("Copied into temp buff");

    /**
     * Search for a new line and decide how many bytes to copy.
    */
    const char *newline_ptr = memchr(temp_buff,'\n',count);
    size_t bytes_to_copy = count;
    if(newline_ptr !=NULL){
        bytes_to_copy = newline_ptr-temp_buff + 1;
    }
    PDEBUG("Bytes to copy %d",bytes_to_copy);
    /**
     * realloc returns null if it fails. If we directly overwrite our previous pointer, we'll have a memory leak in case realloc fails.
     * The previous pointer is still valid and can be freed if required.
    */
    retval = mutex_lock_interruptible(&dev->lock);
    if(retval != 0){
        retval = -ERESTART;
        goto exit;
    }
    temp_realloc_ptr = krealloc(dev->temp.buffptr,(dev->temp.size + bytes_to_copy), GFP_KERNEL); 
    if(temp_realloc_ptr == NULL){
        PDEBUG("Realloc failed during write.");
        kfree(dev->temp.buffptr);
        retval = -ENOMEM;
        goto free_mutex;
    }
    dev->temp.buffptr = (char *)temp_realloc_ptr;
    offset_ptr = (dev->temp.buffptr+dev->temp.size);
    memcpy(offset_ptr,temp_buff,bytes_to_copy);
    dev->temp.size+=bytes_to_copy;
    PDEBUG("Copied bytes to temp structure");
    retval = bytes_to_copy;
    if(newline_ptr != NULL){
        struct aesd_buffer_entry *ret_ptr= aesd_circular_buffer_add_entry(&dev->dev_buff,&dev->temp);
        if(ret_ptr!=NULL){
            kfree(ret_ptr);
        }
        dev->temp.buffptr = NULL; // krealloc should malloc a new pointer next time.
        dev->temp.size = 0;
    }
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    free_mutex:
    mutex_unlock(&dev->lock);
    exit:
    kfree(temp_buff);
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    aesd_circular_buffer_init(&aesd_device.dev_buff);
    mutex_init(&aesd_device.lock); // Initialize the mutex for the circular buffer.

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    struct aesd_buffer_entry *curr;
    int i=0;
    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    
    
    AESD_CIRCULAR_BUFFER_FOREACH(curr,&aesd_device.dev_buff,i){
        if(curr->buffptr != NULL){
            kfree(curr->buffptr);
        }
    }
    unregister_chrdev_region(devno, 1);
}
module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
