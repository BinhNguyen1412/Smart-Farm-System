#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <lcd1602.h>
#include <sqlite3.h>
#include <cjson/cJSON.h>
#include <wiringPi.h>
#include <mongoose.h>

sqlite3 *db;

#define LCD_ADDR 0x27  // I2C address of the LCD

void lcd_display(float temperature, float humidity, int air_quality, int water_level) {
    char line1[21], line2[21], line3[21], line4[21]; // Use 20 characters for each line, plus null terminator

    // Format strings with 20 characters per line
    snprintf(line1, sizeof(line1), "T:%.1fC  H:%.1f%%", temperature, humidity);
    snprintf(line2, sizeof(line2), "AQ: %d ppm W: %d%%", air_quality, water_level);

    // Display on LCD
    lcd1602WriteString(line1);  // Line 1
    lcd1602SetCursor(0, 1);     // Move the cursor to line 2
    lcd1602WriteString(line2);  // Line 2
    lcd1602SetCursor(0, 2);     // Move the cursor to line 3
}

// Function to handle API requests and return JSON data
static void handle_web_data(struct mg_connection *nc, struct http_message *hm) {
    const char *query = "SELECT temperature, humidity, air_quality, water_level FROM environment ORDER BY timestamp DESC LIMIT 1;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
    
    if (rc != SQLITE_OK) {
        mg_send_head(nc, 500, 0, "Content-Type: text/plain");
        mg_printf(nc, "\r\n");
        return;
    }

    // Fetch data from the database
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        float temperature = sqlite3_column_double(stmt, 0);
        float humidity = sqlite3_column_double(stmt, 1);
        int air_quality = sqlite3_column_int(stmt, 2);
        int water_level = sqlite3_column_int(stmt, 3);

        // Display data on the LCD
        lcd_display(temperature, humidity, air_quality, water_level);

        // Create JSON data to send back to the client
        cJSON *json = cJSON_CreateObject();
        cJSON_AddNumberToObject(json, "temperature", temperature);
        cJSON_AddNumberToObject(json, "humidity", humidity);
        cJSON_AddNumberToObject(json, "air_quality", air_quality);
        cJSON_AddNumberToObject(json, "water_level", water_level);

        // Convert JSON data to a string and send it to the client
        char *response = cJSON_Print(json);
        cJSON_Delete(json);
        
        mg_printf(nc, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",
                  strlen(response), response);
        free(response);
    } else {
        mg_send_head(nc, 404, 0, "Content-Type: text/plain");
        mg_printf(nc, "\r\n");
    }
    
    sqlite3_finalize(stmt);
}

// Function to process data sent from the client
static void handle_data(struct mg_connection *nc, struct http_message *hm) {
    char data[1024];
    size_t data_len = hm->body.len < sizeof(data) - 1 ? hm->body.len : sizeof(data) - 1;
    memcpy(data, hm->body.p, data_len);
    data[data_len] = '\0';

    cJSON *json = cJSON_Parse(data);
    if (!json) {
        mg_send_head(nc, 400, 0, "Content-Type: text/plain");
        mg_printf(nc, "\r\n");
        return;
    }

    cJSON *temp = cJSON_GetObjectItemCaseSensitive(json, "temperature");
    cJSON *hum = cJSON_GetObjectItemCaseSensitive(json, "humidity");
    cJSON *aq = cJSON_GetObjectItemCaseSensitive(json, "airQuality");
    cJSON *wl = cJSON_GetObjectItemCaseSensitive(json, "waterLevel");

    if (!cJSON_IsNumber(temp) || !cJSON_IsNumber(hum) || !cJSON_IsNumber(aq) || !cJSON_IsNumber(wl)) {
        cJSON_Delete(json);
        mg_send_head(nc, 400, 0, "Content-Type: text/plain");
        mg_printf(nc, "\r\n");
        return;
    }

    // Insert data into the database
    char sql[256];
    snprintf(sql, sizeof(sql), "INSERT INTO environment (temperature, humidity, air_quality, water_level) VALUES (%f, %f, %d, %d);",
             temp->valuedouble, hum->valuedouble, aq->valueint, wl->valueint);

    char *errmsg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &errmsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errmsg);
    }

    // Respond to the client
    mg_send_head(nc, 200, 0, "Content-Type: text/plain");
    mg_printf(nc, "\r\n");

    cJSON_Delete(json);
}

// Function to handle HTTP events
static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *hm = (struct http_message *)ev_data;

    switch (ev) {
        case MG_EV_HTTP_REQUEST:
            if (mg_vcmp(&hm->uri, "/data_endpoint") == 0) {
                handle_web_data(nc, hm);  // Handle data query requests
            } else if (mg_vcmp(&hm->uri, "/data") == 0) {
                handle_data(nc, hm);  // Receive data from the client and store it in the database
            } else {
                mg_serve_http(nc, hm, (struct mg_serve_http_opts) { .document_root = "www", .enable_directory_listing = "yes" });
            }
            break;
        default:
            break;
    }
}

int main(void) {
    struct mg_mgr mgr;
    struct mg_connection *nc;

    // Open SQLite database
    if (sqlite3_open("smart_farm.db", &db)) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // Initialize WiringPi
    if (wiringPiSetupGpio() == -1) {
        printf("Setup wiringPi failed!\n");
        return -1;
    }

    // Initialize LCD
    int rc = lcd1602Init(1, 0x27);
    if (rc) {
        printf("Initialization failed; aborting...\n");
        return 0;
    }

    mg_mgr_init(&mgr, NULL);
    nc = mg_bind(&mgr, "8080", ev_handler);
    if (nc == NULL) {
        fprintf(stderr, "Failed to create listener\n");
        return 1;
    }

    mg_set_protocol_http_websocket(nc);

    // Run server
    for (;;) {
        mg_mgr_poll(&mgr, 1000);
    }

    mg_mgr_free(&mgr);
    sqlite3_close(db);

    return 0;
}
