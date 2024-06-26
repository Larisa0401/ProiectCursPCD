#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <opencv2/opencv.hpp>
#include <fstream>
#include <vector>

#define SERVER_PORT 12349
#define MAX_BUFFER_SIZE 1024

void send_image(int client_socket, cv::Mat& image) {
    std::vector<uchar> buffer;
    cv::imencode(".jpg", image, buffer);
    int image_size = buffer.size();

    if (send(client_socket, &image_size, sizeof(int), 0) == -1) {
        perror("Eroare la trimiterea dimensiunii imaginii");
        return;
    }

    if (send(client_socket, buffer.data(), image_size, 0) == -1) {
        perror("Eroare la trimiterea imaginii");
        return;
    }

    printf("Imagine trimisă (%d bytes)\n", image_size);
}

void adjust_gamma(cv::Mat& image, double gamma, int client_socket) {
    unsigned char lut[256];
    for (int i = 0; i < 256; ++i) {
        lut[i] = cv::saturate_cast<unsigned char>(pow((i / 255.0), gamma) * 255.0);
    }

    cv::Mat adjusted_image = image.clone();
    cv::LUT(image, cv::Mat(1, 256, CV_8U, lut), adjusted_image);

    send_image(client_socket, adjusted_image);
}

void cropping_image(cv::Mat& image, int client_socket) {
    cv::Rect roi(20, 0, 100, 100);
    cv::Mat cropped_image = image(roi).clone();
    send_image(client_socket, cropped_image);
}


void sharpen_image(cv::Mat& image, int client_socket) {
    cv::Mat sharpened_image;
    cv::GaussianBlur(image, sharpened_image, cv::Size(0, 0), 3);
    cv::addWeighted(image, 1.5, sharpened_image, -0.5, 0, sharpened_image);
    send_image(client_socket, sharpened_image);
}

void process_image(const char *command, cv::Mat& image, int client_socket) {
    if (strcmp(command, "grayscale") == 0) {
        cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);
        send_image(client_socket, image);
    } else if (strcmp(command, "invert") == 0) {
        cv::bitwise_not(image, image);
        send_image(client_socket, image);
    } else if (strcmp(command, "cropping") == 0) {
        cropping_image(image, client_socket);
    } else if (strcmp(command, "gamma") == 0) {
        adjust_gamma(image, 1.5, client_socket);
    } else if (strcmp(command, "sharpening") == 0) {
        sharpen_image(image, client_socket);
    }else {
        printf("Comanda necunoscuta: %s\n", command);
    }
}

void block_user(const char *username, bool block) {
    std::ifstream inFile("Credentials.txt");
    if (!inFile) {
        perror("Eroare la deschiderea fisierului Credentials.txt");
        return;
    }

    std::ofstream outFile("temp.txt");
    if (!outFile) {
        perror("Eroare la crearea fisierului temporar");
        inFile.close();
        return;
    }

    std::string line;
    bool found = false;
    while (std::getline(inFile, line)) {
        std::string user, password;
        int status;
        std::istringstream iss(line);
        if (iss >> user >> password >> status) {
            if (user == username) {
                outFile << user << " " << password << " " << (block ? 1 : 0) << std::endl;
                found = true;
            } else {
                outFile << line << std::endl;
            }
        }
    }

    inFile.close();
    outFile.close();

    if (!found) {
        printf("Utilizatorul %s nu exista in fisierul Credentials.txt\n", username);
        remove("temp.txt");
    } else {
        remove("Credentials.txt");
        rename("temp.txt", "Credentials.txt");
        printf("Utilizatorul %s a fost %s.\n", username, block ? "blocat" : "deblocat");
    }
}

void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    free(arg);

    char buffer[MAX_BUFFER_SIZE];
    char image_path[MAX_BUFFER_SIZE];
    cv::Mat initial_image;

    // Determine if the connected user is a client or an admin
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        perror("Eroare la primirea tipului de utilizator");
        close(client_socket);
        return NULL;
    }
    buffer[bytes_received] = '\0';

    if (strcmp(buffer, "admin") == 0) {
        // Admin handling
        printf("Admin conectat.\n");
        while (1) {
            bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0) {
                perror("Eroare la primirea comenzii de la admin");
                close(client_socket);
                return NULL;
            }
            buffer[bytes_received] = '\0';

            if (strncmp(buffer, "block ", 6) == 0) {
                char username[MAX_BUFFER_SIZE];
                sscanf(buffer + 6, "%s", username);
                block_user(username, true);
            } else if (strncmp(buffer, "unblock ", 8) == 0) {
                char username[MAX_BUFFER_SIZE];
                sscanf(buffer + 8, "%s", username);
                block_user(username, false);
            } else if (strcmp(buffer, "disconnect") == 0) {
                printf("Admin deconectat.\n");
                close(client_socket);
                return NULL;
            } else {
                printf("Comanda necunoscuta pentru admin: %s\n", buffer);
            }
        }
    } else {
        // Client handling
        printf("Client conectat.\n");

        // Expect image path
        strcpy(image_path, buffer); // The first message is the image path

        initial_image = cv::imread(image_path, cv::IMREAD_COLOR);
        if (initial_image.empty()) {
            fprintf(stderr, "Eroare la citirea imaginii\n");
            close(client_socket);
            return NULL;
        }

        while (1) {
            bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0) {
                perror("Eroare la primirea comenzii");
                close(client_socket);
                return NULL;
            }
            buffer[bytes_received] = '\0';

            if (strcmp(buffer, "disconnect") == 0) {
                printf("Client deconectat.\n");
                close(client_socket);
                return NULL;
            }

            process_image(buffer, initial_image, client_socket);
        }
    }

    close(client_socket);
    return NULL;
}

int main() {
    int server_socket;
    struct sockaddr_in server_address;

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Eroare la crearea socketului");
        exit(EXIT_FAILURE);
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(SERVER_PORT);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Eroare la legarea adresei");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) == -1) {
        perror("Eroare la ascultarea conexiunilor");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Serverul ascultă pe portul %d\n", SERVER_PORT);

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_address_len = sizeof(client_address);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);
        if (client_socket == -1) {
            perror("Eroare la acceptarea conexiunii");
            continue;
        }

        printf("Client conectat: %s\n", inet_ntoa(client_address.sin_addr));

        pthread_t client_thread;
        int *client_sock_ptr = (int*)malloc(sizeof(int));
        *client_sock_ptr = client_socket;
        if (pthread_create(&client_thread, NULL, handle_client, client_sock_ptr) != 0) {
            perror("Eroare la crearea thread-ului pentru client");
            close(client_socket);
            free(client_sock_ptr);
        }
        pthread_detach(client_thread);
    }

    close(server_socket);
    return 0;
}

