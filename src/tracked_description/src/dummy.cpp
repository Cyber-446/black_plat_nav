/**
 *  t21_udp_echo_server.cpp
 *  ---------------------------------------------------------------
 *  Эмулятор Т-21: простой UDP-эхо-сервер.
 *
 *  Сборка:
 *    g++ -std=c++17 -O2 t21_udp_echo_server.cpp -o t21_udp_echo_server
 *
 *  Запуск:
 *    ./t21_udp_echo_server            # порт 8888
 *    ./t21_udp_echo_server 9000       # свой порт
 *  ---------------------------------------------------------------
 */
#include <arpa/inet.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

int main(int argc, char* argv[])
{
  /* ─── выбираем порт ─────────────────────────────────────────── */
  int port = 8888;
  if (argc > 1) port = std::stoi(argv[1]);

  /* ─── создаём UDP-сокет ─────────────────────────────────────── */
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket");
    return 1;
  }

  sockaddr_in srv{};
  srv.sin_family = AF_INET;
  srv.sin_addr.s_addr = INADDR_ANY;
  srv.sin_port = htons(static_cast<uint16_t>(port));

  if (bind(sock, reinterpret_cast<sockaddr*>(&srv), sizeof(srv)) < 0) {
    perror("bind");
    return 1;
  }

  std::cout << "T21 UDP echo-server listening on *:" << port << '\n';

  /* ─── цикл: приём + эхо ─────────────────────────────────────── */
  char buf[2048];
  while (true)
  {
    sockaddr_in cli{};
    socklen_t   clen = sizeof(cli);

    ssize_t n = recvfrom(sock, buf, sizeof(buf), 0,
                         reinterpret_cast<sockaddr*>(&cli), &clen);
    if (n < 0) { perror("recvfrom"); continue; }

    std::cout << "RX " << n << " bytes from "
              << inet_ntoa(cli.sin_addr) << ':'
              << ntohs(cli.sin_port) << std::endl;

    /* отправляем тот же пакет обратно */
    sendto(sock, buf, static_cast<size_t>(n), 0,
           reinterpret_cast<sockaddr*>(&cli), clen);
  }
}
