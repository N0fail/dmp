/*
 * Copyright (C) 2020 Kudryavtsev Konstantin <suuupport2@gmail.com>
 *
 * This file is released under the GPL.
 */

#include <linux/device-mapper.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/bio.h>

#define DM_MSG_PREFIX "zero"

struct stat_target
{
	struct dm_dev* dev;
	__UINT64_TYPE__ read_sum;
	__UINT64_TYPE__ read_calls;
	__UINT64_TYPE__ write_sum;
	__UINT64_TYPE__ write_calls;
};

/*
 * Constructor
 */
static int stat_target_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	struct stat_target *st;

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
	
	ti->private = st;

	printk(KERN_CRIT "\n>>out function stat_target_ctr \n");
	return 0;
}

// Destructor
static void stat_target_dtr(struct dm_target *ti)
{
  struct stat_target *mdt = (struct stat_target *) ti->private;
  printk(KERN_CRIT "\n<<in function basic_target_dtr \n");        
  dm_put_device(ti, mdt->dev);
  kfree(mdt);
  printk(KERN_CRIT "\n>>out function basic_target_dtr \n");               
}


/*
 * Return zeros only on reads
 */
static int stat_map(struct dm_target *ti, struct bio *bio)
{
	struct stat_target* st = (struct stat_target*) ti->private;
	printk(KERN_CRIT "\n<<in function basic_target_map \n");

	switch (bio_op(bio)) {
	case REQ_OP_READ:
		if (bio->bi_opf & REQ_RAHEAD) {
			/* readahead of null bytes only wastes buffer cache */
			return DM_MAPIO_KILL;
		}
		// count read stats
		st->read_sum += bio->bi_io_vec->bv_len;
		++st->read_calls;
		printk(KERN_CRIT "\n read stats = %ld avg  %ld calls\n", st->read_sum/st->read_calls, st->read_calls);
		zero_fill_bio(bio);

		break;
	case REQ_OP_WRITE:
		// count write stats
		st->write_sum += bio->bi_io_vec->bv_len;
		++st->write_calls;
		printk(KERN_CRIT "\n read stats = %ld avg  %ld calls\n", st->write_sum/st->write_calls, st->write_calls);
		break;
	default:
		return DM_MAPIO_KILL;
	}

	bio_endio(bio);

	/* accepted bio, don't make new request */
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
		DMERR("register failed %d", r);

	return r;
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
