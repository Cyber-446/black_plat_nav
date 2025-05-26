#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Joy
from std_msgs.msg import Float64MultiArray

class JoyFullTeleop(Node):
    def __init__(self):
        super().__init__('joy_full_teleop')
        # параметры осей
        self.declare_parameter('axis_lin', 1)        # линейная скорость (левый стик вертикаль)
        self.declare_parameter('axis_ang', 0)        # угловая скорость (левый стик горизонталь)
        self.declare_parameter('axis_flipper', 4)    # флипер (правый триггер)
        # параметры масштабирования
        self.declare_parameter('scale_lin', 0.5)     # м/с
        self.declare_parameter('scale_ang', 1.0)     # рад/с
        self.declare_parameter('scale_flipper', 0.7) # рад
        # параметры мертвой зоны
        self.declare_parameter('deadzone', 0.05)

        p = self.get_parameter
        self.axis_lin       = p('axis_lin').value
        self.axis_ang       = p('axis_ang').value
        self.axis_flipper   = p('axis_flipper').value
        self.scale_lin      = p('scale_lin').value
        self.scale_ang      = p('scale_ang').value
        self.scale_flipper  = p('scale_flipper').value
        self.deadzone       = p('deadzone').value

        # паблишеры
        self.pub_lin = self.create_publisher(
            Float64MultiArray, '/lin_vel_controller/commands', 10
        )
        self.pub_ang = self.create_publisher(
            Float64MultiArray, '/ang_vel_controller/commands', 10
        )
        self.pub_flip = self.create_publisher(
            Float64MultiArray, '/geom_position_controller/commands', 10
        )

        self.create_subscription(Joy, '/joy', self.cb_joy, 10)

    def cb_joy(self, joy: Joy):
        # читаем оси
        v = joy.axes[self.axis_lin] if self.axis_lin < len(joy.axes) else 0.0
        w = joy.axes[self.axis_ang] if self.axis_ang < len(joy.axes) else 0.0
        f = joy.axes[self.axis_flipper] if self.axis_flipper < len(joy.axes) else 0.0
        # мертвая зона
        v = 0.0 if abs(v) < self.deadzone else v
        w = 0.0 if abs(w) < self.deadzone else w
        f = 0.0 if abs(f) < self.deadzone else f
        # масштабирование
        v_cmd = v * self.scale_lin
        w_cmd = w * self.scale_ang
        f_cmd = f * self.scale_flipper
        # публикуем
        msg_v = Float64MultiArray(data=[v_cmd])
        msg_w = Float64MultiArray(data=[w_cmd])
        msg_f = Float64MultiArray(data=[f_cmd])
        self.pub_lin.publish(msg_v)
        self.pub_ang.publish(msg_w)
        self.pub_flip.publish(msg_f)


def main():
    rclpy.init()
    node = JoyFullTeleop()
    rclpy.spin(node)
    rclpy.shutdown()

if __name__ == '__main__':
    main()
