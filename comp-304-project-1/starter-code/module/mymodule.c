#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>

#define PROC_NAME "psvis"
#define BUFFER_SIZE 128

static char pid_buffer[BUFFER_SIZE];
static struct proc_dir_entry *entry;

static void print_process_tree(struct seq_file *m, struct task_struct *task) {
    struct task_struct *child;
    struct list_head *list;

    list_for_each(list, &task->children) {
        child = list_entry(list, struct task_struct, sibling);
        seq_printf(m, "\"%d %s\" -> \"%d %s\";\n",
                   task->pid, task->comm, child->pid, child->comm);
        print_process_tree(m, child); // recursive print call to traverse through tree
    }
}

static int psvis_show(struct seq_file *m, void *v) {
    int target_pid;
    struct task_struct *task;

    
    if (kstrtoint(pid_buffer, 10, &target_pid) != 0) {
        seq_puts(m, "Invalid PID\n"); // convert target PID from buffer to integer
        return 0;
    }

    
    task = pid_task(find_get_pid(target_pid), PIDTYPE_PID); // find the task corresponding to the target PID
    if (!task) {
        seq_printf(m, "Process with PID %d not found.\n", target_pid);
        return 0;
    }

    
    print_process_tree(m, task);
    return 0;
}


static int psvis_open(struct inode *inode, struct file *file) {
    return single_open(file, psvis_show, NULL);
}

static ssize_t psvis_write(struct file *file, const char __user *buf, size_t count, loff_t *pos) {
    if (count > BUFFER_SIZE - 1) {
        return -EINVAL;
    }

    if (copy_from_user(pid_buffer, buf, count)) {
        return -EFAULT;
    }

    pid_buffer[count] = '\0';
    return count;
}

static const struct proc_ops psvis_fops = {
    .proc_open = psvis_open,
    .proc_read = seq_read,
    .proc_write = psvis_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init psvis_init(void) {
    entry = proc_create_data(PROC_NAME, 0666, NULL, &psvis_fops, NULL);
    if (!entry) {
        return -ENOMEM;
    }
    printk(KERN_INFO "psvis module loaded.\n");
    return 0;
}

static void __exit psvis_exit(void) {
    proc_remove(entry);
    printk(KERN_INFO "psvis module unloaded.\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ahmet Koca - Yusuf Çelik");
MODULE_DESCRIPTION("Psvis module");

module_init(psvis_init);
module_exit(psvis_exit);
