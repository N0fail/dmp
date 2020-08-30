/*
 * Copyright (C) 2020 Kudryavtsev Konstantin <suuupport2@gmail.com>
 *
 * This file is released under the GPL.
 */

#include <linux/device-mapper.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/bio.h>
#include <linux/moduleparam.h> 
#include <linux/string.h> 
#include <linux/sysfs.h>
#define DM_MSG_PREFIX "zero"

__UINT64_TYPE__ read_sum = 0;
__UINT64_TYPE__ read_calls = 0;
__UINT64_TYPE__ write_sum = 0;
__UINT64_TYPE__ write_calls = 0;

static struct kobject* my_kobj;
int devices_count = 0;


ssize_t my_dev_show(struct device* dev, struct device_attribute* attr, char* buff) // print stats in file
{
	__UINT64_TYPE__ avg_read = 0;
	__UINT64_TYPE__ avg_write = 0;
	__UINT64_TYPE__ avg_all = 0;

	if (read_calls > 0)
	{
		avg_read = read_sum/read_calls;
		avg_all = (read_sum+write_sum)/(read_calls+write_calls);
	}

	if (write_calls > 0)
		avg_write = write_sum / write_calls;

	return sprintf(buff, "read:\n\treqs: %ld\n\tavg size: %ld\nwrite:\n\treqs: %ld\n\tavg size: %ld\ntotal:\n\treqs: %ld\n\tavg size: %ld\n", 
					read_calls, avg_read, write_calls, avg_write, read_calls+write_calls, avg_all);
}

ssize_t my_dev_store(struct device* dev, struct device_attribute* attr, const char* buff, size_t count)
{
	return 0;
}

static struct device_attribute my_dev_attribute =
{
	.show = my_dev_show,
	.store = my_dev_store
};

static struct stat_target
{
	struct dm_dev* dev;
};

static struct target_type stat_target;

/*
 * Constructor
 */
static int stat_target_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	struct stat_target *st;
	int create_file_err = 0;

	if (argc != 1) {
		ti->error = "One argument required";
		return -EINVAL;
	}

	st = kmalloc(sizeof(struct stat_target), GFP_KERNEL);

	if(st==NULL)
	{
		printk(KERN_CRIT "\n Mdt is null\n");
		ti->error = "dm-stat_target: Cannot allocate linear context";
		return -ENOMEM;
	}       

	if (dm_get_device(ti, argv[0], dm_table_get_mode(ti->table), &st->dev)) {
            ti->error = "dm-stat_target: Device lookup failed";
            kfree(st);
        	printk(KERN_CRIT "\n>>out function stat_target_ctr with errorrrrrrrrrr \n");           
        	return -EINVAL;
        }
	
	if (devices_count++ == 0) // if first device attached, create stat folder
	{
		my_kobj = kobject_create_and_add("stat", &(stat_target.module->mkobj.kobj));
		if(!my_kobj)
		{
			DMERR("failed to create an object");
			return -ENOMEM;
		}

		my_dev_attribute.attr.name = "volumes";
		my_dev_attribute.attr.mode = 0660;

		create_file_err = sysfs_create_file(my_kobj, &(my_dev_attribute.attr));
		if (create_file_err < 0)
		{
			kobject_put(my_kobj);
			printk(KERN_CRIT "\nerror creating sysfs file\n");           
			return -EINVAL;
		}
	}

	ti->private = st;
	return 0;
}

// Destructor
static void stat_target_dtr(struct dm_target *ti)
{
	struct stat_target *mdt = (struct stat_target *) ti->private;
	dm_put_device(ti, mdt->dev);
	kfree(mdt);
	if (--devices_count == 0) // if last device deleted, delete stat folder
		kobject_put(my_kobj);
}


static int stat_map(struct dm_target *ti, struct bio *bio)
{
	struct stat_target* st = (struct stat_target*) ti->private;

	switch (bio_op(bio)) {
	case REQ_OP_READ:
		if (bio->bi_opf & REQ_RAHEAD) {
			/* readahead of null bytes only wastes buffer cache */
			return DM_MAPIO_KILL;
		}
		// count read stats
		read_sum += bio->bi_iter.bi_size;
		++read_calls;
		break;
	case REQ_OP_WRITE:
		// count write stats
		write_sum += bio->bi_iter.bi_size;
		++write_calls;
		break;
	default:
		return DM_MAPIO_KILL;
	}

	submit_bio(bio);
	bio_endio(bio);

	return DM_MAPIO_SUBMITTED;
}

static struct target_type stat_target = {
	.name   = "dmp",
	.version = {1, 0, 0},
	.module = THIS_MODULE,
	.ctr    = stat_target_ctr,
	.dtr    = stat_target_dtr,
	.map    = stat_map,
};

static int __init dm_zero_init(void)
{
	int r = dm_register_target(&stat_target);
	if (r < 0)
	{
		DMERR("register failed %d", r);
		return r;
	}

	return 0;
}

static void __exit dm_zero_exit(void)
{
	dm_unregister_target(&stat_target);
}

module_init(dm_zero_init)
module_exit(dm_zero_exit)

MODULE_AUTHOR("Konstantin Kudryavtsev <suuupport2@gmail.com>");
MODULE_DESCRIPTION(DM_NAME " target saving i/o stats");
MODULE_LICENSE("GPL");
