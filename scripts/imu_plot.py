import pandas as pd
import matplotlib.pyplot as plt
import os

def plot_imu_data(file_path):
    # Проверка наличия файла
    if not os.path.exists(file_path):
        print(f"Файл {file_path} не найден!")
        return

    # Чтение данных с помощью pandas
    df = pd.read_csv(file_path)

    # Создаем сетку графиков 2x2 на основе вашего шаблона
    fig, ax = plt.subplots(2, 2, figsize=(12, 10))
    fig.suptitle('Анализ данных IMU из CSV', fontsize=16)

    # 1. Линейное ускорение (Верхний левый)
    ax[0, 0].set_title('Линейное ускорение')
    ax[0, 0].set_xlabel('Время (с)')
    ax[0, 0].set_ylabel('Ускорение (м/с²)')
    ax[0, 0].plot(df['time_data'], df['linear_acceleration_x'], label='X', color='r')
    ax[0, 0].plot(df['time_data'], df['linear_acceleration_y'], label='Y', color='g')
    ax[0, 0].plot(df['time_data'], df['linear_acceleration_z'], label='Z', color='b')
    ax[0, 0].legend()
    ax[0, 0].grid(True)

    # 2. Угловая скорость (Нижний левый)
    ax[1, 0].set_title('Угловая скорость')
    ax[1, 0].set_xlabel('Время (с)')
    ax[1, 0].set_ylabel('Скорость (рад/с)') # Исправил единицы измерения
    ax[1, 0].plot(df['time_data'], df['angular_velocity_x'], label='Roll (X)', color='r')
    ax[1, 0].plot(df['time_data'], df['angular_velocity_y'], label='Pitch (Y)', color='g')
    ax[1, 0].plot(df['time_data'], df['angular_velocity_z'], label='Yaw (Z)', color='b')
    ax[1, 0].legend()
    ax[1, 0].grid(True)

    # 3. Ориентация (Верхний правый) - Заглушка, так как в CSV этого нет
    ax[0, 1].set_title('Ориентация (Данные отсутствуют)')
    ax[0, 1].set_xlabel('Время (с)')
    ax[0, 1].text(0.5, 0.5, 'Нет данных в CSV', ha='center')
    ax[0, 1].grid(True)

    # 4. Положение (Нижний правый) - Заглушка
    ax[1, 1].set_title('Положение (Данные отсутствуют)')
    ax[1, 1].set_xlabel('Время (с)')
    ax[1, 1].text(0.5, 0.5, 'Нет данных в CSV', ha='center')
    ax[1, 1].grid(True)

    # Автоматическая компоновка, чтобы графики не наезжали друг на друга
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.show()

if __name__ == '__main__':
    # Укажите путь к вашему файлу
    PATH_TO_CSV = './imu_data/imu_data.csv'
    plot_imu_data(PATH_TO_CSV)