/*
 * com_modbus_tcp.c
 *
 *  Created on: 14 Sept 2026
 *      Author: Rafdi
 *
 *  Deskripsi: Universal Modbus TCP Driver - Support MASTER & SLAVE mode.
 *             Ganti nilai MODBUS_MODE di com_modbus_tcp.h untuk memilih mode.
 */

#include "com_modbus_tcp.h"


static const char *TAG = "MODBUS_TCP";

/* State untuk Mode MASTER */
static modbus_slave_node_t *s_slaves_ptr    = NULL;
static uint8_t              s_num_slaves    = 0;

/* State untuk Mode SLAVE */
static holding_reg_params_t s_holding_regs = {
    /*
     * Nilai awal Holding Register saat ESP32 baru dinyalakan.
     * Ubah nilai-nilai ini sesuai dengan kondisi awal perangkat Anda.
     */
    .sensor_1 = 0,
    .sensor_2 = 0,
    .sensor_3 = 0,
    .sensor_4 = 0,
    .sensor_5 = 0,
};
static uint8_t s_slave_id __attribute__((unused)) = 1;
static int create_tcp_socket_with_timeout(void) {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) return -1;

    struct timeval tv;
    tv.tv_sec  = MODBUS_CONNECT_TIMEOUT_MS / 1000;
    tv.tv_usec = (MODBUS_CONNECT_TIMEOUT_MS % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    return sock;
}

static void parse_response_to_struct(const uint8_t *resp, uint8_t reg_count,
                                     holding_reg_params_t *out) {
    /*
     * Format Modbus TCP Response (FC 0x03):
     *   Byte 0-1: Transaction ID
     *   Byte 2-3: Protocol ID (0x0000)
     *   Byte 4-5: Length
     *   Byte 6  : Unit ID
     *   Byte 7  : Function Code (0x03)
     *   Byte 8  : Byte Count (reg_count * 2)
     *   Byte 9+ : Data (2 byte per register, Big-Endian)
     */
    uint16_t *dest = (uint16_t *)out;
    for (int i = 0; i < reg_count && i < MODBUS_HOLDING_REG_COUNT; i++) {
        dest[i] = ((uint16_t)resp[9 + (i * 2)] << 8) | resp[9 + (i * 2) + 1];
    }
}

static void build_read_regs_request(uint8_t *frame, uint16_t transaction_id,
                                    uint8_t unit_id, uint16_t start_addr,
                                    uint16_t reg_count) {
    /*
     * Format Modbus TCP Request (FC 0x03 - Read Holding Registers):
     *   Byte 0-1: Transaction ID (angka bebas, dikirim balik oleh Slave)
     *   Byte 2-3: Protocol ID (selalu 0x0000 untuk Modbus)
     *   Byte 4-5: Length (jumlah byte setelah field ini = selalu 6)
     *   Byte 6  : Unit ID (Slave ID, 1-247)
     *   Byte 7  : Function Code (0x03)
     *   Byte 8-9: Starting Address (0x0000 = register 40001)
     *   Byte 10-11: Quantity of Registers
     */
    frame[0]  = (transaction_id >> 8) & 0xFF;
    frame[1]  = (transaction_id)      & 0xFF;
    frame[2]  = 0x00;  /* Protocol ID High */
    frame[3]  = 0x00;  /* Protocol ID Low  */
    frame[4]  = 0x00;  /* Length High      */
    frame[5]  = 0x06;  /* Length Low       */
    frame[6]  = unit_id;
    frame[7]  = 0x03;  /* FC 0x03: Read Holding Registers */
    frame[8]  = (start_addr >> 8) & 0xFF;
    frame[9]  = (start_addr)      & 0xFF;
    frame[10] = (reg_count >> 8)  & 0xFF;
    frame[11] = (reg_count)       & 0xFF;
}


