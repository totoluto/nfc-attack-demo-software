/**
* @file main.c
* @brief main.c is the entry point for the RFID Attack Demo application.
* @author totoluto
* @date 28.10.2025
* @version 1.0
**/

#include "serial.h"
#include "gui.h"

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    serialInit();

    app = gtk_application_new("ch.hslu.rfid_attack_demo", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(guiActivate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    serialClose();
    return status;
}
