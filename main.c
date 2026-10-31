#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "mongoose.h"

#define DATA_FILE "electricity_records.txt"
#define TEMP_FILE "temp_records.txt"

struct Customer {
    int id;
    char name[50];
    int units;
    double bill;
    int paid;
};


/* Calculate electricity bill */
double calculateBill(int units) {

    double amount = 0;

    if (units <= 100) {

        amount = units * 1.50;

    } else if (units <= 200) {

        amount = 100 * 1.50
               + (units - 100) * 2.50;

    } else {

        amount = 100 * 1.50
               + 100 * 2.50
               + (units - 200) * 4.00;
    }

    return amount + 50.00;
}


/* Validate customer name */
int validName(const char *name) {

    int hasLetter = 0;

    if (
        name == NULL ||
        strlen(name) == 0 ||
        strlen(name) >= 50
    ) {
        return 0;
    }


    for (int i = 0; name[i] != '\0'; i++) {

        unsigned char ch =
            (unsigned char) name[i];


        if (isalpha(ch)) {

            hasLetter = 1;

        } else if (
            ch != ' ' &&
            ch != '-'
        ) {

            return 0;
        }
    }


    return hasLetter;
}


/* Check customer ID */
int customerExists(int id) {

    FILE *fp =
        fopen(DATA_FILE, "r");

    struct Customer c;


    if (fp == NULL) {
        return 0;
    }


    while (
        fscanf(
            fp,
            "%d|%49[^|]|%d|%lf|%d",
            &c.id,
            c.name,
            &c.units,
            &c.bill,
            &c.paid
        ) == 5
    ) {

        if (c.id == id) {

            fclose(fp);

            return 1;
        }
    }


    fclose(fp);

    return 0;
}


/* Add customer */
void addCustomer(
    struct mg_connection *c,
    struct mg_http_message *hm
) {

    struct Customer customer;

    double idValue;
    double unitsValue;

    char *name;


    if (
        !mg_json_get_num(
            hm->body,
            "$.id",
            &idValue
        )

        ||

        !mg_json_get_num(
            hm->body,
            "$.units",
            &unitsValue
        )

        ||

        !isfinite(idValue)
        ||
        !isfinite(unitsValue)

        ||

        idValue < 1
        ||
        idValue > 2147483647
        ||
        floor(idValue) != idValue

        ||

        unitsValue < 0
        ||
        unitsValue > 1000000
        ||
        floor(unitsValue) != unitsValue
    ) {

        mg_http_reply(
            c,
            400,
            "Content-Type: application/json\r\n",
            "{\"error\":\"Enter a valid ID and unit value\"}\n"
        );

        return;
    }


    name =
        mg_json_get_str(
            hm->body,
            "$.name"
        );


    if (!validName(name)) {

        if (name != NULL) {
            mg_free(name);
        }


        mg_http_reply(
            c,
            400,
            "Content-Type: application/json\r\n",
            "{\"error\":\"Use a name with letters, spaces or hyphens\"}\n"
        );

        return;
    }


    customer.id =
        (int) idValue;


    customer.units =
        (int) unitsValue;


    strcpy(
        customer.name,
        name
    );


    mg_free(name);


    if (
        customerExists(
            customer.id
        )
    ) {

        mg_http_reply(
            c,
            409,
            "Content-Type: application/json\r\n",
            "{\"error\":\"Customer ID already exists\"}\n"
        );

        return;
    }


    customer.bill =
        calculateBill(
            customer.units
        );


    customer.paid = 0;


    FILE *fp =
        fopen(
            DATA_FILE,
            "a"
        );


    if (fp == NULL) {

        mg_http_reply(
            c,
            500,
            "Content-Type: application/json\r\n",
            "{\"error\":\"Could not save record\"}\n"
        );

        return;
    }


    fprintf(
        fp,
        "%d|%s|%d|%.2f|%d\n",
        customer.id,
        customer.name,
        customer.units,
        customer.bill,
        customer.paid
    );


    fclose(fp);


    mg_http_reply(
        c,
        201,
        "Content-Type: application/json\r\n",
        "{\"id\":%d,\"name\":%m,\"units\":%d,"
        "\"bill\":%.2f,\"paid\":false}\n",

        customer.id,
        MG_ESC(customer.name),
        customer.units,
        customer.bill
    );
}


