/**
 *  udp_tracked.cpp
 *  ----------------
 *  Реализация класса EthTrackedSocket:
 *   • создание/закрытие UDP-сокета
 *   • отправка команд гусеничной платформе
 *   • неблокирующий приём 128-байтных пакетов состояния
 *
 *  Автор: (ваше имя)      Дата: 22-05-2025
 *  ---------------------------------------------------------------------
 */

#include "ros2_control_t21_hardware/udp_tracked.hpp"
#include <iostream>
#include <unistd.h>

using namespace tracked_platform_udp;

// ─────────── создание сокета ───────────
EthTrackedSocket::EthTrackedSocket(const char *listen_ip,
                                   uint16_t     listen_prt,
                                   const char *remote_ip,
                                   uint16_t     remote_prt)
{
  sock_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock_ == -1) { perror("socket"); return; }

  // приём
  cliaddr_.sin_family      = AF_INET;
  cliaddr_.sin_port        = htons(listen_prt);
  cliaddr_.sin_addr.s_addr = inet_addr(listen_ip);
  if (bind(sock_, reinterpret_cast<sockaddr*>(&cliaddr_),
           sizeof(cliaddr_)) < 0) {
    perror("bind"); close(sock_); sock_ = -1; return;
  }

  // отправка
  servaddr_.sin_family      = AF_INET;
  servaddr_.sin_port        = htons(remote_prt);
  servaddr_.sin_addr.s_addr = inet_addr(remote_ip);

  // таймаут приёма 10 мс
  timeval tv{0, 10000};
  setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  std::cout << "UDP socket ready\n";
}

// ─────────── деструктор ───────────
EthTrackedSocket::~EthTrackedSocket()
{
  if (sock_ != -1 && close(sock_) == -1) perror("close");
}

// ─────────── отправка команды ───────────
void EthTrackedSocket::sendCommand(float lin_vel, float ang_vel,
                                   float geom,    bool geom_pos_mode)
{
  if (sock_ == -1) return;

  Packet128 pkt{};                  // все поля нуль-инициализированы
  pkt.geoMode = geom_pos_mode ? 0x01 : 0x00;
  pkt.linVel  = lin_vel;
  pkt.angVel  = ang_vel;
  pkt.geomPos = geom;

  ssize_t n = sendto(sock_, &pkt, sizeof(pkt), MSG_CONFIRM,
                     reinterpret_cast<sockaddr*>(&servaddr_),
                     sizeof(servaddr_));
  if (n < 0) perror("sendto");
}

// ─────────── приём состояния ───────────
bool EthTrackedSocket::receiveState(Packet128 &state)
{
  if (sock_ == -1) return false;
  ssize_t n = recvfrom(sock_, &state, sizeof(state), MSG_WAITALL,
                       nullptr, nullptr);
  return n == sizeof(state);
}
