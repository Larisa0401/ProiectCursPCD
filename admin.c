#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_USERS 100
#define MAX_USER_LENGTH 50
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 12349
#define MAX_BUFFER_SIZE 1024

GtkWidget *window;
GtkWidget *button_save;
GtkWidget *button_disconnect;
GtkWidget *scrolled_window;
GtkWidget *check_buttons[MAX_USERS];
int server_socket;

typedef struct {
    char user[MAX_USER_LENGTH];
    char password[MAX_USER_LENGTH];
    int status; // 0 for not blocked, 1 for blocked
} User;

User users[MAX_USERS];
int user_count = 0;

void send_block_unblock_command(const char *command) {
    if (send(server_socket, command, strlen(command), 0) == -1) {
        perror("Error sending block/unblock command");
    } else {
        printf("Command sent: %s\n", command);
    }
}

void on_button_save_clicked(GtkWidget *widget, gpointer data) {
    // Update user statuses based on checkboxes
    for (int i = 0; i < user_count; ++i) {
        int new_status = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_buttons[i])) ? 1 : 0;
        if (users[i].status != new_status) {
            users[i].status = new_status;
            char command[MAX_BUFFER_SIZE];
            snprintf(command, sizeof(command), "%s %s", new_status ? "block" : "unblock", users[i].user);
            send_block_unblock_command(command);
        }
    }

    // Save updated statuses to file
    FILE *file = fopen("Credentials.txt", "w");
    if (file == NULL) {
        perror("Cannot open file for writing");
        return;
    }

    for (int i = 0; i < user_count; ++i) {
        fprintf(file, "%s %s %d\n", users[i].user, users[i].password, users[i].status);
    }

    fclose(file);
    printf("User statuses saved successfully\n");
}

void on_button_disconnect_clicked(GtkWidget *widget, gpointer data) {
    // Send disconnect command to server
    char disconnect_command[] = "disconnect";
    if (send(server_socket, disconnect_command, strlen(disconnect_command), 0) == -1) {
        perror("Error sending disconnect command");
    }

    // Close the socket connection
    if (close(server_socket) == -1) {
        perror("Error closing socket");
    } else {
        printf("Disconnected from server\n");
    }

    gtk_main_quit(); // Exit the GTK main loop
}

void setup_gui(int argc, char *argv[]) {
    // Initialize GTK
    gtk_init(&argc, &argv);

    // Create the main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Admin Panel");
    gtk_window_set_default_size(GTK_WINDOW(window), 300, 400);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Create scrolled window
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(window), scrolled_window);

    // Create grid
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_add(GTK_CONTAINER(scrolled_window), grid);

    // Add headers
    GtkWidget *label_user = gtk_label_new("User");
    gtk_grid_attach(GTK_GRID(grid), label_user, 0, 0, 1, 1);

    GtkWidget *label_status = gtk_label_new("Status");
    gtk_grid_attach(GTK_GRID(grid), label_status, 1, 0, 1, 1);

    // Create checkboxes for users
    for (int i = 0; i < user_count; ++i) {
        GtkWidget *label = gtk_label_new(users[i].user);
        gtk_grid_attach(GTK_GRID(grid), label, 0, i + 1, 1, 1);

        check_buttons[i] = gtk_check_button_new();
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_buttons[i]), users[i].status == 1);
        gtk_grid_attach(GTK_GRID(grid), check_buttons[i], 1, i + 1, 1, 1);
    }

    // Create save button
    button_save = gtk_button_new_with_label("Salvează");
    g_signal_connect(button_save, "clicked", G_CALLBACK(on_button_save_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), button_save, 0, user_count + 1, 1, 1);

    // Create disconnect button
    button_disconnect = gtk_button_new_with_label("Deconectează-te");
    g_signal_connect(button_disconnect, "clicked", G_CALLBACK(on_button_disconnect_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), button_disconnect, 1, user_count + 1, 1, 1);

    // Show all widgets
    gtk_widget_show_all(window);
}

int main(int argc, char *argv[]) {
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating socket");
        return EXIT_FAILURE;
    }

    // Setup server address
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return EXIT_FAILURE;
    }

    // Connect to server
    if (connect(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        return EXIT_FAILURE;
    }
    printf("Connected to server\n");

    // Send initial message indicating admin connection
    char admin_msg[] = "admin";
    if (send(server_socket, admin_msg, strlen(admin_msg), 0) == -1) {
        perror("Error sending admin message");
        return EXIT_FAILURE;
    }

    // Read users from file
    FILE *file = fopen("Credentials.txt", "r");
    if (file == NULL) {
        perror("Cannot open file");
        return EXIT_FAILURE;
    }

    while (fscanf(file, "%s %s %d", users[user_count].user, users[user_count].password, &users[user_count].status) != EOF) {
        user_count++;
        if (user_count >= MAX_USERS) {
            break;
        }
    }
    fclose(file);

    setup_gui(argc, argv);
    gtk_main();
    return 0;
}