/* List customers */
void listCustomers(
    struct mg_connection *c
) {

    FILE *fp =
        fopen(
            DATA_FILE,
            "r"
        );


    struct Customer customer;

    char json[20000];

    size_t used = 0;

    int first = 1;


    used +=
        (size_t) snprintf(
            json,
            sizeof(json),
            "["
        );


    if (fp != NULL) {

        while (
            fscanf(
                fp,
                "%d|%49[^|]|%d|%lf|%d",
                &customer.id,
                customer.name,
                &customer.units,
                &customer.bill,
                &customer.paid
            ) == 5
        ) {

            int n =
                snprintf(
                    json + used,
                    sizeof(json) - used,

                    "%s"
                    "{\"id\":%d,"
                    "\"name\":\"%s\","
                    "\"units\":%d,"
                    "\"bill\":%.2f,"
                    "\"paid\":%s}",

                    first ? "" : ",",

                    customer.id,
                    customer.name,
                    customer.units,
                    customer.bill,

                    customer.paid
                        ? "true"
                        : "false"
                );


            if (
                n < 0 ||
                (size_t)n >=
                sizeof(json) - used
            ) {
                break;
            }


            used +=
                (size_t)n;


            first = 0;
        }


        fclose(fp);
    }


    if (
        used <
        sizeof(json) - 2
    ) {

        json[used++] = ']';

        json[used] = '\0';

    } else {

        json[
            sizeof(json) - 2
        ] = ']';

        json[
            sizeof(json) - 1
        ] = '\0';
    }


    mg_http_reply(
        c,
        200,
        "Content-Type: application/json\r\n",
        "%s\n",
        json
    );
}


/* Search customer */
void searchCustomer(
    struct mg_connection *c,
    struct mg_http_message *hm
) {

    double idValue;

    struct Customer customer;

    FILE *fp;

    int found = 0;


    if (
        !mg_json_get_num(
            hm->body,
            "$.id",
            &idValue
        )

        ||

        !isfinite(idValue)

        ||

        idValue < 1
        ||
        idValue > 2147483647
        ||
        floor(idValue) != idValue
    ) {

        mg_http_reply(
            c,
            400,
            "Content-Type: application/json\r\n",
            "{\"error\":\"Enter a valid customer ID\"}\n"
        );

        return;
    }


    fp =
        fopen(
            DATA_FILE,
            "r"
        );


    if (fp != NULL) {

        while (
            fscanf(
                fp,
                "%d|%49[^|]|%d|%lf|%d",
                &customer.id,
                customer.name,
                &customer.units,
                &customer.bill,
                &customer.paid
            ) == 5
        ) {

            if (
                customer.id ==
                (int) idValue
            ) {

                found = 1;

                break;
            }
        }


        fclose(fp);
    }


    if (!found) {

        mg_http_reply(
            c,
            404,
            "Content-Type: application/json\r\n",
            "{\"error\":\"Customer not found\"}\n"
        );

        return;
    }


    mg_http_reply(
        c,
        200,
        "Content-Type: application/json\r\n",

        "{\"id\":%d,"
        "\"name\":%m,"
        "\"units\":%d,"
        "\"bill\":%.2f,"
        "\"paid\":%s}\n",

        customer.id,

        MG_ESC(customer.name),

        customer.units,

        customer.bill,

        customer.paid
            ? "true"
            : "false"
    );
}


