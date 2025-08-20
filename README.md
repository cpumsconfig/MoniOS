# MoniOS怎么编译
### 1.下载源码
### 2.下载gcc、nasm、ft工具、i686-gcc(当然，ft工具没发，可以使用其他工具，要求是可以使用fat16)、qemu
### 3.安装gcc、nasm、qemu、i686-gcc
### 4.输入make clean清理
### 5.输入make run编译并运行
### 6.如果需要vmdk文件请先输入make clean后输入make vmdk
# MoniOS支持的指令
### 文件系统类：cd、ls、cat、mkdir、rm
### 常用类：echo、clear、shutdown、help、ver
### 网络类(由于有一些bug，所以ping将会是S)ping、netinit
### 测试专用类：demo(大家也根据execute.c文件随便改，这里是显示蓝色背景的测试)
# MoniOS支持的文件系统
### fat16、fat32
# MoniOS支持的声卡
### sb16(目前在测试中)


# How to compile MoniOS
### 1. Download the source code
### 2. Download gcc, nasm, ft tool, i686 gcc (of course, ft tool has not been released, other tools can be used, the requirement is to be able to use fat16), qemu
### 3. Install gcc, nasm, qemu, i686 gcc
### 4. Enter 'make clean' to clean up
### 5. Enter make run to compile and run
### 6. If you need a vmdk file, please enter make clean first and then enter make vmdk
# MoniOS supported commands
### File system classes: cd, ls, cat, mkdir, rm
### Common classes: echo, clear, shutdown, help, ver
### Network class (due to some bugs, ping will be S) ping, netinit
### Test specific class: demo (everyone can also modify it according to the execute. c file, here is the test with a blue background)
# MoniOS supported file systems
### fat16、fat32
# MoniOS supported sound cards
### Sb16 (currently under testing)