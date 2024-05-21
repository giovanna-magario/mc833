Para rodar este projeto, seguir as seguintes instruções:
Para compilar o servidor (server.c): g++ server.c -o server vcpkg/installed/x64-linux/debug/lib/libcjson.a -I vcpkg/installed/x64-linux/include
Para compilar o cliente (client.c): gcc -o client client.c
Para rodar o servidor: ./server
Para rodar o cliente: ./client <nome da máquina em que se localiza o servidor>
Obs: Se necessário, instalar a lib CJSON via VCPKG, seguindo o link: https://github.com/DaveGamble/cJSON