/* Mark bill as paid */
void markPaid(
    struct mg_connection *c,
    struct mg_http_message *hm
) {

    double idValue;

    struct Customer customer;

    FILE *in;
    FILE *out;

    int found = 0;
    int changed = 0;


    if (
        !mg_json_get_num(
            hm->body,
            "$.id",
            &idValue
        )

        ||

        !isfinite(idValue)

        ||

        idValue < 1
        ||
        idValue > 2147483647
        ||
        floor(idValue) != idValue
    ) {

        mg_http_reply(
            c,
            400,
            "Content-Type: application/json\r\n",
            "{\"error\":\"Enter a valid customer ID\"}\n"
        );

        return;
    }


    in =
        fopen(
            DATA_FILE,
            "r"
        );


    if (in == NULL) {

        mg_http_reply(
            c,
            404,
            "Content-Type: application/json\r\n",
            "{\"error\":\"No customer records found\"}\n"
        );

        return;
    }


    out =
        fopen(
            TEMP_FILE,
            "w"
        );


    if (out == NULL) {

        fclose(in);

        mg_http_reply(
            c,
            500,
            "Content-Type: application/json\r\n",
            "{\"error\":\"Could not create temporary file\"}\n"
        );

        return;
    }


    while (
        fscanf(
            in,
            "%d|%49[^|]|%d|%lf|%d",
            &customer.id,
            customer.name,
            &customer.units,
            &customer.bill,
            &customer.paid
        ) == 5
    ) {

        if (
            customer.id ==
            (int) idValue
        ) {

            found = 1;


            if (!customer.paid) {

                customer.paid = 1;

                changed = 1;
            }
        }


        fprintf(
            out,
            "%d|%s|%d|%.2f|%d\n",
            customer.id,
            customer.name,
            customer.units,
            customer.bill,
            customer.paid
        );
    }


    fclose(in);

    fclose(out);


    if (!found) {

        remove(TEMP_FILE);

        mg_http_reply(
            c,
            404,
            "Content-Type: application/json\r\n",
            "{\"error\":\"Customer not found\"}\n"
        );

        return;
    }


    if (!changed) {

        remove(TEMP_FILE);

        mg_http_reply(
            c,
            200,
            "Content-Type: application/json\r\n",
            "{\"message\":\"Bill is already paid\"}\n"
        );

        return;
    }


    if (
        remove(DATA_FILE) != 0
        ||
        rename(
            TEMP_FILE,
            DATA_FILE
        ) != 0
    ) {

        mg_http_reply(
            c,
            500,
            "Content-Type: application/json\r\n",
            "{\"error\":\"File update failed\"}\n"
        );

        return;
    }


    mg_http_reply(
        c,
        200,
        "Content-Type: application/json\r\n",
        "{\"message\":\"Payment marked as paid\"}\n"
    );
}


/* =========================================
   EDIT CUSTOMER
   ========================================= */

void editCustomer(
    struct mg_connection *c,
    struct mg_http_message *hm
) {

    double idValue;
    double unitsValue;

    char *name;

    struct Customer customer;

    FILE *in;
    FILE *out;

    int found = 0;


    /* Get ID and units */

    if (
        !mg_json_get_num(
            hm->body,
            "$.id",
            &idValue
        )

        ||

        !mg_json_get_num(
            hm->body,
            "$.units",
            &unitsValue
        )

        ||

        !isfinite(idValue)

        ||

        !isfinite(unitsValue)

        ||

        idValue < 1

        ||

        idValue > 2147483647

        ||

        floor(idValue) != idValue

        ||

        unitsValue < 0

        ||

        unitsValue > 1000000

        ||

        floor(unitsValue) != unitsValue
    ) {

        mg_http_reply(
            c,
            400,
            "Content-Type: application/json\r\n",
            "{\"error\":\"Enter valid ID and units\"}\n"
        );

        return;
    }


    /* Get new name */

    name =
        mg_json_get_str(
            hm->body,
            "$.name"
        );


    if (!validName(name)) {

        if (name != NULL) {
            mg_free(name);
        }


        mg_http_reply(
            c,
            400,
            "Content-Type: application/json\r\n",
            "{\"error\":\"Enter a valid customer name\"}\n"
        );

        return;
    }


    /* Open original file */

    in =
        fopen(
            DATA_FILE,
            "r"
        );


    if (in == NULL) {

        mg_free(name);

        mg_http_reply(
            c,
            404,
            "Content-Type: application/json\r\n",
            "{\"error\":\"No customer records found\"}\n"
        );

        return;
    }


    /* Open temporary file */

    out =
        fopen(
            TEMP_FILE,
            "w"
        );


    if (out == NULL) {

        fclose(in);

        mg_free(name);

        mg_http_reply(
            c,
            500,
            "Content-Type: application/json\r\n",
            "{\"error\":\"Could not create temporary file\"}\n"
        );

        return;
    }


    /* Read every customer */

    while (
        fscanf(
            in,
            "%d|%49[^|]|%d|%lf|%d",
            &customer.id,
            customer.name,
            &customer.units,
            &customer.bill,
            &customer.paid
        ) == 5
    ) {


        /* Find selected customer */

        if (
            customer.id ==
            (int) idValue
        ) {

            found = 1;


            /* Update name */

            strcpy(
                customer.name,
                name
            );


            /* Update units */

            customer.units =
                (int) unitsValue;


            /* Recalculate bill */

            customer.bill =
                calculateBill(
                    customer.units
                );


            /* Bill changed */

            customer.paid = 0;
        }


        /* Save record */

        fprintf(
            out,
            "%d|%s|%d|%.2f|%d\n",

            customer.id,

            customer.name,

            customer.units,

            customer.bill,

            customer.paid
        );
    }


    fclose(in);

    fclose(out);

    mg_free(name);


    /* Customer not found */

    if (!found) {

        remove(TEMP_FILE);

        mg_http_reply(
            c,
            404,
            "Content-Type: application/json\r\n",
            "{\"error\":\"Customer not found\"}\n"
        );

        return;
    }


    /* Replace old file */

    if (
        remove(DATA_FILE) != 0

        ||

        rename(
            TEMP_FILE,
            DATA_FILE
        ) != 0
    ) {

        mg_http_reply(
            c,
            500,
            "Content-Type: application/json\r\n",
            "{\"error\":\"File update failed\"}\n"
        );

        return;
    }


    /* Success */

    mg_http_reply(
        c,
        200,
        "Content-Type: application/json\r\n",
        "{\"message\":\"Customer updated successfully\"}\n"
    );
}


