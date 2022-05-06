# run tests
``` bash
### KM
# Copy code to runbox
rsync -avIL -e "ssh -i ~/.ssh/id_rsa" --exclude-from="rsync-only-km.txt" que@192.168.1.12:Desktop/mymodule/* /home/que/Desktop/mymodule/
# Compile
make km kmup s
# kernel module:
sudo insmod intercept-module.ko
sudo ./user
# server: ~/Desktop/mymodule/helper$ 
./s km
# remove module with sudo rmmod intercept_module

### eBPF
# Compile on dev box and copy code to runbox: ~/Desktop/mymodule/eBPF_measure$
bash make_and_run.sh
# server: ~/Desktop/mymodule/helper$ 
./s ebpf

### stress test
stress --cpu 4

### Dev box:
./c

### Copy data to dev machine:
scp -r -i ~/.ssh/id_rsa /home/que/Desktop/mymodule/logs/*.txt que@192.168.1.12:Desktop/mymodule/logs/; sudo rm /home/que/Desktop/mymodule/logs/*.txt
# Run quick test on dev machine
python check_improved_plot.py
# after first run can use:
# sudo ./ebpf_measure_user -d ens33 --filename ebpf_measure_kern.o
```

# Plot
Test should be organized in `logs/test_suit/` as follow:
```bash
|-- test_suit
|   |-- 32k_nonstress_3pkpms_1024Bytes#1
|   |   |-- EBPF_kern.txt
|   |   |-- EBPF_socket_server.txt
|   |   |-- EBPF_user.txt
|   |   |-- KM_kern.txt
|   |   |-- KM_socket_server.txt
|   |   |-- KM_user.txt
|   |-- 32k_nonstress_3pkpms_1024Bytes#2
|   |   |-- ...
|   |-- 32k_nonstress_3pkpms_1024Bytes#3
|   |   |-- ...
|   |-- 32k_nonstress_3pkpms_1024Bytes#4
|   |   |-- ...
|   |-- 32k_nonstress_3pkpms_1024Bytes#5
|   |   |-- ...
|   |-- 32k_stress_3pkpms_1024Bytes#1
|   |   |-- ...
|   |-- 32k_stress_3pkpms_1024Bytes#2
|   |   |-- ...
|   |-- 32k_stress_3pkpms_1024Bytes#3
|   |   |-- ...
|   |-- 32k_stress_3pkpms_1024Bytes#4
|   |   |-- ...
|   |-- 32k_stress_3pkpms_1024Bytes#5
|   |   |-- ...
|   |-- 524k_nonstress_50pkpms_64Bytes#1
|   |   |-- ...
|   |-- 524k_nonstress_50pkpms_64Bytes#2
|   |   |-- ...
|   |-- 524k_nonstress_50pkpms_64Bytes#3
|   |   |-- ...
|   |-- 524k_nonstress_50pkpms_64Bytes#4
|   |   |-- ...
|   |-- 524k_nonstress_50pkpms_64Bytes#5
|   |   |-- ...
|   |-- 524k_stress_50pkpms_64Bytes#1
|   |   |-- ...
|   |-- 524k_stress_50pkpms_64Bytes#2
|   |   |-- ...
|   |-- 524k_stress_50pkpms_64Bytes#3
|   |   |-- ...
|   |-- 524k_stress_50pkpms_64Bytes#4
|   |   |-- ...
|   |-- 524k_stress_50pkpms_64Bytes#5
|   |   |-- ...
|   |-- 65k_nonstress_6pkpms_512Bytes#1
|   |   |-- ...
|   |-- 65k_nonstress_6pkpms_512Bytes#2
|   |   |-- ...
|   |-- 65k_nonstress_6pkpms_512Bytes#3
|   |   |-- ...
|   |-- 65k_nonstress_6pkpms_512Bytes#4
|   |   |-- ...
|   |-- 65k_nonstress_6pkpms_512Bytes#5
|   |   |-- ...
|   |-- 65k_stress_6pkpms_512Bytes#1
|   |   |-- ...
|   |-- 65k_stress_6pkpms_512Bytes#2
|   |   |-- ...
|   |-- 65k_stress_6pkpms_512Bytes#3
|   |   |-- ...
|   |-- 65k_stress_6pkpms_512Bytes#4
|   |   |-- ...
|   |-- 65k_stress_6pkpms_512Bytes#5
|       |-- ...
```
Run scripts in `scripts/`:

```bash
python3 -m venv venv
. venv/bin/activate
python3 ps_combi_2_stress_non.py   # process suit combi 2 (sub graphs) of stress and non-stress
python3 process_suit_combi_4.py  # combine all test, split into stress and non-stress
```



