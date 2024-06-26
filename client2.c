#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <opencv2/opencv.hpp>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 12349
#define MAX_BUFFER_SIZE 1024

// Declarați butoanele și variabilele necesare interfeței GTK
GtkWidget *connect_button;
GtkWidget *image_button;
GtkWidget *image_widget;
GtkWidget *grayscale_button;
GtkWidget *invert_button;
GtkWidget *detect_object_button;
GtkWidget *cropping_button;
GtkWidget *gamma_button;
GtkWidget *sharpening_button;
GtkWidget *disconnect_button;
GtkWidget *select_new_image;
// Declarația variabilei globale pentru fereastra principală
GtkWidget *window;


int client_socket_fd = -1; // Socketul clientului
char current_image_path[MAX_BUFFER_SIZE]; // Calea imaginii curente

// Funcția pentru a primi toți octeții disponibili
ssize_t recv_all(int socket, void *buffer, size_t length) {
    size_t total_received = 0;
    ssize_t bytes_received;
    while (total_received < length) {
        bytes_received = recv(socket, (char *)buffer + total_received, length - total_received, 0);
        if (bytes_received == -1) {
            perror("Eroare la primirea datelor");
            return -1;
        } else if (bytes_received == 0) {
            // Conexiunea s-a închis
            break;
        }
        total_received += bytes_received;
    }
    return total_received;
}

// Funcția pentru a trimite calea imaginii către server
void send_image_path(const char *path) {
    printf("Calea imaginii trimise: %s\n", path); // Mesaj de debug pentru verificarea căii
    size_t path_len = strlen(path);
    if (send(client_socket_fd, path, path_len + 1, 0) == -1) { // Trimite și terminatorul null '\0'
        perror("Eroare la trimiterea căii către server");
        exit(EXIT_FAILURE);
    }
}

