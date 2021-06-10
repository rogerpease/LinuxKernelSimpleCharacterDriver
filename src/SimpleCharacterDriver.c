#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Simple reference design to show use of a character device. 
//
// Defined behavior:   
// 
//  Each minor-device can be read to and written from. I.E. 
//    /dev/charDriver1   
//    /dev/charDriver2 
//    /dev/charDriver3 
//
//  Each writer writes one message up to 255 chars which is read multiple times. 
//  Each reader gets a fresh copy per-Char driver (i.e. charDriver1 may have a different message from charDriver2).
//  If you read all characters of a message, then the next time you read it starts over. 
//  Otherwise it gives you however many you asked for up to the end. 
//
//  IE Suppose message was: Hello World. 
//   First      read 7 characters would put "Hello W" in buffer and return 7. 
//   Subsequent read 1 characters would return "o" and 1. 
//   Subsequent read 7 characters would return "rld" and 3. 
//   Next read from the same filehandle would start over. Each filehandle would read independently, even if they
//    were reading the same file. 
//
//


#define MAJOR_DEV_NUM      (228)
#define MINOR_DEV_MAX_NUM    (5)
#define MAX_MESSAGE_LEN    (256) 

//
// Contains information about all of the "devices" we will use this driver to represent. 
// 
static struct cdev my_dev;

typedef struct {
   char Message[MAX_MESSAGE_LEN]; 
   int MessageLen; 
}  MyMinorDevContext_t;

typedef struct {
   MyMinorDevContext_t Contexts[MINOR_DEV_MAX_NUM]; 
}  MyGlobalData_t; 


static MyGlobalData_t MyGlobalData; 

typedef struct { 
  int minorDevice;
  int byteStartIndex;  
} MyFHPrivateData_t; 


//
//  When the application writer does: 
//      int sysfh        = open("/dev/....");       <=== charDriverFileOpen  is called
//      int bytesRead    = read(sysfh,*buf,size);   <=== charDriverFileRead  is called
//      int bytesWritten = write(sysfh,*buf,size);  <=== charDriverFileWrite is called
//                         close(sysfh);            <=== charDriverFileClose is called
//
//
//
// Inode is a data structure about the file you opened, 
// filp is about the "filehandle" you are working on. 
// 
// struct file is different from struct FILE but everything is a file in Linux. 
// 
//


static int charDriverFileOpen(struct inode *inode, struct file *filp)
{

  MyFHPrivateData_t * myFHPrivateData;

  pr_info("charDriverFileOpen() called inode %p filp %p.\n",inode,filp);
  pr_info("charDriverFileOpen() Major %d Minor %d.\n",imajor(inode),iminor(inode));

  myFHPrivateData = (MyFHPrivateData_t *) kmalloc(sizeof(MyFHPrivateData_t),GFP_KERNEL); 
  filp->private_data = (void *) myFHPrivateData; 

  myFHPrivateData->minorDevice = iminor(inode); 
  myFHPrivateData->byteStartIndex = 0; 
  
  return 0;

}

static int charDriverFileClose(struct inode *inode, struct file *filp)
{
  pr_info("charDriverFileClose() called Maj %d Min %d filp %p.\n",imajor(inode),iminor(inode),filp);
  kfree(filp->private_data); 
  return 0;
}


static long charDriverFileIOCTL(struct file *filp, unsigned int cmd, unsigned long arg)
{
  pr_info("charDriverFileIOCTL() is called. cmd = %d, arg = %ld\n", cmd, arg);
  return 0;
}



