使用方法，将该dts复制到设备树中，按照如下规则添加希望控制的GPIO属性标签

jx-gpio {
    compatible = "jx,jx-gpio";
    power-gpio = <&ap_gpio 146 0>;
    reset-gpio = <&ap_gpio 149 0>;
    //添加自己想提供给上层控制的GPIO口
    xxxx-gpio = <&ap_gpio xxx 0>;
    yyyy-gpio = <&ap_gpio yyy 0>;
    zzzz-gpio = <&ap_gpio zzz 0>;
};

文件节点讲解

关于用户空间，读写对应的GPIO
1、通过该驱动会自动生成如下节点，同时可以具备读写功能
sys/gpios/power-gpio
sys/gpios/reset-gpio
sys/gpios/xxxx-gpio
sys/gpios/yyyy-gpio
sys/gpios/zzzz-gpio

2、还会生成一个gpio_list节点，功能是展示该驱动注册的所有gpio


3、关于内核空间，读写对应的GPIO
可通过：ret = jx_gpio_read("dts配置的标签名称"); // ret的值为0或者1 ，就是对应IO的电平 ，如果为负数，表示标签错误或者GPIO没注册上
ret = jx_gpio_set("dts配置的标签名称",value)  // ret 0 表示传的标签参数正确，且注册了GPIO，如果为负数，表示标签错误或者GPIO没注册上


3、功能展示

130|ums9620_2h10:/ # cat /sys/gpios/gpio_list
(reset-gpio->197:1)
(power-gpio->194:1)
ums9620_2h10:/ # cat /sys/gpios/power-gpio
1
ums9620_2h10:/ # cat /sys/gpios/reset-gpio
1
ums9620_2h10:/ # echo 0 > /sys/gpios/power-gpio
ums9620_2h10:/ # echo 0 > /sys/gpios/reset-gpio
ums9620_2h10:/ # cat /sys/gpios/gpio_list
(reset-gpio->197:0)
(power-gpio->194:0)
ums9620_2h10:/ # cat /sys/gpios/power-gpio
0
ums9620_2h10:/ # cat /sys/gpios/reset-gpio
0
ums9620_2h10:/ #


