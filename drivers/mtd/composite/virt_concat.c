/*
 * Virtual concat MTD device driver
 *
 * Copyright (C) 2018 Bernhard Frauendienst
 * Author: Bernhard Frauendienst, kernel@nospam.obeliks.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/mtd/concat.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

struct of_virt_concat {
	struct mtd_info	 *cmtd;
	int num_devices;
	struct mtd_info	 **devices;
};

static int virt_concat_remove(struct platform_device *pdev)
{
	struct of_virt_concat *info;
	int i;

	info = platform_get_drvdata(pdev);
	if (!info)
		return 0;
	platform_set_drvdata(pdev, NULL);

	if (info->cmtd) {
		mtd_device_unregister(info->cmtd);
		mtd_concat_destroy(info->cmtd);
	}

	if (info->devices) {
		for (i = 0; i < info->num_devices; i++)
			put_mtd_device(info->devices[i]);

		kfree(info->devices);
	}

	return 0;
}

static int virt_concat_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct of_phandle_iterator it;
	int err, count;
	struct of_virt_concat *info;
	struct mtd_info *mtd;

	count = of_count_phandle_with_args(node, "devices", NULL);
	if (count <= 0)
		return -EINVAL;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	err = -ENOMEM;
	info->devices = kcalloc(count, sizeof(*(info->devices)), GFP_KERNEL);
	if (!info->devices)
		goto err_remove;

	platform_set_drvdata(pdev, info);

	of_for_each_phandle(&it, err, node, "devices", NULL, 0) {
		mtd = get_mtd_device_by_node(it.node);
		if (IS_ERR(mtd)) {
			of_node_put(it.node);
			err = -EPROBE_DEFER;
			goto err_remove;
		}

		info->devices[info->num_devices++] = mtd;
	}

	err = -ENXIO;
	info->cmtd = mtd_concat_create(info->devices, info->num_devices,
		dev_name(&pdev->dev));
	if (!info->cmtd)
		goto err_remove;

	info->cmtd->dev.parent = &pdev->dev;
	mtd_set_of_node(info->cmtd, node);
	mtd_device_register(info->cmtd, NULL, 0);

	return 0;

err_remove:
	virt_concat_remove(pdev);

	return err;
}

static const struct of_device_id virt_concat_of_match[] = {
	{ .compatible = "mtd-concat", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, virt_concat_of_match);

static struct platform_driver virt_concat_driver = {
	.probe = virt_concat_probe,
	.remove = virt_concat_remove,
	.driver	 = {
		.name   = "virt-mtdconcat",
		.of_match_table = virt_concat_of_match,
	},
};

module_platform_driver(virt_concat_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bernhard Frauendienst <kernel@nospam.obeliks.de>");
MODULE_DESCRIPTION("Virtual concat MTD device driver");
