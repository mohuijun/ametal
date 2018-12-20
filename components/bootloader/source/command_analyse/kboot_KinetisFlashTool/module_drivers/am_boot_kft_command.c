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
 * \brief bootloader kboot KinetisFlashTool �������ģ��ʵ��
 *
 * ��Ҫ��ͨ��״̬������λ��KinetisFlashTool���͹����ĵ�packet��packet�������ͨ������ִ����Ӧ��handle����
 *
 * \internal
 * \par Modification history
 * - 1.00 18-10-25  yrh, first implementation
 * \endinternal
 */
#include "am_boot_kft.h"
#include "am_boot_kft_command.h"
#include "am_boot_kft_common.h"
#include "am_boot_kft_property.h"
#include "am_zlg116.h"
#include "am_boot_memory.h"
#include "ametal.h"
#include "am_types.h"
#include "am_int.h"
#include "am_boot_flash.h"

/**
 * \brief ״̬��
 */
static status_t __handle_command(am_boot_kft_command_dev_t *p_dev,uint8_t *p_packet, uint32_t packet_length);
static status_t __handle_data(am_boot_kft_command_dev_t *p_dev,am_bool_t *p_has_more_data);

status_t bootloader_command_pump(void *p_arg);

/**
 * \name ���������
 * @{
 */
void handle_reset(am_boot_kft_command_dev_t *p_dev, uint8_t *p_packet, uint32_t packet_length);
void handle_flash_erase_all(am_boot_kft_command_dev_t *p_dev, uint8_t *p_packet, uint32_t packet_length);
void handle_flash_erase_region(am_boot_kft_command_dev_t *p_dev, uint8_t *p_packet, uint32_t packet_length);
void handle_read_memory(am_boot_kft_command_dev_t *p_dev, uint8_t *p_packet, uint32_t packet_length);
void handle_fill_memory(am_boot_kft_command_dev_t *p_dev, uint8_t *p_packet, uint32_t packet_length);
void handle_set_property(am_boot_kft_command_dev_t *p_dev, uint8_t *p_packet, uint32_t packet_length);
void handle_get_property(am_boot_kft_command_dev_t *p_dev, uint8_t *p_packet, uint32_t packet_length);
void handle_write_memory(am_boot_kft_command_dev_t *p_dev, uint8_t *p_packet, uint32_t packet_length);
void handle_execute(am_boot_kft_command_dev_t *p_dev, uint8_t *p_packet, uint32_t packet_length);
void handle_call(am_boot_kft_command_dev_t *p_dev, uint8_t *p_packet, uint32_t packet_length);
void handle_flash_read_resource(am_boot_kft_command_dev_t *p_dev, uint8_t *p_packet, uint32_t length);
//void handle_reliable_update(uint8_t *packet, uint32_t packetLength);

/** @} */

/**
 * \name ������Ӧ����
 * @{
 */
void send_read_memory_response(am_boot_kft_command_dev_t *p_dev, uint32_t command_status, uint32_t length);
void send_get_property_response(am_boot_kft_command_dev_t *p_dev, uint32_t command_status, uint32_t *p_value, uint32_t num_values);
void send_flash_read_once_response(am_boot_kft_command_dev_t *p_dev, uint32_t command_status, uint32_t *p_value, uint32_t byte_count);
void send_flash_read_resource_response(am_boot_kft_command_dev_t *p_dev, uint32_t command_status, uint32_t length);
void send_generic_response(am_boot_kft_command_dev_t *p_dev, uint32_t command_status, uint32_t command_tag);

/** @} */

/**
 * \brief ���ݽ׶�
 */
void     reset_data_phase(am_boot_kft_command_dev_t *p_dev);
void     finalize_data_phase(am_boot_kft_command_dev_t *p_dev, status_t status);
status_t handle_data_producer(am_boot_kft_command_dev_t *p_dev, am_bool_t *p_has_more_data);
status_t handle_data_consumer(am_boot_kft_command_dev_t *p_dev, am_bool_t *p_has_more_data);


/**
 * \brief ������ص�������
 */
