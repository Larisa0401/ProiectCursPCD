# ProiectCursPCD

Pentru funcționarea codului client, este necesar să instalați următoarele biblioteci:

* `sudo apt install g++`
* `sudo apt-get install libgtk-3-dev libopencv-dev`
* `sudo apt-get install libcurl4-openssl-dev`

Pentru funcționarea codului server, este necesar să instalați următoarele biblioteci:

* wget https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
 
Pentru funcționarea codului interfata, este necesar să instalați următoarele biblioteci:

* sudo apt-get install libgtk-3-dev

Compilare:
* client: g++ client2.c -o client2 `pkg-config --cflags --libs gtk+-3.0 opencv4` -lcurl
* server: g++ servergui.c -o servergui `pkg-config --cflags --libs opencv4`
* admin: gcc -o admin admin.c `pkg-config --cflags --libs gtk+-3.0`
* interfata: g++ interfata.c -o interfata `pkg-config --cflags --libs gtk+-3.0` `pkg-config --cflags --libs opencv4`

Rulare:
* ./client2
* ./servergui
* ./admin
* ./interfata

Va fi nevoie doar de trei terminale in care se vor rula interfata, serverul si adminul. Din interfata daca sunt introduse datele corecte de conectare se va deschide automat clientul care va putea face modificari pe imagini.

TBA client.html
