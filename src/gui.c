#include "gui.h"
#include "serial.h"

#define SERIAL_BUF_SIZE 256

static const char *AUTH_CMD = "auth";
static const char *AUTH_SUCCESS_CMD = "success";

static const char *CHECK_MODE_CMD = "checkMode";
static const char *CHECK_MODE_ON_CMD = "auth";

static const char *UID_CMD = "uid";

static bool checkMode = false;
static gchar *currentUIDValue = NULL;

static char serialBuffer[SERIAL_BUF_SIZE];
static int serialBufferLen = 0;

static GtkLabel *connectionStateLabel;
static GtkLabel *authentificationStateLabel;
static GtkLabel *currentUIDLabel;
static GtkLabel *accessStateLabel;

static GtkWidget *databaseUIDList;
static GtkWidget *newUIDList;
static GtkWidget *serialPortList;
static GtkTextView *logTextField;
static GtkScrolledWindow *logFieldScroller;

// -- Helpers --
typedef enum {
    UID_LIST_NONE = 0,
    UID_LIST_NEW,
    UID_LIST_DATABASE
} UIDListType;

typedef enum {
    ERROR_COLOR,
    SUCCESS_COLOR,
    WARNING_COLOR
} LabelColorType;

static GHashTable *uidMap = NULL;

void initUIDMap() {
    uidMap = g_hash_table_new(g_str_hash, g_str_equal);
}

UIDListType uid_get_list(const char *uid) {
    gpointer value = g_hash_table_lookup(uidMap, uid);
    return value ? GPOINTER_TO_INT(value) : UID_LIST_NONE;
}

void uid_set_list(const char *uid, UIDListType list) {
    g_hash_table_insert(uidMap, g_strdup(uid), GINT_TO_POINTER(list));
}

void uid_remove(const char *uid) {
    g_hash_table_remove(uidMap, uid);
}

static void trim(char *str) {
    char *end;
    char *dst = str;

    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) {
        *str = 0;
        return;
    }

    if (str != dst) {
        memmove(dst, str, strlen(str) + 1);
    }

    end = dst + strlen(dst) - 1;
    while (end > dst && isspace((unsigned char)*end)) end--;

    *(end + 1) = '\0';
}

void setLabelColor(GtkLabel *label, LabelColorType type) {
    const char *colorHex;

    switch(type) {
        case ERROR_COLOR:   colorHex = "#e01b24"; break;
        case SUCCESS_COLOR: colorHex = "#33cc66"; break;
        case WARNING_COLOR: colorHex = "#FFB300"; break;
        default:            colorHex = "#FFFFFF"; break;
    }

    PangoColor color;
    if (!pango_color_parse(&color, colorHex)) {
        color.red = 0xFFFF;
        color.green = 0xFFFF;
        color.blue = 0xFFFF;
    }

    PangoAttrList *attrList = pango_attr_list_new();

    PangoAttribute *colorAttr = pango_attr_foreground_new(color.red, color.green, color.blue);
    colorAttr->start_index = 0;
    colorAttr->end_index = G_MAXUINT;
    pango_attr_list_insert(attrList, colorAttr);

    PangoAttribute *sizeAttr = pango_attr_size_new(12288); // font size
    sizeAttr->start_index = 0;
    sizeAttr->end_index = G_MAXUINT;
    pango_attr_list_insert(attrList, sizeAttr);

    gtk_label_set_attributes(label, attrList);
    pango_attr_list_unref(attrList);
}

// -- Functionalities --

static gboolean sendInitialCommand(gpointer userData) {
    serialSend("getRFIDMode");
    return G_SOURCE_REMOVE;
}

void onRemoveClicked(GtkButton *button, gpointer userData) {
    GtkWidget *listRow = GTK_WIDGET(userData);
    GtkWidget *box = gtk_bin_get_child(GTK_BIN(listRow));

    GList *children = gtk_container_get_children(GTK_CONTAINER(box));
    GtkWidget *label = GTK_WIDGET(children->data);
    gchar *uid = g_strdup(gtk_label_get_text(GTK_LABEL(label)));
    g_list_free(children);

    uid_remove(uid);
    gtk_widget_destroy(listRow);
    g_free(uid);
}