static am_boot_kft_command_handler_entry_t __g_command_handler_table[] = {
    { handle_flash_erase_all, NULL },              // KFT_CommandTag_FlashEraseAll = 0x01
    { handle_flash_erase_region, NULL },           // kCommandTag_FlashEraseRegion = 0x02
    { handle_read_memory, handle_data_producer },  // kCommandTag_ReadMemory = 0x03
    { handle_write_memory, handle_data_consumer }, // kCommandTag_WriteMemory = 0x04
    { handle_fill_memory, NULL },                  // kCommandTag_FillMemory = 0x05
#if BL_FEATURE_FLASH_SECURITY
    { handle_flash_security_disable, NULL },       // kCommandTag_FlashSecurityDisable = 0x06
#else
    { 0 },
#endif                                             // BL_FEATURE_FLASH_SECURITY
    { handle_get_property, NULL },                 // kCommandTag_GetProperty = 0x07
    { NULL, handle_data_consumer },                // kCommandTag_ReceiveSbFile = 0x08
    { 0/*handle_execute*/, NULL },                 // kCommandTag_Execute = 0x09
    { 0/*handle_call*/, NULL },                    // kCommandTag_Call = 0x0a
    { handle_reset, NULL },                        // kCommandTag_Reset = 0x0b
    { handle_set_property, NULL },                 // kCommandTag_SetProperty = 0x0c
#if BL_FEATURE_ERASEALL_UNSECURE
    { handle_flash_erase_all_unsecure, NULL },     // kCommandTag_FlashEraseAllUnsecure = 0x0d
#else                                              // BL_FEATURE_ERASEALL_UNSECURE
    { 0 },                                         // kCommandTag_FlashEraseAllUnsecure = 0x0d
#endif                                             // KFT_FEATURE_ERASEALL_UNSECURE
    { NULL, NULL },                                // kCommandTag_ProgramOnce = 0x0e
    { NULL, NULL },                                // kCommandTag_ReadOnce = 0x0f
    { NULL, handle_data_producer },                // kCommandTag_ReadResource = 0x10
#if BL_FEATURE_QSPI_MODULE
    { handle_configure_quadspi, NULL },            // kCommandTag_ConfigureQuadSpi = 0x11
#else                                              // KFT_FEATURE_QSPI_MODULE
    { 0 },                                         // kCommandTag_ConfigureQuadSpi = 0x11
#endif                                             // KFT_FEATURE_QSPI_MODULE
#if BL_FEATURE_RELIABLE_UPDATE
    { handle_reliable_update, NULL },              // kCommandTag_ReliableUpdate = 0x12
#else
    { 0 },                                         // kCommandTag_ReliableUpdate = 0x12
#endif                                             // KFT_FEATURE_RELIABLE_UPDATE

};

static struct am_boot_kft_command_funcs __g_command_funcs = {
    bootloader_command_pump,
};
static am_boot_kft_command_dev_t __g_command_dev;

/**
 * \brief bootloader ����ģ���ʼ��
*/
am_boot_kft_command_handle_t am_boot_kft_command_init(am_boot_kft_packet_handle_t   packet_handle,
                                                      am_boot_mem_handle_t          memory_handle,
                                                      am_boot_kft_property_handle_t property_handle,
                                                      am_boot_flash_handle_t        flash_handle)
{
    __g_command_dev.p_handler_table = __g_command_handler_table;
    __g_command_dev.packet_handle   = packet_handle;
    __g_command_dev.property_handle = property_handle;
    __g_command_dev.memory_handle   = memory_handle;
    __g_command_dev.flash_handle    = flash_handle;

    __g_command_dev.command_serv.p_funcs = &__g_command_funcs;
    __g_command_dev.command_serv.p_drv = &__g_command_dev;

    __g_command_dev.state_data.state = AM_BOOT_KFT_COMMAND_STATE_COMMAND_PHASE;
    return &__g_command_dev.command_serv;
}