## Kernel module
```bash
# insert module
sudo insmod intercept-module.ko
# remove module
sudo rmmod intercept_module
# compile user app
gcc user-processing.c -o user-processing
# run user app:
sudo ./user-processing /proc/intercept_mmap
```

```bash
#
sudo dmesg -wH
# runbox commands
cd /home/que/Desktop/mymodule
# copy code from dev machine
scp -r -i ~/.ssh/id_rsa que@192.168.1.11:Desktop/mymodule/* /home/que/Desktop/mymodule/
## or
rsync -avIL -e "ssh -i ~/.ssh/id_rsa" --exclude-from="rsync-exclude.txt" que@192.168.1.12:Desktop/mymodule/* /home/que/Desktop/mymodule/
## or 
rsync -avIL -e "ssh -i ~/.ssh/id_rsa" --exclude-from="rsync-only-km.txt" que@192.168.1.12:Desktop/mymodule/* /home/que/Desktop/mymodule/
# Copy logs from run machine
scp -r -i ~/.ssh/id_rsa /home/que/Desktop/mymodule/logs/*.txt que@192.168.1.12:Desktop/mymodule/logs/
# Compile and run module
make clean && make
sudo rmmod intercept_module
sudo insmod intercept-module.ko
# Compile and run user space program
gcc -Wall user-processing.c -o user
sudo ./user

make sc; make kmupc; make kmc; make s && make kmup && make km

### Copy logs to dev machine
scp -r -i ~/.ssh/id_rsa /home/que/Desktop/mymodule/logs/*.txt que@192.168.1.11:Desktop/mymodule/logs/ 
```
----------------------------------

## Debug, utils, clean up ...
```bash
# Create link folder. This way, one scp command will copy our whole project to the runbox machine.
ln -s /home/que/Desktop/xdp-tutorial/XDP_measure /home/que/Desktop/mymodule/
ln -s /home/que/Desktop/xdp-tutorial/eBPF_measure /home/que/Desktop/mymodule/
```
```bash
du -h /var/log/
# Clear journal log
journalctl --disk-usage
sudo journalctl --vacuum-size=500M
sudo -- sh -c "cat /dev/null > /var/log/kern.log"
sudo rm /var/log/syslog.1
```

Change sublime text to default editor:
```bash
# https://askubuntu.com/a/397387
subl /usr/share/applications/defaults.list
# Search for all instances of gedit (org.gnome.gedit on some systems) 
# and replace them with sublime_text. 
# Save the file, log out and back in, and you should be all set.
```

Kill process using the port
```bash
sudo apt install net-tools
netstat -tulpn
kill <pid>
```

Kernel that we use is 5.8
```bash
sudo cp /etc/default/grub /etc/default/grub.bak
sudo -H gedit /etc/default/grub
#GRUB_DEFAULT=0 #change to the line below
GRUB_DEFAULT="Advanced options for Ubuntu>Ubuntu, with Linux 5.8.0-63-generic"
sudo update-grub
```
 
Processing log files with python
```bash
# linux
sudo apt install python3.8-venv
sudo apt install python3-pip
python3 -m venv venv
. venv/bin/activate
pip3 install matplotlib numpy 
# Successfully installed cycler-0.10.0 kiwisolver-1.3.1 matplotlib-3.4.3 
# numpy-1.21.2 pillow-8.3.1 pyparsing-2.4.7 python-dateutil-2.8.2 six-1.16.0
# IF "Matplotlib is currently using agg, which is a non-GUI backend, so cannot show the figure."
sudo apt install python3-tk
```

-----------------------------------------------------------------------


## XDP measure
```bash
# Compile (not work now. Use makefile)
#clang -O2 -Wall -target bpf -c xdp_measure_user.c -o xdp_measure_user.o
# Insert xdp kern program
sudo ip link set dev ens33 xdp obj xdp_measure_kern.o
# or 
ip -force link set dev ens33 xdp obj xdp_measure_kern.o
# force remove xdp program
sudo ip link set dev ens33 xdp off

sudo ./xdp_measure_user -d ens33 --filename xdp_measure_kern.o
```

## eBPF measure
```bash
sudo ./ebpf_measure_user -d ens33 --filename ebpf_measure_kern.o
# Insert xdp kern program
sudo ip link set dev ens33 xdp obj ebpf_measure_kern.o sec xdp_sock
# or 
ip -force link set dev ens33 xdp obj ebpf_measure_kern.o
# force remove xdp program
sudo ip link set dev ens33 xdp off

```