/*******************************************************************************
*                                 AMetal
*                       ----------------------------
*                       innovating embedded platform
*
* Copyright (c) 2001-2018 Guangzhou ZHIYUAN Electronics Co., Ltd.
* All rights reserved.
*
* Contact information:
* web site:    http://www.zlg.cn/
*******************************************************************************/

/**
 * \file
 * \brief ������ LPS22HB �����ļ�
 *
 * \internal
 * \par Modification history
 * - 1.00 18-12-03  yrz, first implementation.
 * \endinternal
 */

#include "am_sensor_lps22hb.h"
#include "am_common.h"
#include "zlg116_pin.h"
#include "am_zlg116_inst_init.h"

/** \brief ������ LPS22HB �豸��Ϣʵ�� */
am_const am_local struct am_sensor_lps22hb_devinfo __g_lps22hb_info = {
    PIOB_0,            /*< \brief �������Ŷ���    */
    0x5C               /*< \breif LPS22HB I2C��ַ */
};

/** \breif ������ LPS22HB �豸�ṹ�嶨�� */
am_local struct am_sensor_lps22hb_dev __g_lps22hb_dev;

/** \brief ������ LPS22HB �豸ʵ���� */
am_sensor_handle_t am_sensor_lps22hb_inst_init (void)
{
    return am_sensor_lps22hb_init(&__g_lps22hb_dev,
                                  &__g_lps22hb_info,
                                  am_zlg116_i2c1_inst_init());
}

/** \brief ������ LPS22HB ʵ�����ʼ�� */
am_err_t am_sensor_lps22hb_inst_deinit (am_sensor_handle_t handle)
{
    return am_sensor_lps22hb_deinit(handle);
}

/* end of file */