/**
 * \brief  ִ������״̬��
 *
 * \note ִ��һ������������ݵĴ���
*/
status_t bootloader_command_pump(void *p_arg)
{
    am_boot_kft_command_dev_t *p_dev = (am_boot_kft_command_dev_t *)p_arg;
    am_boot_kft_command_processor_data_t *p_state_data = &p_dev->state_data;
    status_t         status = KFT_STATUS_SUCCESS;
    am_bool_t has_more_data = AM_FALSE;

    if (p_dev->packet_handle) {
        switch (p_dev->state_data.state) {
        default:
        case AM_BOOT_KFT_COMMAND_STATE_COMMAND_PHASE:
            status = p_dev->packet_handle->p_funcs->pfn_read_packet(
                p_dev->packet_handle->p_drv,
               &p_state_data->p_packet,
               &p_state_data->packet_length,
                KFT_PACKET_TYPE_COMMAND);

            if ((status != KFT_STATUS_SUCCESS) &&
                (status != KFT_STATUS_ABORT_DATA_PHASE) &&
                (status != KFT_STATUS_PING)) {
                am_kprintf("Error: readPacket returned status 0x%x\r\n", status);
                break;
            }
            if (p_state_data->packet_length == 0) {
                // No command packet is available. Return success.
                break;
            }
            status = __handle_command(p_dev,
                                      p_state_data->p_packet,
                                      p_state_data->packet_length);
            if (status != KFT_STATUS_SUCCESS) {
                am_kprintf("Error: handle_command returned status 0x%x\r\n", status);
                break;
            }
            p_state_data->state = AM_BOOT_KFT_COMMAND_STATE_DATA_PHASE;
            break;

        case AM_BOOT_KFT_COMMAND_STATE_DATA_PHASE:
            status = __handle_data(p_dev, &has_more_data);
            if (status != KFT_STATUS_SUCCESS) {
                am_kprintf("Error: handle_data returned status 0x%x\r\n", status);
                p_state_data->state = AM_BOOT_KFT_COMMAND_STATE_COMMAND_PHASE;
                break;
            }
            p_state_data->state =
                has_more_data ? AM_BOOT_KFT_COMMAND_STATE_DATA_PHASE : AM_BOOT_KFT_COMMAND_STATE_COMMAND_PHASE;
            break;
        }
    }

    return status;
}

/**
 * \brief Ѱ�����������
 *
 * \note ���û���������������NULL
*/
static const am_boot_kft_command_handler_entry_t *find_entry(
    am_boot_kft_command_dev_t *p_dev,uint8_t tag)
{
    if (tag < AM_BOOT_KFT_FIRST_COMMAND_TAG || tag > AM_BOOT_KFT_LAST_COMMAND_TAG) {
        return 0; // invalid command
    }
    const am_boot_kft_command_handler_entry_t *p_entry =
        &p_dev->p_handler_table[(tag - AM_BOOT_KFT_FIRST_COMMAND_TAG)];

    return p_entry;
}

/**
 * \brief ��������
*/
static status_t __handle_command(am_boot_kft_command_dev_t *p_dev,uint8_t *p_packet, uint32_t packet_length)
{
    command_packet_t *p_command_packet = (command_packet_t *)p_packet;
    uint8_t           command_tag    = p_command_packet->command_tag;
    status_t          status        = KFT_STATUS_SUCCESS;

    /* Look up the handler entry and save it for the data phaase. */
    p_dev->state_data.p_handler_entry = find_entry(p_dev, command_tag);

    if (p_dev->state_data.p_handler_entry &&
        p_dev->state_data.p_handler_entry->pfn_handle_Command) {
        p_dev->state_data.p_handler_entry->pfn_handle_Command(p_dev, p_packet, packet_length);
        return KFT_STATUS_SUCCESS;
    }
    else {
        /* We don't recognize this command, so return an error response. */
        am_kprintf("unknown command 0x%x\r\n", p_command_packet->command_tag);
        status = KFT_STATUS_UNKNOWN_COMMAND;
    }
    /* Should only get to this point if an error occurred before running the command handler. */
    send_generic_response(p_dev, status, command_tag);
    return status;
}