// Funcția de conectare la server într-un fir de execuție separat
void *connect_to_server(void *arg) {
    struct sockaddr_in server_address;

    // Crearea unui socket TCP (IPv4)
    if ((client_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Eroare la crearea socketului");
        exit(EXIT_FAILURE);
    }

    // Inițializarea structurii pentru adresa serverului
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_address.sin_port = htons(SERVER_PORT);

    // Conectarea la server
    if (connect(client_socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Eroare la conectarea la server");
        exit(EXIT_FAILURE);
    }

    printf("Conectat la server %s:%d\n", SERVER_IP, SERVER_PORT);

    return NULL;
}

// Funcția de gestionare a apăsării butonului de conectare
void on_connect_button_clicked(GtkWidget *widget, gpointer data) {
    // Lansarea conectării în contextul GTK
    g_idle_add((GSourceFunc)connect_to_server, NULL);

    gtk_widget_hide(connect_button);
    gtk_widget_show(image_button);
}


// Funcția callback pentru alegerea unei noi imagini
void on_image_button_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    dialog = gtk_file_chooser_dialog_new("Alegeți o imagine",
                                         NULL,
                                         action,
                                         "_Cancel",
                                         GTK_RESPONSE_CANCEL,
                                         "_Open",
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename;
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        filename = gtk_file_chooser_get_filename(chooser);
        g_print("Selected file: %s\n", filename);

        strcpy(current_image_path, filename); // Actualizare cale curentă a imaginii

        gtk_image_set_from_file(GTK_IMAGE(image_widget), filename);
        send_image_path(filename);
        g_free(filename);

        gtk_widget_show_all(select_new_image);
        gtk_widget_hide(image_button);
    }

    gtk_widget_destroy(dialog);
}

// Funcția pentru a trimite o comandă la server
void send_command(const char *command) {
    // Trimite comanda la server
    if (send(client_socket_fd, command, strlen(command) + 1, 0) == -1) { // Trimite și terminatorul null '\0'
        perror("Eroare la trimiterea comenzii către server");
        exit(EXIT_FAILURE);
    }

    send_image_path(current_image_path);
    // Așteaptă să primească imaginea modificată de la server
    int image_size;
    if (recv(client_socket_fd, &image_size, sizeof(image_size), 0) == -1) {
        perror("Eroare la primirea dimensiunii imaginii");
        exit(EXIT_FAILURE);
    }

    // Alocă memorie pentru imagine
    char *image_data = (char *)malloc(image_size);
    if (!image_data) {
        perror("Eroare la alocarea memoriei pentru imagine");
        exit(EXIT_FAILURE);
    }

    // Primește imaginea modificată
    if (recv_all(client_socket_fd, image_data, image_size) != image_size) {
        perror("Eroare la primirea imaginii modificate");
        free(image_data);
        exit(EXIT_FAILURE);
    }

    // Salvează imaginea primită într-un fișier temporar
    char temp_image_path[] = "/tmp/received_image.jpg";
    FILE *file = fopen(temp_image_path, "wb");
    if (!file) {
        perror("Eroare la deschiderea fișierului temporar pentru imagine");
        free(image_data);
        exit(EXIT_FAILURE);
    }
    fwrite(image_data, 1, image_size, file);
    fclose(file);
    free(image_data);

    // Afișează imaginea modificată în widget-ul de imagine
    gtk_image_set_from_file(GTK_IMAGE(image_widget), temp_image_path);
}

// Funcțiile callback pentru butoanele de filtrare
void on_grayscale_button_clicked(GtkWidget *widget, gpointer data) {
    send_command("grayscale");
}

void on_invert_button_clicked(GtkWidget *widget, gpointer data) {
    send_command("invert");
}

void on_detect_object_button_clicked(GtkWidget *widget, gpointer data) {
    send_command("detect_object");
}

void on_cropping_button_clicked(GtkWidget *widget, gpointer data) {
    send_command("cropping");
}

void on_gamma_button_clicked(GtkWidget *widget, gpointer data) {
    send_command("gamma");
}

void on_sharpening_button_clicked(GtkWidget *widget, gpointer data) {
    send_command("sharpening");
}

void on_disconnect_button_clicked(GtkWidget *widget, gpointer data) {
    // Trimite o comandă de deconectare la server
    char disconnect_command[] = "disconnect";
    if (send(client_socket_fd, disconnect_command, strlen(disconnect_command) + 1, 0) == -1) { // Trimite și terminatorul null '\0'
        perror("Eroare la trimiterea comenzii de deconectare către server");
        exit(EXIT_FAILURE);
    }

    // Închide conexiunea socketului
    close(client_socket_fd);
    client_socket_fd = -1;
    printf("Deconectat de la server\n");

    // Închide bucla evenimentelor GTK și iese din aplicație
    gtk_main_quit();
}





// Funcția pentru crearea interfeței grafice GTK
void create_gui(int argc, char *argv[]) {
    // Inițializarea GTK
    gtk_init(&argc, &argv);
    // Crearea ferestrei principale
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    // Restul codului pentru crearea ferestrei rămâne neschimbat

    // Crearea ferestrei principale
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Conectare la server");
    gtk_container_set_border_width(GTK_CONTAINER(window), 20);
    gtk_widget_set_size_request(window, 800, 600); // Dimensiunile ferestrei
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Setarea poziției ferestrei pe mijlocul ecranului
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ALWAYS);

    // Crearea containerului pentru butoane și widget-ul de imagine
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), box);

    // Crearea butonului de conectare la server
    connect_button = gtk_button_new_with_label("Conectare la server");
    gtk_widget_set_size_request(connect_button, 150, 50); // Dimensiunile butonului
    gtk_box_pack_start(GTK_BOX(box), connect_button, FALSE, FALSE, 0); // Adăugarea butonului la container

    // Conectarea funcției callback la semnalul "clicked" al butonului de conectare
    g_signal_connect(connect_button, "clicked", G_CALLBACK(on_connect_button_clicked), NULL);

    // Crearea butonului de alegere a imaginii
    image_button = gtk_button_new_with_label("Alege imagine");
    gtk_widget_set_size_request(image_button, 150, 50); // Dimensiunile butonului
    gtk_box_pack_start(GTK_BOX(box), image_button, FALSE, FALSE, 0); // Adăugarea butonului la container
    gtk_widget_hide(image_button); // Ascundeți butonul de alegere a imaginii inițial

    // Conectarea funcției callback la semnalul "clicked" al butonului de alegere a imaginii
    g_signal_connect(image_button, "clicked", G_CALLBACK(on_image_button_clicked), NULL);

    // Crearea widget-ului de imagine
    image_widget = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(box), image_widget, TRUE, TRUE, 0); // Adăugarea widget-ului de imagine la container

    // Crearea butoanelor de filtrare și deconectare
    grayscale_button = gtk_button_new_with_label("Grayscale");
    invert_button = gtk_button_new_with_label("Invert");
    cropping_button = gtk_button_new_with_label("Cropping");
    gamma_button = gtk_button_new_with_label("Gamma");
    sharpening_button = gtk_button_new_with_label("Sharpening");
    disconnect_button = gtk_button_new_with_label("Deconectare de la server");

    // Adăugarea butoanelor la container
    gtk_box_pack_start(GTK_BOX(box), grayscale_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), invert_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), cropping_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), gamma_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), sharpening_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), disconnect_button, FALSE, FALSE, 0);

    // Ascunderea butoanelor de filtrare inițial
    gtk_widget_hide(grayscale_button);
    gtk_widget_hide(invert_button);
    gtk_widget_hide(cropping_button);
    gtk_widget_hide(gamma_button);
    gtk_widget_hide(sharpening_button);
    gtk_widget_hide(disconnect_button);

    // Conectarea funcțiilor callback la semnalul "clicked" al butoanelor de filtrare
    g_signal_connect(grayscale_button, "clicked", G_CALLBACK(on_grayscale_button_clicked), NULL);
    g_signal_connect(invert_button, "clicked", G_CALLBACK(on_invert_button_clicked), NULL);
    g_signal_connect(detect_object_button, "clicked", G_CALLBACK(on_detect_object_button_clicked), NULL);
    g_signal_connect(cropping_button, "clicked", G_CALLBACK(on_cropping_button_clicked), NULL);
    g_signal_connect(gamma_button, "clicked", G_CALLBACK(on_gamma_button_clicked), NULL);
    g_signal_connect(sharpening_button, "clicked", G_CALLBACK(on_sharpening_button_clicked), NULL);
    g_signal_connect(disconnect_button, "clicked", G_CALLBACK(on_disconnect_button_clicked), NULL);

    // Afișarea ferestrei
    gtk_widget_show_all(window);

    // Intrarea în bucla evenimentelor GTK
    gtk_main();
}

// Funcția principală
int main(int argc, char *argv[]) {
    create_gui(argc, argv);
    return 0;
}