ssize_t charDriverFileRead (struct file *filp, char * buf, size_t byteCount, loff_t * fileOffset)
{

    char c; 
    MyFHPrivateData_t * myFHPrivateData = filp->private_data; 
    //
    // Pick up where we left off. 
    //
    int startByte = myFHPrivateData->byteStartIndex; 
    int bytesRead = 0; 

    int messageLen = MyGlobalData.Contexts[myFHPrivateData->minorDevice].MessageLen;

    pr_info("Minor Device %d read request bytesread starting at %d of a %d message",
                        myFHPrivateData->minorDevice,
                        startByte,
                        messageLen); 

    while ((bytesRead < byteCount) && ((startByte + bytesRead) < messageLen))
    { 
      c = MyGlobalData.Contexts[myFHPrivateData->minorDevice].Message[startByte + bytesRead];
      pr_info("Returning %d %d (%c)",bytesRead,(int) c,c); 
      put_user(c,buf+bytesRead);
      bytesRead ++; 
    }  
    startByte += bytesRead; 
    if (startByte == messageLen)
    {  
      pr_info("Resetting read counter to 0"); 
      startByte = 0; 
    }  

    myFHPrivateData->byteStartIndex = startByte; 
    return bytesRead; 

}

// 
// Called whenever someone does a write() on a file they just opened. 
// 
//
ssize_t charDriverFileWrite (struct file *filp, const char * buf, size_t byteCount, loff_t * byteOffset)
{

    MyFHPrivateData_t * myFHPrivateData;
    char c; 
    ssize_t bytesWritten = 0; 
    
    myFHPrivateData = filp->private_data;

    while ((bytesWritten < byteCount) && (bytesWritten < MAX_MESSAGE_LEN))
    { 
      get_user(c,buf+bytesWritten); 
      MyGlobalData.Contexts[myFHPrivateData->minorDevice].Message[bytesWritten] = c;  
      bytesWritten ++; 
    }  
    MyGlobalData.Contexts[myFHPrivateData->minorDevice].MessageLen = bytesWritten;
    return bytesWritten; 

}

// Declare File operations structure. 
static const struct file_operations my_dev_fops = {
	.owner          = THIS_MODULE,
        .read           = charDriverFileRead,  
        .write          = charDriverFileWrite,  
	.open           = charDriverFileOpen,
	.release        = charDriverFileClose,
	.unlocked_ioctl = charDriverFileIOCTL,

};

static int __init charDriverInit(void)
{

   int minorDevice; 
   int j;
   int ret;

   dev_t dev;
  
   dev = MKDEV(MAJOR_DEV_NUM, 0);
   pr_info("Char Driver initialized for Major Device %d\n",MAJOR_DEV_NUM);

   // Allocate Device Numbers 
   ret = register_chrdev_region(dev, 1, "simpleCharacterDriver");
   if (ret < 0) {
     pr_info("Unable to allocate Major Device %d\n", MAJOR_DEV_NUM);
     return ret;
   }

   //
   // Initialize the cdev structure and add it to the kernel space 
   //

   cdev_init(&my_dev, &my_dev_fops);
   for (minorDevice = 0; minorDevice < MINOR_DEV_MAX_NUM; minorDevice++)
   { 
      pr_info("  Minor Driver %d\n",minorDevice);
      dev = MKDEV(MAJOR_DEV_NUM, minorDevice);
      ret = cdev_add(&my_dev, dev, 1);
      if (ret < 0) 
      {
        unregister_chrdev_region(dev, 1);
        pr_info("Unable to add cdev\n");
        return ret;
      }
      for (j = 0; j < MAX_MESSAGE_LEN; j++)
      {
        MyGlobalData.Contexts[minorDevice].Message[j] = (char) 0;    
        MyGlobalData.Contexts[minorDevice].MessageLen = 0;    
      }
  }


  return 0;
}

static void __exit charDriverExit(void)
{
   pr_info("Roger's Char Driver exit\n");
   cdev_del(&my_dev);
   unregister_chrdev_region(MKDEV(MAJOR_DEV_NUM, 0), 1);
}

module_init(charDriverInit);
module_exit(charDriverExit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Roger Pease (adapted from De Los Rios textook)");
MODULE_DESCRIPTION("This is a simple character driver which takes in an up-to-255 byte message and outputs it to readers upon request.");
