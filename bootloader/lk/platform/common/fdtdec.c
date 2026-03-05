#include <libfdt.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sprd_common.h>
#include <libfdt_env.h>
#include <fdtdec.h>
#include <assert.h>
#include <config.h>

/*Memory address where DTB is located
 * if enable ld file dtb_start , need del this located variable.*/
int  __dtb_start;
int  __dtb_end;

int fdt_getprop_u32(const void *fdt, int off, const char *prop, u32 *dflt)
{
	const void *val;
	int len;

	if (!fdt || !prop) {
		pr_err("fdt or prop is NULL\n");
		return -1;
	}

	val = fdt_getprop(fdt, off, prop, &len);
	if (val) {
		*dflt = fdt32_to_cpu(*((const fdt32_t*)val));
		return 0;
	}

	return -1;
}

int fdtdec_lookup_phandle(const void *blob, int node, const char *prop_name)
{
	const u32 *phandle;
	int lookup;

	//debug("%s: %s\n", __func__, prop_name);
	phandle = fdt_getprop(blob, node, prop_name, NULL);
	if (!phandle)
		return -FDT_ERR_NOTFOUND;

	lookup = fdt_node_offset_by_phandle(blob, fdt32_to_cpu(*phandle));
	return lookup;
}

int fdtdec_decode_region(const void *blob, int node, const char *prop_name,
	 fdt_addr_t *basep, fdt_size_t *sizep)
{
	const fdt_addr_t *cell;
	int len;
	int size = sizeof(fdt_addr_t);
	ulong temp[2] = {0};
	unsigned int i=0;

	dprintf(INFO,"%s: %s: %s\n", __func__, fdt_get_name(blob, node, NULL),prop_name);

	cell = fdt_getprop(blob, node, prop_name, &len);
	if (!cell || (len < size * 2)) {
		dprintf(INFO,"cell=%p, len=%d\n", cell, len);
		return -1;
	}else{
		dprintf(INFO,"cell=%p, len=%d\n", cell, len);
	}
	/*"cell" are not aligned in 8 bytes, this will cause "Synchronous Abort",
	so we use a temporary array to force align*/

	//memcpy is not available here.
	//memcpy((void *)temp, (void *)cell, 2 * sizeof(ulong));
	char *dst = (char *)temp;
	char *src = (char *)cell;

	for (i=0; i< 2*sizeof(ulong); i++){
		dst[i] = src[i];
	}

	*basep = fdt_addr_to_cpu(temp[0]);
	*sizep = fdt_size_to_cpu(temp[1]);
	dprintf(INFO,"%s: base=%08lx, size=%lx\n", __func__, (ulong)*basep,(ulong)*sizep);

	return 0;
}

int fdtdec_prepare_fdt(void)
{

    if (fdt_check_header((uintptr_t *)&__dtb_start)) {
		errorf("**%s: No valid device tree binary found in the address=%8d.\n", __func__,__dtb_start);
        return -1;
    }
    return 0;
}

int fdtdec_check_fdt(void)
{
	ASSERT(!fdtdec_prepare_fdt());

	return 0;
}

int get_buffer_base_size_from_dt(const char *name, unsigned long *basep, unsigned long *sizep)
{
        int phandle = 0;
        const void *fdt_blob = &__dtb_start;
        int nodeoffset = fdt_path_offset(fdt_blob, "/ion");

        dprintf(INFO,"ion node offset = %d\n", nodeoffset);
        if (nodeoffset == -FDT_ERR_NOTFOUND)
                return -1;

        nodeoffset = fdt_subnode_offset(fdt_blob, nodeoffset, name);
        dprintf(INFO,"%s node offset = %d\n", name, nodeoffset);
        if (nodeoffset == -FDT_ERR_NOTFOUND)
                return -1;

        phandle = fdtdec_lookup_phandle(fdt_blob, nodeoffset, "memory-region");
        dprintf(INFO,"memory-region phandle = %d\n", phandle);
        if (phandle == -FDT_ERR_NOTFOUND)
                return -1;

        if (fdtdec_decode_region(fdt_blob, phandle, "reg", basep, sizep)) {
                errorf("Failed to decode reg property \n");
                return -EINVAL;
        }

        return 0;
}

int fdtdec_get_int(const void *blob, int node, const char *prop_name,
		int default_val)
{
	const int *cell;
	int len;

	//debug("%s: %s: ", __func__, prop_name);
	cell = fdt_getprop(blob, node, prop_name, &len);
	if (cell && len >= sizeof(int)) {
		int val = fdt32_to_cpu(cell[0]);

		//debug("%#x (%d)\n", val, val);
		return val;
	}
	debug("(not found)\n");
	return default_val;
}

int fdtdec_get_child_count(const void *blob, int node)
{
	int subnode;
	int num = 0;

	fdt_for_each_subnode(subnode, blob, node)
		num++;

	return num;
}

