# Brief sysfs Tutorial
[From this webpage: sysfs_Tutorial](https://www.cs.swarthmore.edu/~newhall/sysfstutorial.pdf)
## Get a Handle to sysfs.
获取handler
```c
struct device *my_dev;
my_dev = root_device_register("my_folder");
```
此方法将创建`/sys/devices/my_folder/`，并在`my_dev`内存放相应的指针；失败的话会返回NULL.   
在注销模块时，调用`void root_device_unregister(struct device *my_dev)`来注销此设备。
***
## Using a `struct device`
+   Not that really useful   
+   Most useful field: `kobject` struct:   
    ```c
    struct kobject *ko = my_dev->kobj;
    ```
+   Each folder in sysfs has an associated `kobject` struct.   
    +   You'll need the `struct kobject *` to add subdiectories, add files, etc.
***
## Creating subdirectories
+   Making a new folder:   
    ``` c
    struct kobject * subdir =
        kobject_create_and_add(
            char *name, struct kobject *parent
        );
    ```
    It returns `NULL` on error.
    这个函数也可以用来直接创建sysfs节点，例如`kobject_create_and_add("new_k_node",  kernel_kobj)`会直接创建`/sys/kernel/new_k_node/`。
+   Free it when done:
    ``` c
    void kobject_put(struct kobject * subdir);
    ```
***
## Bring on the Structs
+   Each file is associated with a set of structs.
    ``` c
    // From linux/sysfs.h, kernel version: 4.19 rc1.
    struct attribute {
	    const char		        *name;
	    umode_t			mode;
    #ifdef CONFIG_DEBUG_LOCK_ALLOC
	    bool			ignore_lockdep:1;
	    struct lock_class_key	*key;
	    struct lock_class_key	skey;
    #endif
    };
    ```
+   Initialize the name and mode fields:
    ``` c
    struct attribute attr;
    attr.name = "attr1";
    attr.mode = 0666; //rw-rw-rw-
    ```
***
## Layer II: device_attribute
The `struct attribute` is references the following struct:
``` c
//From linux/device.h, kernel version: 4.19 rc1

/* interface for exporting device attributes */
struct device_attribute {
	struct attribute	attr;
	ssize_t (*show)(struct device *dev, struct device_attribute *attr,
			char *buf);
	ssize_t (*store)(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count);
};
```
***
## That's Weird Syntax...
+   It's C's way tof saying "Fuction Pointer"
    -   The `show` function is called when your sysfs file is read.
        - Place the data you want to pass back into the buffer.
        - Return the number of bytes placed into the buffer.
        - Buffer is PAGE_SIZE bytes. (On most x86 platform it is 4KB).
        - `show()` methods should return the number of bytes printed into the buffer. This is the return value of `scnprintf()`. 
        - `show()` must not use `snprintf()` when formatting the value to be returned to user space. If you can guarantee that an overflow will never happen you can use `sprintf()` otherwise you must use `scnprintf()`.
***
## That's Weird Syntax... Part 2
+   The `store` function is called when your sysfs file is written to.
    -   The third argument is a buffer containing the data.
    -   The fourth argument is the number of bytes of data in the buffer.
    -   Return the number of bytes consumed -normally, just return the 4th argument to say you processed al the data.
***
## Back to Creating files
+   If your file will be read, create a function:   
    ``` c
    ssize_t mydev_do_read
        (   struct device *dev,
            struct device_attribute *attr,
            char *buf);
+   If your file will be written to, create a function:
    ``` c
    ssize_t mydev_do_write
        (   struct device *dev,
            struct device_attribute *attr,
            const char *buf,
            size_t count);
    ```
***
## Creating Files
+   Then, create and initialize `device_attribute` struct. Must be either a global variable (ideally, static), or dynamically allocated.
    ``` c
    // Global declaration
    static struct device_attribute my_dev_attr;

    // Later on in the file
    my_dev_attr.attr.name = "my_file";
    my_dev_attr.attr.mode = 0444;   // Read Only
    my_dev_attr.show = mydev_do_read;   // Read function
    my_dev_attr.store = NULL;   // No Write function needed, because this is read only.
    ```
***
## Adding 1 File
+   Once we have the device_attribute structure, we can add the file to the folder.
    ``` c
    int sysfs_create_file(
        struct kobject *folder,
        const struct attribute *attr);
    ```
+   `kobject *` and `attribute *` are passed as first two arguments to read and write functions.
    -   A handy way to identify which folder was involved if you have many folders woth the same files in each one.
    -   Argument is `struct device *`, but the pointers are the same.   __(原文如此，理解不了)__ 
***
## Continuing the Examples
``` c
static struct device_attribute my_dev_attr;
struct kobject *s1;
if(PTR_ERR(sysfs_create_file(s1, &my_dev_attr.attr))) {
    // Handle error
}
```
***
## Cleaning Up: Removing One File
``` c
void sysfs_remove_file (
    struct kobject * folder;
    const struct attribute *attr);
```

Again continuing the examples:
``` c
sysfs_remove_file(s1, &my_dev_attr.attr);
```
***
## `struct attribute_group`
+   An attribute group stores the null terminated array of attribute pointers.
``` c
struct attribute_group {
    const char *name;
    umode_t (*is_visible)
        (struct kobject *, struct attribute *, int);
    struct attribute **attrs;
    struct bin_attribute    **bin_attrs;
}
```
***
## Registering a Group of Attributes
``` c
int sysfs_create_group (
    struct kobject * folder;
    const struct attribute_group *group;)
```
Returns 0 on success; non-zero in failure.   
Continuing the example: THis code creates a file for each atribute in the array, `bundle`, pointed to from `my_attr_grp`, inside the folde corresponding to `s1`.
``` c
struct attribute_group my_attr_grp;
struct kobject *s1;
if(sysfs_create_group(s1, &my_attr_grp)) {
    // Handle Error.
}
```
***
## Cleaning up a group
``` c
void sysfs_remove_group(
    struct kobject *folder,
    struct attribute_group *grp);
```
``` c
struct attribute_group my_attr_group;
struct kobject *s1;
sysfs_remove_group(s1, &my_attr_group);