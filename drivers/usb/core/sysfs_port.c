/*
 * Copyright (C) 2016, Socionext Inc.
 *                       All Rights Reserved.
 *
 */

#if defined(CONFIG_USB_UNIPHIER_WA_PORT_CONTROL)

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/usb.h>
#include <linux/usb/quirks.h>
#include "usb.h"

static ssize_t show_port_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_device	*udev;
	ssize_t				sz;
	int					i, ret, portNum;
	u32					status;

	sz = 0;
	udev = to_usb_device(dev);

	ret = usb_get_hub_status( udev, &status );
/*_ 	printk( "%s:%d ret=%d\n", __FUNCTION__, __LINE__, ret ); _*/
	if( ret < 0 )
		return -EINVAL;
	ret = sprintf( &buf[sz], "%u %08X\n", 0, status );
	if( ret < 0 )
		return -EINVAL;
	sz += ret;

	portNum = udev->maxchild;
	for( i=1 ; i <= portNum ; i++ ){
		if( usb_get_port_status( udev, i, &status ) < 0 )
			return -EINVAL;

		ret = sprintf( &buf[sz], "%u %08X\n", i, status );
		if( ret < 0 )
			return -EINVAL;
		sz += ret;
	}

	return sz;
}
static DEVICE_ATTR(port_status, S_IRUGO, show_port_status, NULL);

static ssize_t set_port_ctrl(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_device* udev;
	u32 flag, status, port;
	int ret;

	udev = to_usb_device(dev);

	ret = sscanf( buf, "%x %x %x \n", &flag, &status, &port );
	if( (ret != 3) || (port == 0) )
		return -EINVAL;

	if( flag ){
		ret = usb_set_port_status( udev, port, status );
	}else{
		ret = usb_clr_port_status( udev, port, status );
	}
	if( ret < 0 )
		return -EINVAL;

	return count;
}
static DEVICE_ATTR(port_ctrl, S_IWUSR, NULL, set_port_ctrl);

static struct attribute *dev_port_attrs[] = {
	&dev_attr_port_status.attr,
	&dev_attr_port_ctrl.attr,
	NULL
};

static umode_t dev_port_attrs_are_visible(struct kobject *kobj, struct attribute *a, int n)
{
/*_ 	struct device *dev = container_of(kobj, struct device, kobj); _*/
/*_ 	struct usb_device *udev = to_usb_device(dev); _*/

	if( a == &dev_attr_port_status.attr || a == &dev_attr_port_ctrl.attr ){
		/*_ return 0; _*/
	}
	return a->mode;
}

const struct attribute_group dev_port_attr_grp = {
	.attrs =		dev_port_attrs,
	.is_visible =	dev_port_attrs_are_visible,
};

#endif /* CONFIG_USB_UNIPHIER_WA_PORT_CONTROL */
