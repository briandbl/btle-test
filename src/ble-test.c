/*
 * Copyright (C) 2011-2012 by Matthias Ringwald
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. This software may not be used in a commercial product
 *    without an explicit license granted by the copyright holder. 
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

//*****************************************************************************
//
// att device demo
//
//*****************************************************************************

// TODO: seperate BR/EDR from LE ACL buffers
// TODO: move LE init into HCI
// ..

// NOTE: Supports only a single connection

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include <btstack/run_loop.h>

#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"

#include "att.h"

static att_connection_t att_connection;
static uint16_t         att_response_handle = 0;
static uint16_t         att_response_size   = 0;
static uint8_t          att_response_buffer[28];

void hexdump2(void *data, int size){
    int i;
    for (i=0; i<size;i++){
        printf("%02X ", ((uint8_t *)data)[i]);
    }
    printf("\n");
}

static void att_try_respond(void){
    if (!att_response_size) return;
    if (!att_response_handle) return;
    if (!hci_can_send_packet_now(HCI_ACL_DATA_PACKET)) return;
    
    // update state before sending packet
    uint16_t size = att_response_size;
    att_response_size = 0;
    l2cap_send_connectionless(att_response_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, att_response_buffer, size);
}


static void att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){
    if (packet_type != ATT_DATA_PACKET) return;
    
    att_response_handle = handle;
    att_response_size = att_handle_request(&att_connection, packet, size, att_response_buffer);
    att_try_respond();
}


// enable LE, setup ADV data
static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t addr;
    uint8_t adv_data[] = { 02, 01, 05,   03, 02, 0xf0, 0xff }; 
    switch (packet_type) {
            
		case HCI_EVENT_PACKET:
			switch (packet[0]) {
				
                case BTSTACK_EVENT_STATE:
					// bt stack activated, get started - set local name
					if (packet[2] == HCI_STATE_WORKING) {
					   printf("Working!\n");
						hci_send_cmd(&hci_read_local_supported_features);
					}
					break;
                
                case DAEMON_EVENT_HCI_PACKET_SENT:
                    att_try_respond();
                    break;
                    
                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            // reset connection MTU
                            att_connection.mtu = 23;
                            break;
                        default:
                            break;
                    }
                    break;

                case BTSTACK_EVENT_NR_CONNECTIONS_CHANGED:
				    if (packet[2]) {
                        printf("Connected.\n");
                    } else {
				        printf("Not connected.\n");
                    }
                    break;
                    
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    att_response_handle =0;
                    att_response_size = 0;
                    
                    // restart advertising
                    hci_send_cmd(&hci_le_set_advertise_enable, 1);
                    break;
                    
				case HCI_EVENT_COMMAND_COMPLETE:
					if (COMMAND_COMPLETE_EVENT(packet, hci_read_bd_addr)){
    					bt_flip_addr(addr, &packet[6]);
					    printf("BD ADDR: %s\n", bd_addr_to_str(addr));
						break;
					}
					if (COMMAND_COMPLETE_EVENT(packet, hci_read_local_supported_features)){
					     printf("Local supported features: %04X%04X\n", READ_BT_32(packet, 10), READ_BT_32(packet, 6));
                        hci_send_cmd(&hci_set_event_mask, 0xffffffff, 0x20001fff);
						break;
					}
					if (COMMAND_COMPLETE_EVENT(packet, hci_set_event_mask)){
                        hci_send_cmd(&hci_write_le_host_supported, 1, 1);
						break;
					}
					if (COMMAND_COMPLETE_EVENT(packet, hci_write_le_host_supported)){
				        hci_send_cmd(&hci_le_set_event_mask, 0xffffffff, 0xffffffff);
                    	break;
					}
					if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_event_mask)){
				        hci_send_cmd(&hci_le_read_buffer_size);
                    	break;
					}
				    if (COMMAND_COMPLETE_EVENT(packet, hci_le_read_buffer_size)){
					    printf("LE buffer size: %u, count %u\n", READ_BT_16(packet,6), packet[8]);
					   hci_send_cmd(&hci_le_read_supported_states);
					   break;
					}
				    if (COMMAND_COMPLETE_EVENT(packet, hci_le_read_supported_states)){
					   hci_send_cmd(&hci_le_set_advertising_parameters,  0x0400, 0x0800, 0, 0, 0, &addr, 0x07, 0);
					   break;
					}
				    if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_advertising_parameters)){
					   hci_send_cmd(&hci_le_set_advertising_data, sizeof(adv_data), adv_data);
					   break;
					}
				    if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_advertising_data)){
					   hci_send_cmd(&hci_le_set_scan_response_data, 10, adv_data);
					   break;
					}
				    if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_scan_response_data)){
					   hci_send_cmd(&hci_le_set_advertise_enable, 1);
					   break;
					}
				    if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_advertise_enable)){
                        hci_discoverable_control(1);
                        break;
					}
                    
			}
	}
}

// test profile
#include "profile.h"

// write requests
static void att_write_callback(uint16_t handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size, signature_t * signature){
    printf("WRITE Callback, handle %04x\n", handle);
    switch(handle){
        case 0x000b:
            buffer[buffer_size]=0;
            printf("New text: %s\n", buffer);
            break;
        case 0x000d:
            printf("New value: %u\n", buffer[0]);
            break;
    }
}

// main
int main(void)
{
	/// GET STARTED with BTstack ///
	btstack_memory_init();
    run_loop_init(RUN_LOOP_POSIX);
	
    // init HCI
	hci_transport_t    * transport = hci_transport_usb_instance();
	bt_control_t       * control   = NULL;
    hci_uart_config_t  * config    = NULL;
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
	hci_init(transport, config, control, remote_db);
	
    // set up l2cap_le
    l2cap_init();
    l2cap_register_fixed_channel(att_packet_handler, L2CAP_CID_ATTRIBUTE_PROTOCOL);
    l2cap_register_packet_handler(packet_handler);
    
    // set up ATT
    att_set_db(profile_data);
    att_set_write_callback(att_write_callback);
    att_dump_attributes();
    att_connection.mtu = 27;
    
	printf("Run...\n\r");

    // turn on!
	hci_power_control(HCI_POWER_ON);

    // go!
    run_loop_execute();	
    
    // happy compiler!
    return 0;
}

