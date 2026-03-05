/*-----------------------------------------------------------------------------*/
// File Name : zyt_info.c
// for kernel debug info
// Author : wangming
// Date : 2015-1-18
/*-----------------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/device.h>
#include <asm/uaccess.h>

#include <linux/stat.h>
#include <linux/of.h>
#include <soc/sprd/board.h>

#define BUFFER_LEN 1024
static char zyt_info_buffer[BUFFER_LEN]={0};
static struct sensor_name_tag *s_zyt_info_sensor_name;
static char *zyt_peripherals_LCD_name;
static char *s_zyt_battery_info;


struct sensor_name_tag{
	    char main_sensor[20];
	   char main2_sensor[20];
	   char sub_sensor[20];
	   char sub2_sensor[20];
	   char main3_sensor[20];
};


static bool cali_mode;

bool zyt_check_cali_mode(void)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	//printk("lhx zyt_check_cali_mode ret = %d, cmd_line = %s\n",ret, cmd_line);
	if (ret)
		return ret;

	if (strstr(cmd_line, "sprdboot.mode=cali"))
		cali_mode = true;
	else if (strstr(cmd_line, "sprdboot.mode=autotest"))
		cali_mode = true;
	else
		cali_mode =  false;
	
	//printk("lhx zyt_check_cali_mode cali_mode = %d\n",cali_mode);
	return cali_mode;
}
EXPORT_SYMBOL_GPL(zyt_check_cali_mode);



void zyt_info(char* content)
{
	unsigned int content_len = strlen(content);
	unsigned int buffer_used = strlen(zyt_info_buffer);
	if(content == NULL) return;
	if((buffer_used + content_len) > BUFFER_LEN) return;
	strcat(zyt_info_buffer,content);
}

void zyt_info_s(char* c1){zyt_info(c1);zyt_info("\n");}
void zyt_info_s2(char* c1,char* c2){zyt_info(c1);zyt_info(c2);zyt_info("\n");}
void zyt_info_sx(char* c1,int x){char temp[50];sprintf(temp,"%s 0x%x",c1,x);zyt_info(temp);zyt_info("\n");}

void zyt_get_lcm_info(void *name)
{
	//printk("zyt_get_lcm_info name=%s\n", name);
	if(name == NULL) 
		return;
	zyt_peripherals_LCD_name = (char*)name;
	//printk("zyt_get_lcm_info zyt_peripherals_LCD_name=%s\n", zyt_peripherals_LCD_name);
}

void zyt_get_sensor_info(void *name)
{
	s_zyt_info_sensor_name = (struct sensor_name_tag*)name;
	//printk("zyt_get_sensor_info %s\n",s_zyt_info_sensor_name->main_sensor);

}

void zyt_get_battery_info(void *info)
{
	//printk("zyt_get_battery_info info=%s\n", info);
	if(info == NULL) 
		return;
	s_zyt_battery_info = (char*)info;
	//printk("zyt_get_battery_info s_zyt_battery_info=%s\n", s_zyt_battery_info);
}

EXPORT_SYMBOL_GPL(zyt_info_s);
EXPORT_SYMBOL_GPL(zyt_info_s2);
EXPORT_SYMBOL_GPL(zyt_info_sx);
EXPORT_SYMBOL_GPL(zyt_get_lcm_info);
EXPORT_SYMBOL_GPL(zyt_get_sensor_info);
EXPORT_SYMBOL_GPL(zyt_get_battery_info);

//extern char* get_sensor_info(int sensor_id);/*sensor_drv_k.c*/
static int zyt_info_proc_show(struct seq_file *m, void *v) {
	seq_printf(m, "[LCD] : %s\n[MainSensor] : %s\n[Main2Sensor] : %s\n[SubSensor] : %s\n[Sub2Sensor] : %s\n[Main3Sensor] : %s\n%s%s\n",
					zyt_peripherals_LCD_name,
					s_zyt_info_sensor_name->main_sensor,
					s_zyt_info_sensor_name->main2_sensor,
					s_zyt_info_sensor_name->sub_sensor,
					s_zyt_info_sensor_name->sub2_sensor,
					s_zyt_info_sensor_name->main3_sensor,
					zyt_info_buffer,
					s_zyt_battery_info
					);
	return 0;
}

static int zyt_info_proc_open(struct inode *inode, struct file *file) {
 return single_open(file, zyt_info_proc_show, NULL);
}

static const struct proc_ops zyt_info_proc_fops = {
 //.proc_owner = THIS_MODULE,
 .proc_open = zyt_info_proc_open,
 .proc_read = seq_read,
 .proc_lseek = seq_lseek,
 .proc_release = single_release,
};


int zyt_adb_debug=0;  
module_param(zyt_adb_debug,int,S_IRUGO|S_IWUSR);


static int __init zyt_info_proc_init(void) {
 proc_create("zyt_info", 0, NULL, &zyt_info_proc_fops);
 return 0;
}

static void __exit zyt_info_proc_exit(void) {
 remove_proc_entry("zyt_info", NULL);
}

MODULE_LICENSE("GPL");
module_init(zyt_info_proc_init);
module_exit(zyt_info_proc_exit);
