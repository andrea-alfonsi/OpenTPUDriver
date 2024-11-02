#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#define PROJECT_NAME opentpu
#define MODULE_NAME "opentpu"
#define DEVICE_NAME "opentpu"
#define CLASS_NAME  "opentpu"

MODULE_AUTHOR("Andrea Alfonsi");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("A module used to comunicate or simulate the OpenTPU");
MODULE_VERSION("0.1");

//  Define the name parameter.
static char *emulator = "opentpu-emulator-latest";
module_param(emulator, charp, S_IWUSR|S_IRUGO);
MODULE_PARM_DESC(emulator, "The name of the opentpu emulator executableto use for simulations");

//  The device number, automatically set. The message buffer and current message
//  size. The number of device opens and the device class struct pointers.
static int    majorNumber;
static char   message[256] = {0};
static short  messageSize;
static struct class*  openTPUClass  = NULL;
static struct device* openTPUDevice = NULL;
static DEFINE_MUTEX(ioMutex);

static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

static int __init mod_init(void)
{
    pr_info("%s: module loaded at 0x%p\n", MODULE_NAME, mod_init);
    pr_info("%s: module loaded with emulator: '%s'\n", MODULE_NAME, emulator);

    //  Create a mutex to guard io operations.
    mutex_init(&ioMutex);

    //  Register the device, allocating a major number.
    majorNumber = register_chrdev(0 /* i.e. allocate a major number for me */, DEVICE_NAME, &fops);
    if (majorNumber < 0) {
        pr_alert("%s: failed to register a major number\n", MODULE_NAME);
        return majorNumber;
    }
    pr_info("%s: registered correctly with major number %d\n", MODULE_NAME, majorNumber);

    //  Create the device class.
    openTPUClass = class_create(/*THIS_MODULE, */ CLASS_NAME);
    if (IS_ERR(openTPUClass)) {
        //  Cleanup resources and fail.
        unregister_chrdev(majorNumber, DEVICE_NAME);
        pr_alert("%s: failed to register device class\n", MODULE_NAME);

        //  Get the error code from the pointer.
        return PTR_ERR(openTPUClass);
    }
    pr_info("%s: device class registered correctly\n", MODULE_NAME);

    //  Create the device.
    openTPUDevice = device_create(openTPUClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(openTPUDevice)) {
        class_destroy(openTPUClass);
        unregister_chrdev(majorNumber, DEVICE_NAME);
        pr_alert("%s: failed to create the device\n", DEVICE_NAME);
        return PTR_ERR(openTPUDevice);
    }
    pr_info("%s: device class created correctly\n", DEVICE_NAME);

    //  Success!
    return 0;
}

static void __exit mod_exit(void)
{
    pr_info("%s: unloading...\n", MODULE_NAME);
    device_destroy(openTPUClass, MKDEV(majorNumber, 0));
    class_unregister(openTPUClass);
    class_destroy(openTPUClass);
    unregister_chrdev(majorNumber, DEVICE_NAME);
    mutex_destroy(&ioMutex);
    pr_info("%s: device unregistered\n", MODULE_NAME);
}

/** @brief The device open function that is called each time the device is opened.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep){
    //  Try and lock the mutex.
    if(!mutex_trylock(&ioMutex)) {
        pr_alert("%s: device in use by another process", MODULE_NAME);
        return -EBUSY;
    }

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
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
   int error_count = 0;

   // copy_to_user has the format ( *to, *from, size) and returns 0 on success
   error_count = copy_to_user(buffer, message, messageSize);

   if (error_count == 0) {
      pr_info("%s: sent %d characters to the user\n", MODULE_NAME, messageSize);
      // Clear the position, return 0.
      messageSize = 0;
      return 0;
   }
   else {
      pr_err("%s: failed to send %d characters to the user\n", MODULE_NAME, error_count);
      // Failed -- return a bad address message (i.e. -14)
      return -EFAULT;              
   }
}

/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the sprintf() function along with the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
    //  Write the string into our message memory. Record the length.
    long copied_from_user;
    copied_from_user = copy_from_user(message, buffer, len);
    messageSize = len;
    if(copied_from_user != 0 )
    {
        printk(KERN_WARNING "copy_from_user failed ret_val: %ld\n",copied_from_user );
    }
    else
    {
        pr_info("%s: copy_from_user ret_val : %ld\n", MODULE_NAME, copied_from_user);
        pr_info("%s: received %zu characters from the user\n", MODULE_NAME, len);
    }

    return len;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep){
     mutex_unlock(&ioMutex);
     pr_info("%s: device successfully closed\n", MODULE_NAME);
     return 0;
}
    

module_init(mod_init);
module_exit(mod_exit);