/* =========================================================================
 *
 *  ██████╗  MODBUS TCP MASTER (CLIENT)
 *  ██╔══██╗
 *  ██║  ██║ Aktif polling data dari Slave di jaringan.
 *  ██║  ██║ Untuk mengaktifkan, set MODBUS_MODE = MODBUS_MODE_MASTER
 *  ██████╔╝ di com_modbus_tcp.h
 *  ╚═════╝
 *
 * ========================================================================= */
#if MODBUS_MODE == MODBUS_MODE_MASTER

static bool poll_one_slave(modbus_slave_node_t *node, uint16_t tx_id) {
    int sock = create_tcp_socket_with_timeout();
    if (sock < 0) {
        node->is_online = false;
        return false;
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port   = htons(node->port);
    inet_pton(AF_INET, node->ip, &dest_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        close(sock);
        node->is_online = false;
        return false;
    }

    uint8_t request[12];
    build_read_regs_request(request, tx_id, node->slave_id,
                            node->start_address, node->reg_count);

    if (send(sock, request, sizeof(request), 0) < 0) {
        close(sock);
        node->is_online = false;
        return false;
    }

    uint8_t response[128];
    int len = recv(sock, response, sizeof(response), 0);
    close(sock);

    int expected_len = 9 + (node->reg_count * 2);
    if (len < expected_len || response[7] != 0x03) {
        node->is_online = false;
        return false;
    }

    parse_response_to_struct(response, node->reg_count, &node->data);
    node->is_online = true;
    return true;
}

static void modbus_master_task(void *pvParameters) {
    uint16_t tx_counter = 0;
    while (1) {
        for (int i = 0; i < s_num_slaves; i++) {
            poll_one_slave(&s_slaves_ptr[i], ++tx_counter);
            vTaskDelay(pdMS_TO_TICKS(MODBUS_INTER_SLAVE_DELAY_MS));
        }
        vTaskDelay(pdMS_TO_TICKS(MODBUS_POLL_INTERVAL_MS));
    }
}

esp_err_t com_modbus_master_init(modbus_slave_node_t *slaves, uint8_t num_slaves) {
    if (slaves == NULL || num_slaves == 0) return ESP_ERR_INVALID_ARG;
    s_slaves_ptr = slaves;
    s_num_slaves = (num_slaves > MODBUS_MAX_SLAVES) ? MODBUS_MAX_SLAVES : num_slaves;

    xTaskCreate(modbus_master_task, "modbus_master", 4096, NULL, 5, NULL);
    return ESP_OK;
}

bool com_modbus_get_slave_data(uint8_t slave_idx, holding_reg_params_t *out_data) {
    if (slave_idx >= s_num_slaves || out_data == NULL) return false;
    *out_data = s_slaves_ptr[slave_idx].data;
    return s_slaves_ptr[slave_idx].is_online;
}

bool com_modbus_is_slave_online(uint8_t slave_idx) {
    if (slave_idx >= s_num_slaves) return false;
    return s_slaves_ptr[slave_idx].is_online;
}

esp_err_t com_modbus_slave_init(uint8_t slave_id) {
    (void)slave_id;
    return ESP_OK;
}

void com_modbus_set_holding_regs(const holding_reg_params_t *regs) { (void)regs; }
holding_reg_params_t com_modbus_get_holding_regs(void) { return s_holding_regs; }


/* =========================================================================
 *
 *  ███████╗ MODBUS TCP SLAVE (SERVER)
 *  ██╔════╝
 *  ███████╗ Pasif menunggu query dari Master (SCADA/PLC/PC).
 *  ╚════██║ Untuk mengaktifkan, set MODBUS_MODE = MODBUS_MODE_SLAVE
 *  ███████║ di com_modbus_tcp.h
 *  ╚══════╝
 *
 * ========================================================================= */
#elif MODBUS_MODE == MODBUS_MODE_SLAVE

/* Membentuk frame respons FC 0x03 dari data holding register */
static int build_read_regs_response(const uint8_t *req, uint8_t *resp,
                                    uint16_t start_addr, uint16_t reg_count) {
    uint16_t safe_count = reg_count;
    if (start_addr + safe_count > MODBUS_HOLDING_REG_COUNT) {
        safe_count = (uint16_t)(MODBUS_HOLDING_REG_COUNT - start_addr);
    }

    uint8_t byte_count  = (uint8_t)(safe_count * 2);
    uint16_t total_len  = 3 + byte_count;   /* Unit ID(1) + FC(1) + ByteCount(1) + Data */

    resp[0] = req[0];        /* Transaction ID High */
    resp[1] = req[1];        /* Transaction ID Low  */
    resp[2] = 0x00;          /* Protocol ID High    */
    resp[3] = 0x00;          /* Protocol ID Low     */
    resp[4] = (total_len >> 8) & 0xFF;
    resp[5] =  total_len       & 0xFF;
    resp[6] = req[6];        /* Unit ID             */
    resp[7] = 0x03;          /* Function Code       */
    resp[8] = byte_count;    /* Byte Count          */

    /* Salin data register (Big-Endian) */
    uint16_t *src = (uint16_t *)&s_holding_regs;
    for (int i = 0; i < safe_count; i++) {
        uint16_t val    = src[start_addr + i];
        resp[9 + i*2]   = (val >> 8) & 0xFF;
        resp[9 + i*2+1] = val & 0xFF;
    }
    return 9 + byte_count;
}

/* FreeRTOS task: listen, accept client, proses query, kirim respons */
static void modbus_slave_task(void *pvParameters) {
    struct sockaddr_in server_addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port        = htons(MODBUS_TCP_PORT),
    };

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(listen_sock, 4);
    ESP_LOGI(TAG, "Modbus TCP SLAVE Server siap di Port %d (Unit ID: %d).",
             MODBUS_TCP_PORT, s_slave_id);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock < 0) continue;

        char client_ip[16];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        ESP_LOGI(TAG, "Master terhubung dari %s", client_ip);

        uint8_t rx_buf[64];
        uint8_t tx_buf[128];

        while (1) {
            int len = recv(client_sock, rx_buf, sizeof(rx_buf), 0);
            if (len <= 0) break; /* Master disconnect */

            /* Hanya tangani FC 0x03: Read Holding Registers */
            if (len >= 12 && rx_buf[7] == 0x03) {
                uint16_t start_addr = ((uint16_t)rx_buf[8] << 8)  | rx_buf[9];
                uint16_t reg_count  = ((uint16_t)rx_buf[10] << 8) | rx_buf[11];

                int resp_len = build_read_regs_response(rx_buf, tx_buf, start_addr, reg_count);
                send(client_sock, tx_buf, resp_len, 0);
            }
            /* TODO: Tambahkan FC 0x06 (Write Single Register) atau FC 0x10 di sini */
        }

        close(client_sock);
        ESP_LOGI(TAG, "Master %s terputus.", client_ip);
    }
}

esp_err_t com_modbus_slave_init(uint8_t slave_id) {
    s_slave_id = (slave_id == 0) ? 1 : slave_id;
    xTaskCreate(modbus_slave_task, "modbus_slave", 4096, NULL, 5, NULL);
    return ESP_OK;
}

void com_modbus_set_holding_regs(const holding_reg_params_t *regs) {
    if (regs) s_holding_regs = *regs;
}

holding_reg_params_t com_modbus_get_holding_regs(void) {
    return s_holding_regs;
}

/* Stub kosong agar tidak error saat MASTER API dipanggil di mode SLAVE */
esp_err_t com_modbus_master_init(modbus_slave_node_t *slaves, uint8_t num_slaves) {
    ESP_LOGW(TAG, "com_modbus_master_init() dipanggil, tapi mode aktif adalah SLAVE. Diabaikan.");
    return ESP_OK;
}
bool com_modbus_get_slave_data(uint8_t i, holding_reg_params_t *d) { (void)i; (void)d; return false; }
bool com_modbus_is_slave_online(uint8_t i) { (void)i; return false; }

#endif /* MODBUS_MODE */
