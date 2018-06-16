#ifndef __RCCI_TYPE_H
#define __RCCI_TYPE_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

//! Interface message type ID (includes all TCP & UDP messages)
typedef enum rcci_msg_id_e {
    rcci_msg_init = 0,      //!< RCC Initialization message ID
    rcci_msg_reg_service,   //!< RCC Register service client
    rcci_msg_unreg_service, //!< RCC Unregister service client

// not implemented yet :)
    rcci_msg_close,         //!< RCC Close message ID
    rcci_msg_ctrl,          //!< RCC Control message ID
    rcci_msg_stat,          //!< RCC Status message ID
    rcci_msg_dr_ctrl,       //!< RCC Drive control message ID
    rcci_msg_video,         //!< RCC Video message ID
    rcci_msg_nonexisting    //!< Must be last
} rcci_msg_id_t;

//!< Status enum
typedef enum rcci_status_id_e {
    rcci_status_ack = 0,      //!< ACK
    rcci_status_nack,         //!< NACK
    rcci_status_nonexisting   //<! Must be last
} rcci_status_id_t;

//!< Connection/client flags
typedef enum rcci_client_flags_s {
    rcci_client_flag_none  = 0,   // Empty flag field
    rcci_client_flag_init  = 1,   // Initialized connection
    rcci_client_flag_log   = 2,   // Logging service
    rcci_client_flag_drive = 4,   // Drive control of the vehicle
    rcci_client_flag_video = 5,   // Video streaming
    rcci_client_flag_nonexisting  // Must be last
} rcci_client_flags_t;

//! Measger header definition (starts with every packet)
typedef struct rcci_msg_header_s {
    uint16_t      magic;     //!< Magic word/packet identifier
    uint16_t      ver;       //!< Protocol version
    rcci_msg_id_t type;      //!< Message type id
    uint32_t      size;      //!< Size of message (includes header)
    uint32_t      crc;       //!< Message CRC
} rcci_msg_header_t;

//! Initialization message
const uint16_t rcci_msg_init_magic = 0xBEEF; //!< Init message magic (special case)
const uint16_t rcci_msg_init_ver   = 0xDEAD; //!< Init message version (special case)
typedef struct rcci_msg_init_s {
    rcci_msg_header_t   header;     //!< Header of the message
    rcci_status_id_t    status;     //!< Reply status (ignored on server RX)
    int                 log_port;   //!< UDP port for logging datastream (ignored on server RC)
    int                 drv_port;   //!< UDP port for driving control
} rcci_msg_init_t;

//! Register & unregister services
typedef struct rcci_msg_reg_service_s {
    rcci_msg_header_t   header;
    rcci_client_flags_t service;
    rcci_status_id_t    status;
    struct sockaddr_in  sockaddr;
    int                 params; // various parameters - in log it means 'dump full log' if params != 0
} rcci_msg_reg_service_t;

/*! Drive control message payload definition - it is used in UDP protocol so
  just bare info without any headers
*/
const int32_t rcci_msg_drv_max_param = 1000;
typedef struct rcci_msg_drv_ctrl_s {
    uint8_t           count;  //!< Incremental number
    int32_t           drive;  //!< New drive value
    int32_t           steer;  //!< New stearing value
} rcci_msg_drv_ctrl_t;

const int32_t rcci_msg_vframe_max_frame = (640*480*3);
const int32_t rcci_msg_vframe_max_packet_size = ((1<<16)-40);
typedef struct rcci_msg_vframe_s {
    rcci_msg_header_t header;
    uint8_t           frame[rcci_msg_vframe_max_frame];
} rcci_msg_vframe_t;

#endif // __RCCI__TYPE_H