void onAddClicked(GtkButton *button, gpointer userData) {
    GtkWidget *listRow = GTK_WIDGET(userData);
    GtkWidget *box = gtk_bin_get_child(GTK_BIN(listRow));

    GList *children = gtk_container_get_children(GTK_CONTAINER(box));
    GtkWidget *label = GTK_WIDGET(children->data);
    g_list_free(children);

    gchar *uid = g_strdup(gtk_label_get_text(GTK_LABEL(label)));
    gtk_widget_destroy(listRow);
    uid_set_list(uid, UID_LIST_DATABASE);

    GtkWidget *dbRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *dbLabel = gtk_label_new(uid);
    GtkWidget *removeButton = gtk_button_new_with_label("Remove");

    gtk_box_pack_start(GTK_BOX(dbRow), dbLabel, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(dbRow), removeButton, FALSE, FALSE, 0);

    gtk_widget_show_all(dbRow);

    GtkWidget *dbListRow = gtk_list_box_row_new();
    gtk_container_add(GTK_CONTAINER(dbListRow), dbRow);
    gtk_widget_show(dbListRow);

    g_signal_connect(removeButton, "clicked", G_CALLBACK(onRemoveClicked), dbListRow);

    gtk_list_box_insert(GTK_LIST_BOX(databaseUIDList), dbListRow, -1);

    g_free(uid);
}

void addUIDToList(const char *uid) {
    if (uid_get_list(uid) != UID_LIST_NONE) {
        return;
    }

    uid_set_list(uid, UID_LIST_NEW);

    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *label = gtk_label_new(uid);
    GtkWidget *button = gtk_button_new_with_label("Add");

    gtk_box_pack_start(GTK_BOX(row), label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), button, FALSE, FALSE, 0);

    gtk_widget_show_all(row);

    GtkWidget *list_row = gtk_list_box_row_new();
    gtk_container_add(GTK_CONTAINER(list_row), row);
    gtk_widget_show(list_row);

    g_signal_connect(button, "clicked", G_CALLBACK(onAddClicked), list_row);

    gtk_list_box_insert(GTK_LIST_BOX(newUIDList), list_row, -1);
}

static void resetStates() {
    // Disable connection
    setLabelColor(connectionStateLabel, ERROR_COLOR);
    gtk_label_set_text(connectionStateLabel, "Not established");

    // Reset checkMode
    setLabelColor(authentificationStateLabel, ERROR_COLOR);
    gtk_label_set_text(authentificationStateLabel, "Disabled");

    // Reset access
    setLabelColor(accessStateLabel, ERROR_COLOR);
    gtk_label_set_text(accessStateLabel, "Denied");

    // Reset UID
    gtk_label_set_text(currentUIDLabel, "None");
}

static void onRefreshedClicked() {
    struct sp_port **ports = serialListPorts();
    serialClose();
    resetStates();

    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(serialPortList));
    if (ports) {
        for (int i = 0; ports[i]; i++) {
            const char *name = sp_get_port_name(ports[i]);
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(serialPortList), name);
        }
        sp_free_port_list(ports);
    }
}

static void onConnectClicked(GtkButton *button, gpointer userData) {
    gint index = gtk_combo_box_get_active(GTK_COMBO_BOX(serialPortList));
    if (index < 0) {
        g_print("No port selected.\n");
        return;
    }

    const char *portName = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(serialPortList));
    if (serialOpen(portName) == 0) {
        setLabelColor(connectionStateLabel, SUCCESS_COLOR);
        gtk_label_set_text(connectionStateLabel, "Connected");
        
        g_timeout_add(2000, sendInitialCommand, NULL);
    } else {
        g_print("Failed to open %s\n", portName);
    }
}

static void appendOutput(const char *text) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(logTextField));
    GtkTextIter end;

    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, text, -1);

    GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(logFieldScroller));
    gtk_adjustment_set_value(adj, gtk_adjustment_get_upper(adj));
}

