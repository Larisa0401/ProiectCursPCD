#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_USERNAME_LENGTH 50
#define MAX_PASSWORD_LENGTH 50

struct Credential {
    char username[MAX_USERNAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
    int status;  // Adăugăm un câmp pentru statusul contului (0 - deblocat, 1 - blocat, de exemplu)
};

void showErrorMessage(GtkWidget *parent, const gchar *message) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                               GTK_DIALOG_MODAL,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_OK,
                                               "%s", message);
    gtk_widget_show_all(dialog);
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
}

void showSuccessMessage(GtkWidget *parent, const gchar *message) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                               GTK_DIALOG_MODAL,
                                               GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_OK,
                                               "%s", message);
    gtk_widget_show_all(dialog);
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
}

void registerUser(GtkWidget *widget, gpointer data) {
    struct Credential user;
    GtkWidget **entries = (GtkWidget **)data;
    GtkEntry *username_entry = GTK_ENTRY(entries[0]);
    const gchar *username = gtk_entry_get_text(username_entry);
    strcpy(user.username, username);

    GtkEntry *password_entry = GTK_ENTRY(entries[1]);
    const gchar *password = gtk_entry_get_text(password_entry);
    strcpy(user.password, password);

    // Adăugăm utilizatorul în fișierul de credențiale
    FILE *file = fopen("Credentials.txt", "a+");
    if (file == NULL) {
        printf("Eroare la deschiderea fișierului.\n");
        return;
    }

    // Verificăm dacă numele de utilizator există deja
    char line[MAX_USERNAME_LENGTH + MAX_PASSWORD_LENGTH + 2];
    while (fgets(line, sizeof(line), file)) {
        char *stored_username = strtok(line, " ");
        if (strcmp(username, stored_username) == 0) {
            printf("Numele de utilizator există deja.\n");
            fclose(file);
            showErrorMessage(GTK_WIDGET(gtk_widget_get_toplevel(GTK_WIDGET(widget))), "Numele de utilizator există deja.");
            return;
        }
    }

    // Adăugăm noul utilizator la sfârșitul fișierului cu statusul inițial 0 (deblocat)
    fprintf(file, "%s %s 0\n", user.username, user.password);
    fclose(file);

    printf("Utilizator înregistrat cu succes.\n");
    showSuccessMessage(GTK_WIDGET(gtk_widget_get_toplevel(GTK_WIDGET(widget))), "Utilizator înregistrat cu succes.");
}

void authenticateUser(GtkWidget *widget, gpointer data) {
    GtkWidget **entries = (GtkWidget **)data;
    GtkEntry *username_entry = GTK_ENTRY(entries[0]);
    const gchar *username = gtk_entry_get_text(username_entry);

    GtkEntry *password_entry = GTK_ENTRY(entries[1]);
    const gchar *password = gtk_entry_get_text(password_entry);

    struct Credential user;
    FILE *file = fopen("Credentials.txt", "r");
    if (file == NULL) {
        printf("Eroare la deschiderea fișierului.\n");
        return;
    }
    while (fscanf(file, "%s %s %d", user.username, user.password, &user.status) != EOF) {
        if (strcmp(username, user.username) == 0 && strcmp(password, user.password) == 0) {
            if (user.status == 1) {
                printf("Contul este blocat.\n");
                fclose(file);
                showErrorMessage(GTK_WIDGET(gtk_widget_get_toplevel(GTK_WIDGET(widget))), "Contul este blocat.");
                return;
            }
            printf("Autentificare reusita.\n");
            fclose(file);

            system("./client2");
            // Distrugem fereastra principală după autentificarea reușită
            gtk_widget_destroy(GTK_WIDGET(gtk_widget_get_toplevel(GTK_WIDGET(widget))));
            
            // Închidem ciclul GTK main pentru a încheia aplicația corect
            gtk_main_quit();
            
            return;
        }
    }
    printf("Numele de utilizator sau parola incorecte.\n");
    fclose(file);
    showErrorMessage(GTK_WIDGET(gtk_widget_get_toplevel(GTK_WIDGET(widget))), "Numele de utilizator sau parolă incorecte.");
}

int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *grid;
    GtkWidget *label;
    GtkWidget *button;
    GtkWidget *entry_username;
    GtkWidget *entry_password;
    GtkWidget *entries[2];

    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Autentificare");
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);

    // Centrare fereastra pe mijlocul ecranului
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    gtk_container_add(GTK_CONTAINER(window), grid);

    label = gtk_label_new("Nume de utilizator:");
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);

    entry_username = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), entry_username, 1, 0, 1, 1);
    entries[0] = entry_username;

    label = gtk_label_new("Parola:");
    gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);

    entry_password = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(entry_password), FALSE);
    gtk_grid_attach(GTK_GRID(grid), entry_password, 1, 1, 1, 1);
    entries[1] = entry_password;

    button = gtk_button_new_with_label("Inregistrare");
    g_signal_connect(button, "clicked", G_CALLBACK(registerUser), entries);
    gtk_grid_attach(GTK_GRID(grid), button, 0, 2, 2, 1);

    button = gtk_button_new_with_label("Autentificare");
    g_signal_connect(button, "clicked", G_CALLBACK(authenticateUser), entries);
    gtk_grid_attach(GTK_GRID(grid), button, 0, 3, 2, 1);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(window);

    gtk_main();

    return 0;
}

