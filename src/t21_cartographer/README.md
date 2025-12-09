# ** Cartographer**
# Оглавление

- [Цель](#цель)
- [Структура проекта](#структура-проекта)
- [Зависимости](#зависимости)
- [Настройка](#настройка)
- [Сохранение карты](#сохранение-карты)
- [TODO](#TODO)

---

## Цель

Предназначен для запуска пакета 2D- и 2.5D-картографирования и локализации по данным лазерного дальномера и IMU. Поддерживает 2 режима работы, принимая на вход данные как /LaserScan, так и /PointCloud.

---

#  [Структура проекта](#оглавление)

```bash
src/t21_cartographer/
├── CMakeLists.txt
├── config
│   └── cartographer.lua # Параметры
├── launch
│   └── cartographer.launch.py # Запуск Cartographer
├── package.xml
└── README.md# # <Вы находитесь здесь>
```

---

# [Настройка](#оглавление)
Раздел будет дополнен.

Код взят из следующих статей (ссылки на репозитории в них):
- https://ouster.com/insights/blog/building-maps-using-google-cartographer-and-the-os1-lidar-sensor

- https://www.waveshare.com/wiki/Cartographer_Map_Building
---
# [Сохранение карты](#оглавление)
Для сохранения карты необходимо вызвать сервис:
```
  ros2 service call /write_state ./maps/ma.pbstream
```

---
# [Зависимости](#оглавление)
Будет дополнено

---

# [TODO](#оглавление)
Дописать данный файл

---