static void handleSerialCommand(char *text) {
    char* token = strtok(text, ".");
    if (strcmp(token, CHECK_MODE_CMD) == 0) {
        char *mode = strtok(NULL, ".");
        if (mode) {
            trim(mode);
            checkMode = strcmp(mode, CHECK_MODE_ON_CMD) == 0;
        }

        // Set label
        if (checkMode) {
            setLabelColor(authentificationStateLabel, SUCCESS_COLOR);
            gtk_label_set_text(authentificationStateLabel, "Active");
        } else {
            setLabelColor(authentificationStateLabel, ERROR_COLOR);
            gtk_label_set_text(authentificationStateLabel, "Disabled");
        }
    } else if (strcmp(token, AUTH_CMD) == 0) {
        char *response = strtok(NULL, ".");
        if (response && checkMode) {
            trim(response);
            if (strcmp(response, AUTH_SUCCESS_CMD) == 0 && uid_get_list(currentUIDValue) == UID_LIST_DATABASE) {
                setLabelColor(accessStateLabel, SUCCESS_COLOR);
                gtk_label_set_text(accessStateLabel, "Granted");
                return;
            }
            
            setLabelColor(accessStateLabel, ERROR_COLOR);
            gtk_label_set_text(accessStateLabel, "Denied");
        }

    } else if (strcmp(token, UID_CMD) == 0) {
        char *uidValue = strtok(NULL, ".");
        if (uidValue) {
            trim(uidValue);
            if (currentUIDValue) {
                g_free(currentUIDValue);
            }

            currentUIDValue = g_strdup(uidValue);

            gtk_label_set_text(currentUIDLabel, uidValue);
            addUIDToList(uidValue);

            if (checkMode)  {
                setLabelColor(accessStateLabel, WARNING_COLOR);
                gtk_label_set_text(accessStateLabel, "Authenticating...");
                return;
            }

            if (uid_get_list(uidValue) != UID_LIST_DATABASE) {
                setLabelColor(accessStateLabel, ERROR_COLOR);
                gtk_label_set_text(accessStateLabel, "Denied");
                return;
            }

            setLabelColor(accessStateLabel, SUCCESS_COLOR);
            gtk_label_set_text(accessStateLabel, "Granted");
        }
    }
}

static gboolean pollSerialInput(gpointer user_data) {
    char buf[128];
    int n = serialRead(buf, sizeof(buf));
    if (n > 0) {
        for (int i = 0; i < n; i++) {
            if (serialBufferLen < SERIAL_BUF_SIZE - 1) {
                serialBuffer[serialBufferLen++] = buf[i];
            }
            if (buf[i] == '\n') {
                serialBuffer[serialBufferLen - 1] = '\0';
                appendOutput(serialBuffer);
                handleSerialCommand(serialBuffer);
                serialBufferLen = 0;
            }
        }
    }

    return G_SOURCE_CONTINUE;
}

void guiActivate() {
    gtk_init(NULL, NULL);
    initUIDMap();

    GtkBuilder *builder = gtk_builder_new();
    if (!gtk_builder_add_from_string(builder, (const char*)rfid_ui_glade, rfid_ui_glade_len, NULL)) {
        g_error("Failed to load UI file");
    }

    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "mainWindow"));
    GtkWidget *refreshButton = GTK_WIDGET(gtk_builder_get_object(builder, "refreshButton"));
    GtkWidget *connectButton = GTK_WIDGET(gtk_builder_get_object(builder, "connectButton"));

    connectionStateLabel = GTK_LABEL(gtk_builder_get_object(builder, "connectionState"));
    authentificationStateLabel = GTK_LABEL(gtk_builder_get_object(builder, "authentificationState"));
    currentUIDLabel = GTK_LABEL(gtk_builder_get_object(builder, "currentUID"));
    accessStateLabel = GTK_LABEL(gtk_builder_get_object(builder, "accessState"));

    databaseUIDList = GTK_WIDGET(gtk_builder_get_object(builder, "databaseUIDList"));
    newUIDList = GTK_WIDGET(gtk_builder_get_object(builder, "newUIDList"));
    
    serialPortList = GTK_WIDGET(gtk_builder_get_object(builder, "serialPorts"));
    logTextField = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "logTextField"));
    logFieldScroller = GTK_SCROLLED_WINDOW(gtk_builder_get_object(builder, "logFieldScroller"));

    g_signal_connect(refreshButton, "clicked", G_CALLBACK(onRefreshedClicked), NULL);
    g_signal_connect(connectButton, "clicked", G_CALLBACK(onConnectClicked), NULL);

    onRefreshedClicked();
    gtk_widget_show_all(window);
    g_timeout_add(100, pollSerialInput, NULL);

    gtk_main();
}