/* =========================================
   API ROUTES
   ========================================= */

static void eventHandler(
    struct mg_connection *c,
    int ev,
    void *ev_data
) {

    if (ev != MG_EV_HTTP_MSG) {
        return;
    }


    struct mg_http_message *hm =
        (struct mg_http_message *) ev_data;


    /* Health */

    if (
        mg_match(
            hm->uri,
            mg_str("/api/health"),
            NULL
        )
    ) {

        mg_http_reply(
            c,
            200,
            "Content-Type: application/json\r\n",
            "{\"status\":\"running\"}\n"
        );
    }


    /* Get customers */

    else if (
        mg_match(
            hm->uri,
            mg_str("/api/customers"),
            NULL
        )

        &&

        mg_match(
            hm->method,
            mg_str("GET"),
            NULL
        )
    ) {

        listCustomers(c);
    }


    /* Add customer */

    else if (
        mg_match(
            hm->uri,
            mg_str("/api/customers"),
            NULL
        )

        &&

        mg_match(
            hm->method,
            mg_str("POST"),
            NULL
        )
    ) {

        addCustomer(
            c,
            hm
        );
    }


    /* Search */

    else if (
        mg_match(
            hm->uri,
            mg_str("/api/search"),
            NULL
        )

        &&

        mg_match(
            hm->method,
            mg_str("POST"),
            NULL
        )
    ) {

        searchCustomer(
            c,
            hm
        );
    }


    /* Payment */

    else if (
        mg_match(
            hm->uri,
            mg_str("/api/pay"),
            NULL
        )

        &&

        mg_match(
            hm->method,
            mg_str("POST"),
            NULL
        )
    ) {

        markPaid(
            c,
            hm
        );
    }


    /* EDIT CUSTOMER */

    else if (
        mg_match(
            hm->uri,
            mg_str("/api/edit"),
            NULL
        )

        &&

        mg_match(
            hm->method,
            mg_str("POST"),
            NULL
        )
    ) {

        editCustomer(
            c,
            hm
        );
    }


    /* Website */

    else {

        struct mg_http_serve_opts opts = {
            .root_dir = "./web"
        };


        mg_http_serve_dir(
            c,
            hm,
            &opts
        );
    }
}


/* =========================================
   MAIN
   ========================================= */

int main(void) {

    struct mg_mgr manager;


    mg_mgr_init(
        &manager
    );


    if (
        mg_http_listen(
            &manager,
            "http://127.0.0.1:8001",
            eventHandler,
            NULL
        ) == NULL
    ) {

        printf(
            "Could not start server on port 8001.\n"
        );


        mg_mgr_free(
            &manager
        );


        return 1;
    }


    printf(
        "Electricity Bill Management System is running.\n"
    );


    printf(
        "Open http://127.0.0.1:8001 in your browser.\n"
    );


    printf(
        "Press Ctrl+C to stop the server.\n"
    );


    for (;;) {

        mg_mgr_poll(
            &manager,
            1000
        );
    }


    mg_mgr_free(
        &manager
    );


    return 0;
}