/**
 * \brief ��������
*/
static status_t __handle_data(am_boot_kft_command_dev_t *p_dev,am_bool_t *p_has_more_data)
{
    if (p_dev->state_data.p_handler_entry) {
        /* Run data phase if present, otherwise just return success. */
        *p_has_more_data = 0;
        return p_dev->state_data.p_handler_entry->pfn_handle_Data ?
                   p_dev->state_data.p_handler_entry->pfn_handle_Data(p_dev, p_has_more_data) :
                   KFT_STATUS_SUCCESS;
    }

    am_kprintf("Error: no handler entry for data phase\r\n");
    return KFT_STATUS_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
// Command Handlers
////////////////////////////////////////////////////////////////////////////////

/**
 * \brief ��������
*/
void handle_reset(am_boot_kft_command_dev_t *p_dev,uint8_t *p_packet, uint32_t packet_length)
{
    am_kprintf("bootloader : reset device\r\n");
    command_packet_t *p_command_packet = (command_packet_t *)p_packet;
    send_generic_response(p_dev, KFT_STATUS_SUCCESS, p_command_packet->command_tag);

    /* Wait for the ack from the host to the generic response */
    p_dev->packet_handle->p_funcs->pfn_finalize(p_dev->packet_handle->p_drv);

    am_boot_reset();
    // Does not get here.
    assert(0);
}


//! @brief Reset data phase variables.
void reset_data_phase(am_boot_kft_command_dev_t *p_dev)
{
    memset(&p_dev->state_data.data_phase, 0, sizeof(p_dev->state_data.data_phase));
}

/**
 * \brief �������е�flash�������
 *
*/
void handle_flash_erase_all(am_boot_kft_command_dev_t *p_dev,uint8_t *p_packet, uint32_t packet_length)
{
    flash_erase_all_packet_t *p_command_packet = (flash_erase_all_packet_t *)p_packet;
    status_t status = KFT_STATUS_SUCCESS;

    // For target without QSPI module, ignore the memory identifier
    status = am_boot_flash_erase_all(p_dev->flash_handle);

    send_generic_response(p_dev, status, p_command_packet->command_packet.command_tag);
}

/**
 * \brief flash�������ֵ������
*/
void handle_flash_erase_region(am_boot_kft_command_dev_t *p_dev,
                               uint8_t                   *p_packet,
                               uint32_t                   packet_length)
{
    flash_erase_region_packet_t *p_command = (flash_erase_region_packet_t *)p_packet;
    status_t status = KFT_STATUS_SUCCESS;

    status = am_boot_flash_erase_region(p_dev->flash_handle,
                                        p_command->start_address,
                                        p_command->byte_count);

    send_generic_response(p_dev, status, p_command->command_packet.command_tag);
    am_kprintf("bootloader : firmware transmission is ready\r\n");
}

/**
 * \brief BootLoader���Ի�ȡ�����
 *
*/
void handle_get_property(am_boot_kft_command_dev_t *p_dev,uint8_t *p_packet, uint32_t packet_length)
{
    get_property_packet_t *p_command = (get_property_packet_t *)p_packet;

    uint32_t *value = NULL;
    uint32_t  value_size = 0;
    status_t status = p_dev->property_handle->p_funcs->pfn_get_property(
         p_dev->property_handle->p_drv,
         p_command->property_tag,
         p_command->memory_id,
         (const void **)&value,
         &value_size);

    /* Make sure the property's size is no more than the size of the max number of return parameters. */
    assert(value_size <= (AM_BOOT_KFT_MAX_PROPERTY_RETURN_VALUES * sizeof(uint32_t)));

    /* Currently there are no property responses that contain a data phase. */
    p_dev->state_data.data_phase.count = 0;
    send_get_property_response(p_dev, status, value, (value_size / sizeof(uint32_t)));
}


/**
 * \brief BootLoader�������������
 *
*/
void handle_set_property(am_boot_kft_command_dev_t *p_dev,uint8_t *p_packet, uint32_t packet_length)
{
    set_property_packet_t *p_command = (set_property_packet_t *)p_packet;

    status_t status = p_dev->property_handle->p_funcs->pfn_set_uint32_property(
        p_dev->property_handle->p_drv,
        p_command->property_tag,
        p_command->property_value
        );

    send_generic_response(p_dev, status, p_command->command_packet.command_tag);
}

/**
 * \brief д�ڴ�������
 *
*/
void handle_write_memory(am_boot_kft_command_dev_t *p_dev,uint8_t *p_packet, uint32_t packet_length)
{
    write_memory_packet_t *p_command = (write_memory_packet_t *)p_packet;

    /* Start the data phase. */
    reset_data_phase(p_dev);
    p_dev->state_data.data_phase.count       = p_command->byte_count;
    p_dev->state_data.data_phase.address     = p_command->start_address;
    p_dev->state_data.data_phase.command_tag = AM_BOOT_KFT_COMMAND_TAG_WRITE_MEMORY;
    send_generic_response(p_dev, KFT_STATUS_SUCCESS, p_command->command_packet.command_tag);
    am_kprintf("bootloader : firmware updating...\r\n");
}

/**
 * \brief ���ڴ�������
 *
*/
void handle_read_memory(am_boot_kft_command_dev_t *p_dev,uint8_t *p_packet, uint32_t packet_length)
{
    read_memory_packet_t *p_command = (read_memory_packet_t *)p_packet;

    // Start the data phase.
    reset_data_phase(p_dev);
    p_dev->state_data.data_phase.count       = p_command->byte_count;
    p_dev->state_data.data_phase.address     = p_command->start_address;
    p_dev->state_data.data_phase.command_tag = AM_BOOT_KFT_COMMAND_TAG_READ_MEMORY;
    send_read_memory_response(p_dev,KFT_STATUS_SUCCESS, p_command->byte_count);
}

/**
 * \brief ������ݽ׶Σ�ѡ���Է���һ����Ӧ
 *
*/
void finalize_data_phase(am_boot_kft_command_dev_t *p_dev,status_t status)
{
    p_dev->state_data.data_phase.address = 0;
    p_dev->state_data.data_phase.count = 0;

    // Force to write cached data to target memory
    if (p_dev->state_data.data_phase.command_tag == AM_BOOT_KFT_COMMAND_TAG_WRITE_MEMORY)
    {
//        assert(p_dev->memory_handle->p_funcs->pfn_flush);
//        status_t flushStatus =
//            p_dev->memory_handle->p_funcs->pfn_flush(p_dev->memory_handle->p_drv);

        // Update status only if the last operation result is successfull in order to reflect
        // real result of the write operation.
        if (status == KFT_STATUS_SUCCESS)
        {
            status = KFT_STATUS_SUCCESS;
        }
    }

    // Send final response packet.
    send_generic_response(p_dev, status, p_dev->state_data.data_phase.command_tag);
}

/**
 * \brief �������������ŵ����ݽ׶Σ���������ȡ��
 *
*/
status_t handle_data_consumer(am_boot_kft_command_dev_t *p_dev,am_bool_t *p_has_more_data)
{
    if (p_dev->state_data.data_phase.count == 0)
    {
        // No data phase.
        *p_has_more_data = AM_FALSE;
        finalize_data_phase(p_dev, KFT_STATUS_SUCCESS);
        return KFT_STATUS_SUCCESS;
    }

    *p_has_more_data      = true;
    uint32_t remaining    = p_dev->state_data.data_phase.count;
    uint32_t data_address = p_dev->state_data.data_phase.address;
    uint8_t *p_packet;
    uint32_t packet_length = 0;
    status_t status;

    // Read the data packet.
    status = p_dev->packet_handle->p_funcs->pfn_read_packet(
         p_dev->packet_handle->p_drv, &p_packet, &packet_length, KFT_PACKET_TYPE_DATA);
    if (status != KFT_STATUS_SUCCESS)
    {
        // Abort data phase due to error.
        am_kprintf("consumer abort data phase due to status 0x%x\r\n", status);
        p_dev->packet_handle->p_funcs->pfn_abort_data_phase(p_dev->packet_handle->p_drv);
        finalize_data_phase(p_dev, status);
        *p_has_more_data = AM_FALSE;
        return KFT_STATUS_SUCCESS;
    }
    if (packet_length == 0)
    {
        // Sender requested data phase abort.
        am_kprintf("Data phase aborted by sender\r\n");
        finalize_data_phase(p_dev, KFT_STATUS_ABORT_DATA_PHASE);
        *p_has_more_data = AM_FALSE;
        return KFT_STATUS_SUCCESS;
    }

    packet_length = MIN(packet_length, remaining);

    if (p_dev->state_data.data_phase.command_tag == AM_BOOT_KFT_COMMAND_TAG_RECEIVE_SB_FILE)
    {
//        // Consumer is sb loader state machine
//        p_dev->state_data.dataPhase.data = packet;
//        p_dev->state_data.dataPhase.dataBytesAvailable = packetLength;
//
//        status = sbloader_pump(packet, packetLength);
//
//        // kStatusRomLdrDataUnderrun means need more data
//        // kStatusRomLdrSectionOverrun means we reached the end of the sb file processing
//        // either of these are OK
//        if ((status == kStatusRomLdrDataUnderrun) || (status == kStatusRomLdrSectionOverrun))
//        {
//            status = KFT_STATUS_SUCCESS;
//        }
    }
    else
    {
        // Consumer is memory interface.
        am_boot_mem_write(p_dev->memory_handle,
                data_address,
                          packet_length,
                          p_packet);

        data_address += packet_length;
    }

    remaining -= packet_length;

    if (remaining == 0)
    {
        finalize_data_phase(p_dev, status);
        *p_has_more_data = AM_FALSE;
    }
    else if (status != KFT_STATUS_SUCCESS)
    {
        // Abort data phase due to error.
        am_kprintf("Data phase error 0x%x, aborting\r\n", status);
        p_dev->packet_handle->p_funcs->pfn_abort_data_phase(p_dev->packet_handle->p_drv);
        finalize_data_phase(p_dev,status);
        *p_has_more_data = AM_FALSE;
    }
    else
    {
        p_dev->state_data.data_phase.count = remaining;
        p_dev->state_data.data_phase.address = data_address;
    }

    return KFT_STATUS_SUCCESS;
}

/**
 * \brief �������������������ݽ׶Σ����͵�������
*/
status_t handle_data_producer(am_boot_kft_command_dev_t *p_dev, am_bool_t *p_has_more_data)
{
    if (p_dev->state_data.data_phase.count == 0) {
        // No data phase.
        *p_has_more_data = AM_FALSE;
        finalize_data_phase(p_dev, KFT_STATUS_SUCCESS);
        return KFT_STATUS_SUCCESS;
    }

    *p_has_more_data = true;
    uint32_t remaining   = p_dev->state_data.data_phase.count;
    uint32_t data_address = p_dev->state_data.data_phase.address;
    uint8_t *data        = p_dev->state_data.data_phase.p_data;
    uint8_t command_tag   = p_dev->state_data.data_phase.command_tag;
    status_t status      = KFT_STATUS_SUCCESS;

    // Initialize the data packet to send.
    uint32_t packet_size;
    uint8_t  packet[AM_BOOT_KFT_MIN_PACKET_BUFFER_SIZE];

    // Copy the data into the data packet.
    packet_size = MIN(AM_BOOT_KFT_MIN_PACKET_BUFFER_SIZE, remaining);
    if (data) {
        /* Copy data using compiler-generated memcpy */
        memcpy(packet, data, packet_size);
        data += packet_size;
        status = KFT_STATUS_SUCCESS;
    }
    else {
        if (command_tag == AM_BOOT_KFT_COMMAND_TAG_READ_MEMORY) {
            /* Copy data using memory interface. */
            status = am_boot_mem_read(p_dev->memory_handle,
                                      data_address,
                                      packet_size,
                                      packet);
        }
        data_address += packet_size;
    }
    if (status != KFT_STATUS_SUCCESS) {
        am_kprintf("Error: %s returned status 0x%x, abort data phase\r\n",
                     (command_tag == AM_BOOT_KFT_COMMAND_TAG_READ_MEMORY) ? "read memory" : "flash read resource", status);
        /* Send zero length packet to tell host we are aborting data phase */
        p_dev->packet_handle->p_funcs->pfn_write_packet(
            p_dev->packet_handle->p_drv,
            (const uint8_t *)packet,
            0,
            KFT_PACKET_TYPE_DATA);

        finalize_data_phase(p_dev, status);

        *p_has_more_data = AM_FALSE;
        return KFT_STATUS_SUCCESS;
    }
    remaining -= packet_size;

    status =  p_dev->packet_handle->p_funcs->pfn_write_packet(
        p_dev->packet_handle->p_drv,
        (const uint8_t *)packet,
        packet_size,
        KFT_PACKET_TYPE_DATA);

    if (remaining == 0) {
        finalize_data_phase(p_dev, status);
        *p_has_more_data = AM_FALSE;
    }
    else if (status != KFT_STATUS_SUCCESS) {
        am_kprintf("writePacket aborted due to status 0x%x\r\n", status);
        finalize_data_phase(p_dev, status);
        *p_has_more_data = AM_FALSE;
    }
    else {
        p_dev->state_data.data_phase.count   = remaining;
        p_dev->state_data.data_phase.address = data_address;
    }

    return KFT_STATUS_SUCCESS;
}


/**
 * \brief ����ڴ����������
 *
*/
void handle_fill_memory(am_boot_kft_command_dev_t *p_dev, uint8_t *p_packet, uint32_t packet_Length)
{
    fill_memory_packet_t *p_command = (fill_memory_packet_t *)p_packet;
    status_t status = KFT_STATUS_SUCCESS;
    send_generic_response(p_dev, status, p_command->command_packet.command_tag);
}

/**
 * \brief ִ�����������
*/
void handle_execute(am_boot_kft_command_dev_t *p_dev, uint8_t *p_packet, uint32_t packet_length)
{
    execute_call_packet_t *p_command = (execute_call_packet_t *)p_packet;

    static uint32_t s_addr = 0;
    uint32_t call_address  = p_command->call_address;
    uint32_t argument_word = p_command->argument_word;
    s_addr = p_command->stackpointer;
    status_t responseStatus = KFT_STATUS_SUCCESS;

    // Validate call address.

    if (!am_boot_app_is_ready(call_address)) {
        // Invalid address, respond with BL_STATUS_INVALID_ARGUMENT.
        responseStatus = KFT_STATUS_INVALID_ARGUMENT;
    }

    // Send response immediately since call may not return
    send_generic_response(p_dev, responseStatus, p_command->command_packet.command_tag);

    if (responseStatus == KFT_STATUS_SUCCESS)
    {
        static call_function_t s_callFunction = 0;
        s_callFunction = (call_function_t)call_address;

        // Prepare for shutdown.
        //shutdown_cleanup(p_dev->sys_handle,kShutdownType_Shutdown);

        // Static variables are needed since we are changing the stack pointer out from under the compiler
        // we need to ensure the values we are using are not stored on the previous stack
        static uint32_t s_argument = 0;
        s_argument = argument_word;

        if (s_addr)
        {
            // Set main stack pointer and process stack pointer
//            if(p_dev->sys_handle->p_funcs->pfn_set_msp != NULL) {
//                am_bl_sys_set_msp(p_dev->sys_handle, s_addr);
//            }
//
//            if(p_dev->sys_handle->p_funcs->pfn_set_psp != NULL) {
//                am_bl_sys_set_psp(p_dev->sys_handle, s_addr);
//            }
        }

        s_callFunction(s_argument);
        // Dummy fcuntion call, should never go to this fcuntion call
        //shutdown_cleanup(p_dev->sys_handle,kShutdownType_Shutdown);
    }
}


/**
 * \brief �������������
 *
*/
void handle_call(am_boot_kft_command_dev_t *p_dev, uint8_t *p_packet, uint32_t packet_length)
{
    execute_call_packet_t *p_command = (execute_call_packet_t *)p_packet;
    status_t response_status = KFT_STATUS_SUCCESS;

    /* Validate call address. */
    if (!am_boot_app_is_ready(p_command->call_address)) {
        /* Invalid address, respond with BL_STATUS_INVALID_ARGUMENT. */
        response_status = KFT_STATUS_INVALID_ARGUMENT;
    }
    else {
        call_function_t call_function = (call_function_t)p_command->call_address;
        //shutdown_cleanup(p_dev->sys_handle, kShutdownType_Cleanup);
        response_status = call_function(p_command->argument_word);
    }

    send_generic_response(p_dev, response_status, p_command->command_packet.command_tag);
}


/**
 * \brief flash����Դ���������
*/
void handle_flash_read_resource(am_boot_kft_command_dev_t *p_dev,
                                uint8_t                   *p_packet,
                                uint32_t                   length)
{
    flash_read_resource_packet_t *p_command = (flash_read_resource_packet_t *)p_packet;

    // Start the data phase.
    reset_data_phase(p_dev);
    p_dev->state_data.data_phase.count = p_command->byte_count;
    p_dev->state_data.data_phase.address = p_command->start_address;
    p_dev->state_data.data_phase.command_tag = AM_BOOT_KFT_COMMAND_TAG_FLASH_READ_RESOURCE;
    p_dev->state_data.data_phase.option = (uint8_t)p_command->option;
    send_flash_read_resource_response(p_dev,KFT_STATUS_SUCCESS, p_command->byte_count);
}



/**
 * \brief ���ͻ�ȡ���ԵĻ�Ӧ
*/
void send_get_property_response(am_boot_kft_command_dev_t *p_dev,
                                uint32_t                   command_status,
                                uint32_t                  *p_value,
                                uint32_t                   num_values)
{
    get_property_response_packet_t response_packet;
    response_packet.command_packet.command_tag = AM_BOOT_KFT_COMMAND_TAG_GET_PROPERTY_RESPONSE;
    response_packet.command_packet.flags = 0;
    response_packet.command_packet.reserved = 0;
    response_packet.command_packet.parameter_count = 1 + num_values; // status + value words
    response_packet.status = command_status;

    for (uint32_t i = 0; i < num_values; ++i) {
        response_packet.property_value[i] = p_value[i];
    }

    uint32_t packet_size =
        sizeof(response_packet.command_packet) + (response_packet.command_packet.parameter_count * sizeof(uint32_t));

    status_t status = p_dev->packet_handle->p_funcs->pfn_write_packet(
        p_dev->packet_handle->p_drv,
        (const uint8_t *)&response_packet,
        packet_size,
        KFT_PACKET_TYPE_COMMAND);

    if (status != KFT_STATUS_SUCCESS) {
        am_kprintf("Error: writePacket returned status 0x%x\r\n", status);
    }
}

/**
 * \brief ����һ�����ڴ�Ļ�Ӧ��
*/
void send_read_memory_response(am_boot_kft_command_dev_t *p_dev,
                               uint32_t                   command_status,
                               uint32_t                   length)
{
    read_memory_response_packet_t response_packet;
    response_packet.command_packet.command_tag = AM_BOOT_KFT_COMMAND_TAG_READ_MEMORY_RESPONSE;
    response_packet.command_packet.flags = AM_BOOT_KFT_COMMAND_FLAG_HAS_DATA_PHASE;
    response_packet.command_packet.reserved = 0;
    response_packet.command_packet.parameter_count = 2;
    response_packet.status = command_status;
    response_packet.data_byte_count = length;

    status_t status = p_dev->packet_handle->p_funcs->pfn_write_packet(
         p_dev->packet_handle->p_drv,
         (const uint8_t *)&response_packet,
         sizeof(response_packet),
         KFT_PACKET_TYPE_COMMAND);
    if (status != KFT_STATUS_SUCCESS) {
        am_kprintf("Error: writePacket returned status 0x%x\r\n", status);
    }
}

/**
 * \brief ����һ��flash��һ�εĻ�Ӧ��
*/
void send_flash_read_once_response(am_boot_kft_command_dev_t *p_dev,
                                   uint32_t                   command_status,
                                   uint32_t                  *p_value,
                                   uint32_t                   byte_count)
{
    flash_read_once_response_packet_t response_packet;
    response_packet.command_packet.command_tag = AM_BOOT_KFT_COMMAND_TAG_FLASH_READ_ONCE_RESPONSE;
    response_packet.command_packet.flags = 0;
    response_packet.command_packet.reserved = 0;
    response_packet.command_packet.parameter_count = 2; // always includes two parameters: status and byte count
    response_packet.status = command_status;

    if (command_status == KFT_STATUS_SUCCESS) {
        response_packet.command_packet.parameter_count += byte_count / sizeof(uint32_t); // add parameter: data
        response_packet.byte_count = byte_count;
        memcpy(response_packet.data, p_value, byte_count);
    }
    else {
        response_packet.byte_count = 0;
    }

    uint32_t packetSize =
        sizeof(response_packet.command_packet) + (response_packet.command_packet.parameter_count * sizeof(uint32_t));

    status_t status = p_dev->packet_handle->p_funcs->pfn_write_packet(
        p_dev->packet_handle->p_drv,
        (const uint8_t *)&response_packet,
        packetSize,
        KFT_PACKET_TYPE_COMMAND);

    if (status != KFT_STATUS_SUCCESS) {
        am_kprintf("Error: writePacket returned status 0x%x\r\n", status);
    }
}

/**
 * \brief ���� flash ����Դ�Ļ�Ӧ
*/
void send_flash_read_resource_response(am_boot_kft_command_dev_t *p_dev, uint32_t command_status, uint32_t length)
{
    flash_read_resource_response_packet_t response_packet;
    response_packet.command_packet.command_tag = AM_BOOT_KFT_COMMAND_TAG_FLASH_READ_RESOURCE_RESPONSE;
    response_packet.command_packet.flags       = AM_BOOT_KFT_COMMAND_FLAG_HAS_DATA_PHASE;
    response_packet.command_packet.reserved    = 0;
    response_packet.command_packet.parameter_count = 2;
    response_packet.status = command_status;
    response_packet.data_byte_count = length;

    status_t status = p_dev->packet_handle->p_funcs->pfn_write_packet(
        p_dev->packet_handle->p_drv,
        (const uint8_t *)&response_packet,
        sizeof(response_packet),
        KFT_PACKET_TYPE_COMMAND);

    if (status != KFT_STATUS_SUCCESS) {
        am_kprintf("Error: writePacket returned status 0x%x\r\n", status);
    }
}

/**
 * \brief ����һ����ͨ��Ӧ
*/
void send_generic_response(am_boot_kft_command_dev_t *p_dev, uint32_t command_status, uint32_t command_tag)
{
    generic_response_packet_t responsePacket;
    responsePacket.command_packet.command_tag = AM_BOOT_KFT_COMMAND_TAG_GENERIC_RESPONSE;
    responsePacket.command_packet.flags = 0;
    responsePacket.command_packet.reserved = 0;
    responsePacket.command_packet.parameter_count = 2;
    responsePacket.status = command_status;
    responsePacket.command_tag = command_tag;

    status_t status = p_dev->packet_handle->p_funcs->pfn_write_packet(
        p_dev->packet_handle->p_drv,
        (const uint8_t *)&responsePacket,
        sizeof(responsePacket),
        KFT_PACKET_TYPE_COMMAND);

    if (status != KFT_STATUS_SUCCESS) {
        am_kprintf("Error: writePacket returned status 0x%x\r\n", status);
    }
}


/* end of file */